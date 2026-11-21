// SPDX-License-Identifier: GPL-3.0-or-later

#include "player_study_view_model.h"

#include "services/player/mining_history_store.h"
#include "services/sasayaki/srt_parser.h"

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <tuple>

namespace niratan::viewmodels {
namespace {

using player::MiningHistoryItem;

constexpr double kFoundationEpoch = 978307200.0;
constexpr int kWindowSize = 80;
constexpr int kWindowExtension = 40;

struct Cue {
    double start;
    double end;
    QString text;
};

QString subtitle_format(const QUrl& file) {
    const QString suffix = file.fileName().section(QLatin1Char('.'), -1).toLower();
    if (suffix == QStringLiteral("vtt")) {
        return QStringLiteral("webVTT");
    }
    if (suffix == QStringLiteral("srt") || suffix == QStringLiteral("ass") ||
        suffix == QStringLiteral("ssa")) {
        return suffix;
    }
    return {};
}

double ass_timestamp(const QString& token) {
    const QStringList parts = token.trimmed().split(QLatin1Char(':'));
    if (parts.size() != 3) {
        return -1.0;
    }
    bool valid_hours = false;
    bool valid_minutes = false;
    bool valid_seconds = false;
    const int hours = parts[0].toInt(&valid_hours);
    const int minutes = parts[1].toInt(&valid_minutes);
    const double seconds = parts[2].toDouble(&valid_seconds);
    if (!valid_hours || !valid_minutes || !valid_seconds || hours < 0 || minutes < 0 ||
        minutes >= 60 || !std::isfinite(seconds) || seconds < 0 || seconds >= 60) {
        return -1.0;
    }
    return hours * 3600.0 + minutes * 60.0 + seconds;
}

QList<Cue> parse_ass(const QString& text) {
    static const QRegularExpression overrides(QStringLiteral(R"(\{[^}]*\})"));
    static const QRegularExpression drawing(QStringLiteral(R"(\{[^}]*\\p[1-9]\d*[^}]*\})"));
    QList<Cue> cues;
    bool events = false;
    int fields = 10;
    int start_field = 1;
    int end_field = 2;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('['))) {
            events = trimmed.compare(QStringLiteral("[Events]"), Qt::CaseInsensitive) == 0;
        } else if (events && trimmed.startsWith(QStringLiteral("Format:"), Qt::CaseInsensitive)) {
            QStringList names = trimmed.mid(7).toLower().split(QLatin1Char(','));
            for (QString& name : names) {
                name = name.trimmed();
            }
            fields = int(names.size());
            start_field = names.indexOf(QStringLiteral("start"));
            end_field = names.indexOf(QStringLiteral("end"));
            if (names.last() != QStringLiteral("text") ||
                start_field == fields - 1 || end_field == fields - 1) {
                start_field = end_field = -1;
            }
        } else if (events && start_field >= 0 && end_field >= 0 &&
                   trimmed.startsWith(QStringLiteral("Dialogue:"), Qt::CaseInsensitive)) {
            const QString body = trimmed.mid(9);
            QStringList values;
            qsizetype offset = 0;
            while (values.size() < fields - 1) {
                const qsizetype comma = body.indexOf(QLatin1Char(','), offset);
                if (comma < 0) {
                    break;
                }
                values.append(body.mid(offset, comma - offset));
                offset = comma + 1;
            }
            if (values.size() != fields - 1) {
                continue;
            }
            const double start = ass_timestamp(values[start_field]);
            const double end = ass_timestamp(values[end_field]);
            QString cue = body.mid(offset);
            if (start < 0.0 || end < start || cue.contains(drawing)) {
                continue;
            }
            cue.remove(overrides)
                .replace(QStringLiteral("\\N"), QStringLiteral("\n"))
                .replace(QStringLiteral("\\n"), QStringLiteral("\n"))
                .replace(QStringLiteral("\\h"), QStringLiteral(" "));
            if (!cue.trimmed().isEmpty()) {
                cues.append({start, end, cue.trimmed()});
            }
        }
    }
    return cues;
}

QList<Cue> parse_document(const QByteArray& bytes, const QString& format) {
    if (format == QStringLiteral("ass") || format == QStringLiteral("ssa")) {
        return parse_ass(QString::fromUtf8(bytes).remove(QChar(0xfeff)));
    }
    QString text = QString::fromUtf8(bytes);
    const bool vtt = format == QStringLiteral("webVTT");
    if (vtt) {
        // The shared parser accepts hour-qualified timecodes. Expand only
        // the short WebVTT timecodes on timing lines, not subtitle text.
        static const QRegularExpression timing(
            QStringLiteral(R"(^[ \t]*(\d{2,}:\d{2}(?::\d{2})?\.\d{3})([ \t]+-->[ \t]+)(\d{2,}:\d{2}(?::\d{2})?\.\d{3})(?=[ \t\r]|$))"));
        QStringList lines = text.split(QLatin1Char('\n'));
        for (QString& line : lines) {
            const auto match = timing.match(line);
            if (match.hasMatch()) {
                for (int group : {3, 1}) {
                    if (match.capturedView(group).count(QLatin1Char(':')) == 1) {
                        line.insert(match.capturedStart(group), QStringLiteral("00:"));
                    }
                }
            }
        }
        text = lines.join(QLatin1Char('\n'));
    }
    const auto parsed = sasayaki::parse_cues(text.toStdString());
    QList<Cue> result;
    result.reserve(qsizetype(parsed.size()));
    static const QRegularExpression tags(QStringLiteral(R"(<[^>\n]+>)"));
    for (const auto& cue : parsed) {
        QString plain = QString::fromStdString(cue.text);
        if (vtt) {
            plain.remove(tags)
                .replace(QStringLiteral("&nbsp;"), QStringLiteral(" "))
                .remove(QStringLiteral("&lrm;"))
                .remove(QStringLiteral("&rlm;"))
                .replace(QStringLiteral("&lt;"), QStringLiteral("<"))
                .replace(QStringLiteral("&gt;"), QStringLiteral(">"))
                .replace(QStringLiteral("&quot;"), QStringLiteral("\""))
                .replace(QStringLiteral("&apos;"), QStringLiteral("'"))
                .replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        }
        if (!plain.trimmed().isEmpty()) {
            result.append({cue.start_time, cue.end_time, plain.trimmed()});
        }
    }
    return result;
}

QUrl local_url(const std::string& path) {
    return QUrl::fromLocalFile(QString::fromStdString(path));
}

QString search_key(const QString& text) {
    static const QRegularExpression marks(QStringLiteral(R"(\p{M}+)"));
    return text.normalized(QString::NormalizationForm_KD).toCaseFolded().remove(marks);
}

}  // namespace

struct PlayerStudyViewModel::State {
    player::MiningHistoryStore history_store = player::MiningHistoryStore::at_data_root();
    QUrl video;
    QUrl subtitle_file;
    QString subtitle_format;
    QString transcript_error;
    QString notice;
    QString search;
    QList<Cue> cues;
    QList<double> end_prefix;
    QList<int> filtered;
    int first = 0;
    int last = 0;
    int current = -1;
    int focus = -1;
    int current_chapter = -1;
    double position = 0.0;
    double delay = 0.0;
    Cue native{-1.0, -1.0, {}};
    bool ready = false;
    bool visible = true;
    bool can_mine = false;
    bool following = true;
    QVariantList transcript;
    QVariantList history;
    QVariantList chapters;
    QVariantMap track;
    player::MiningHistoryStore::Snapshot snapshot;
    std::optional<MiningHistoryItem> pending_jump;

    auto first_after(double time) const {
        return std::upper_bound(cues.cbegin(), cues.cend(), time,
                                [](double time, const Cue& cue) { return time < cue.start; });
    }

    int active_row(double time) const {
        for (qsizetype i = first_after(time) - cues.cbegin(); i > 0 && end_prefix[i - 1] >= time;) {
            --i;
            if (cues[i].end >= time) {
                return int(i);
            }
        }
        return -1;
    }

    int nearest_row(double time) const {
        return current >= 0 || cues.isEmpty() ? current
            : int(std::min(first_after(time) - cues.cbegin(), cues.size() - 1));
    }

    auto find_item(const QString& id) const {
        const std::string key = id.toStdString();
        return std::ranges::find(snapshot.items, key, &MiningHistoryItem::id);
    }
};

PlayerStudyViewModel::PlayerStudyViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    refreshHistory();
}

PlayerStudyViewModel::~PlayerStudyViewModel() = default;

QVariantList PlayerStudyViewModel::transcript() const { return state_->transcript; }
QString PlayerStudyViewModel::transcript_error() const { return state_->transcript_error; }
double PlayerStudyViewModel::adjusted_time() const { return state_->position - state_->delay; }
int PlayerStudyViewModel::current_row() const { return state_->current; }
int PlayerStudyViewModel::current_visible_row() const {
    const auto begin = state_->filtered.cbegin() + state_->first;
    const auto end = state_->filtered.cbegin() + state_->last;
    const auto match = std::lower_bound(begin, end, state_->focus);
    return match != end && *match == state_->focus ? int(match - begin) : -1;
}
QString PlayerStudyViewModel::search() const { return state_->search; }
bool PlayerStudyViewModel::can_show_earlier() const { return state_->first > 0; }
bool PlayerStudyViewModel::can_show_later() const { return state_->last < state_->filtered.size(); }
QVariantList PlayerStudyViewModel::history() const { return state_->history; }
QVariantList PlayerStudyViewModel::chapters() const { return state_->chapters; }
int PlayerStudyViewModel::current_chapter() const { return state_->current_chapter; }
QString PlayerStudyViewModel::last_mining_notice() const { return state_->notice; }
int PlayerStudyViewModel::history_limit() const { return state_->snapshot.limit; }
bool PlayerStudyViewModel::can_mine() const { return state_->can_mine; }

void PlayerStudyViewModel::set_search(const QString& query) {
    if (state_->search != query) {
        state_->search = query;
        rebuild_filter();
        emit searchChanged();
    }
}

void PlayerStudyViewModel::set_chapters(const QVariantList& chapters) {
    if (state_->chapters != chapters) {
        state_->chapters = chapters;
        emit chaptersChanged();
        update_current();
    }
}

void PlayerStudyViewModel::setTracks(const QVariantList& tracks) {
    state_->track.clear();
    for (const QVariant& entry : tracks) {
        const auto track = entry.toMap();
        if (track.value(QStringLiteral("type")).toString() == QStringLiteral("sub") &&
            track.value(QStringLiteral("isSelected")).toBool()) {
            state_->track = track;
            break;
        }
    }
}

void PlayerStudyViewModel::setSubtitlesVisible(bool visible) {
    state_->visible = visible;
    update_can_mine();
}

void PlayerStudyViewModel::setVideo(const QUrl& video) {
    state_->ready = false;
    state_->video = video.isLocalFile()
        ? local_url(video.toLocalFile().toStdString()) : video;
    if (state_->pending_jump &&
        (!state_->pending_jump->video_path ||
         local_url(*state_->pending_jump->video_path) != state_->video)) {
        state_->pending_jump.reset();
    }
    const double previous_time = adjusted_time();
    state_->position = 0.0;
    if (previous_time != adjusted_time()) {
        emit adjustedTimeChanged();
    }
    updateNativeCue({}, -1.0, -1.0);
    setSubtitleDocument({}, {}, false);
    set_notice({});
    refreshHistory();
}

void PlayerStudyViewModel::setPlaybackReady(bool ready) {
    if (state_->ready == ready) {
        return;
    }
    state_->ready = ready;
    update_can_mine();
    if (ready) {
        complete_jump();
    }
}

void PlayerStudyViewModel::updatePlayback(double position, double subtitleDelay) {
    const double previous_time = adjusted_time();
    state_->position = position;
    state_->delay = subtitleDelay;
    if (previous_time != adjusted_time()) {
        emit adjustedTimeChanged();
    }
    update_current();
}

void PlayerStudyViewModel::updateNativeCue(const QString& text, double start, double end) {
    state_->native = {start, end, text.trimmed()};
    update_can_mine();
}

void PlayerStudyViewModel::setSubtitleDocument(const QUrl& file, const QByteArray& bytes,
                                              bool readable) {
    state_->subtitle_file = file;
    if (!file.isEmpty()) {
        set_notice({});
    }
    state_->subtitle_format = subtitle_format(file);
    state_->transcript_error.clear();
    state_->cues.clear();
    if (!file.isEmpty()) {
        if (!readable) {
            state_->transcript_error = tr("Could not read the subtitle file.");
        } else if (state_->subtitle_format.isEmpty()) {
            state_->transcript_error = tr("This subtitle format is not supported for transcripts.");
        } else {
            state_->cues = parse_document(bytes, state_->subtitle_format);
            if (state_->cues.isEmpty()) {
                state_->transcript_error = tr("No readable cues were found in this subtitle file.");
            }
        }
    }
    publish_transcript();
}

void PlayerStudyViewModel::setEmbeddedTranscript(const QByteArray& bytes, const QString& error) {
    if (!state_->subtitle_file.isEmpty()) {
        return;
    }
    state_->transcript_error = error;
    state_->cues = parse_document(bytes, QStringLiteral("srt"));
    if (!bytes.isEmpty() && state_->cues.isEmpty()) {
        state_->transcript_error = tr("No readable cues were found in the embedded subtitle track.");
    }
    publish_transcript();
}

void PlayerStudyViewModel::publish_transcript() {
    std::ranges::stable_sort(state_->cues, [](const Cue& lhs, const Cue& rhs) {
        return std::tie(lhs.start, lhs.end) < std::tie(rhs.start, rhs.end);
    });
    state_->end_prefix.clear();
    state_->end_prefix.reserve(state_->cues.size());
    double latest = -1.0;
    for (const Cue& cue : std::as_const(state_->cues)) {
        latest = std::max(latest, cue.end);
        state_->end_prefix.append(latest);
    }
    const int current = state_->active_row(adjusted_time());
    if (std::exchange(state_->current, current) != current)
        emit currentRowChanged();
    rebuild_filter();
    update_can_mine();
}

void PlayerStudyViewModel::rebuild_filter() {
    state_->filtered.clear();
    const QString query = search_key(state_->search.trimmed());
    for (qsizetype i = 0; i < state_->cues.size(); ++i) {
        if (query.isEmpty() || search_key(state_->cues[i].text).contains(query)) {
            state_->filtered.append(int(i));
        }
    }
    state_->focus = state_->nearest_row(adjusted_time());
    reset_window(query.isEmpty() ? state_->focus : 0);
}

void PlayerStudyViewModel::reset_window(int focus) {
    const int count = int(state_->filtered.size());
    if (count == 0) {
        state_->first = state_->last = 0;
    } else {
        focus = std::clamp(focus, 0, count - 1);
        state_->last = std::min(count, std::max(0, focus - kWindowSize / 2) + kWindowSize);
        state_->first = std::max(0, state_->last - kWindowSize);
    }
    publish_window();
}

void PlayerStudyViewModel::publish_window() {
    QVariantList rows;
    rows.reserve(state_->last - state_->first);
    for (int i = state_->first; i < state_->last; ++i) {
        const int index = state_->filtered[i];
        const Cue& cue = state_->cues[index];
        rows.append(QVariantMap{{QStringLiteral("index"), index}, {QStringLiteral("start"), cue.start},
                                {QStringLiteral("end"), cue.end}, {QStringLiteral("text"), cue.text}});
    }
    state_->transcript = std::move(rows);
    emit transcriptChanged();
    emit currentVisibleRowChanged();
}

void PlayerStudyViewModel::showEarlier() {
    state_->first = std::max(0, state_->first - kWindowExtension);
    publish_window();
}

void PlayerStudyViewModel::showLater() {
    state_->last = std::min(int(state_->filtered.size()), state_->last + kWindowExtension);
    publish_window();
}

void PlayerStudyViewModel::setTranscriptFollowing(bool following) {
    state_->following = following;
    if (following && state_->search.trimmed().isEmpty() && state_->focus >= 0 &&
        (state_->focus < state_->first || state_->focus >= state_->last)) {
        reset_window(state_->focus);
    }
}

void PlayerStudyViewModel::update_current() {
    const int current = state_->active_row(adjusted_time());
    if (std::exchange(state_->current, current) != current)
        emit currentRowChanged();
    const int focus = state_->nearest_row(adjusted_time());
    if (state_->focus != focus) {
        state_->focus = focus;
        if (state_->following && state_->search.trimmed().isEmpty() &&
            (focus < state_->first || focus >= state_->last ||
             (state_->first > 0 && focus < state_->first + kWindowSize / 4) ||
             (can_show_later() && focus >= state_->last - kWindowSize / 4))) {
            reset_window(focus);
        } else {
            emit currentVisibleRowChanged();
        }
    }

    int chapter = -1;
    double latest = -std::numeric_limits<double>::infinity();
    for (qsizetype i = 0; i < state_->chapters.size(); ++i) {
        const double start = state_->chapters[i].toMap().value(QStringLiteral("start")).toDouble();
        if (start <= state_->position && start > latest) {
            chapter = int(i);
            latest = start;
        }
    }
    if (state_->current_chapter != chapter) {
        state_->current_chapter = chapter;
        emit currentChapterChanged();
    }
    update_can_mine();
}

void PlayerStudyViewModel::seekToRow(int index) {
    if (state_->ready && index >= 0 && index < state_->cues.size()) {
        emit seekRequested(std::max(0.0, state_->cues[index].start + state_->delay));
    }
}

void PlayerStudyViewModel::playChapter(int index) {
    if (state_->ready && index >= 0 && index < state_->chapters.size()) {
        emit chapterRequested(state_->chapters[index].toMap().value(QStringLiteral("id")).toInt());
    }
}

void PlayerStudyViewModel::update_can_mine() {
    const bool can_mine = state_->ready && state_->visible && state_->snapshot.limit > 0 &&
        !state_->video.isEmpty() && (state_->cues.isEmpty()
            ? !state_->native.text.isEmpty() && state_->native.start >= 0.0
            : state_->current >= 0);
    if (state_->can_mine != can_mine) {
        state_->can_mine = can_mine;
        emit canMineChanged();
    }
}

void PlayerStudyViewModel::mineCurrentSubtitle() {
    refreshHistory();
    if (!state_->can_mine) {
        set_notice(state_->snapshot.limit == 0 ? tr("Mining history is disabled.") : tr("No current subtitle."));
        return;
    }
    QList<Cue> active;
    if (!state_->cues.isEmpty()) {
        const double time = adjusted_time();
        for (qsizetype i = state_->first_after(time) - state_->cues.cbegin();
             i > 0 && state_->end_prefix[i - 1] >= time;) {
            --i;
            if (state_->cues[i].end >= time) {
                active.append(state_->cues[i]);
            }
        }
        std::ranges::sort(active, [](const Cue& lhs, const Cue& rhs) {
            return std::tie(lhs.start, lhs.end, lhs.text) < std::tie(rhs.start, rhs.end, rhs.text);
        });
    } else {
        Cue cue = state_->native;
        // Older builds and some tracks do not provide an end time. Follow
        // HSMpvClient's fallback cue duration without inventing a start.
        if (cue.end < cue.start) {
            cue.end = cue.start + 10.0;
        }
        active.append(cue);
    }
    if (active.isEmpty()) {
        set_notice(tr("No current subtitle."));
        return;
    }

    MiningHistoryItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
    item.created_at = QDateTime::currentMSecsSinceEpoch() / 1000.0 - kFoundationEpoch;
    item.video_file_name = state_->video.fileName().toStdString();
    item.video_title = item.video_file_name;
    if (state_->video.isLocalFile()) {
        item.video_path = state_->video.toLocalFile().toStdString();
    }
    if (!state_->subtitle_file.isEmpty()) {
        item.subtitle_source_name = state_->subtitle_file.fileName().toStdString();
        item.subtitle_source_path = state_->subtitle_file.toLocalFile().toStdString();
        if (!state_->subtitle_format.isEmpty()) {
            item.subtitle_format = state_->subtitle_format.toStdString();
        }
    } else {
        item.subtitle_source_name = item.video_file_name;
        item.subtitle_format = "embedded";
        const auto id = state_->track.value(QStringLiteral("id"));
        if (id.isValid()) {
            item.embedded_subtitle_track_id = id.toInt();
        }
    }
    item.cue_start = active.front().start;
    item.cue_end = active.front().end;
    QStringList lines;
    for (const Cue& cue : std::as_const(active)) {
        item.cue_start = std::min(item.cue_start, cue.start);
        item.cue_end = std::max(item.cue_end, cue.end);
        lines.append(cue.text);
    }
    item.subtitle_text = lines.join(QLatin1Char('\n')).toStdString();
    if (!state_->history_store.add(item, state_->snapshot.limit)) {
        set_notice(tr("Could not save the subtitle to mining history."));
        return;
    }
    refreshHistory();
    set_notice(tr("Subtitle added to mining history."));
}

void PlayerStudyViewModel::jumpToHistory(const QString& id) {
    const auto item = state_->find_item(id);
    if (item == state_->snapshot.items.cend()) {
        return;
    }
    if (item->remote_video_identity) {
        set_notice(tr("This remote video cannot be reopened from history."));
        return;
    }
    const QUrl target = item->video_path ? local_url(*item->video_path) : QUrl{};
    if (target.isEmpty() && (!state_->ready || state_->video.fileName().toStdString() != item->video_file_name)) {
        set_notice(tr("The original video is unavailable."));
        return;
    }
    state_->pending_jump = *item;
    set_notice({});
    if (!target.isEmpty() && (target != state_->video || !state_->ready)) {
        emit videoRequested(target);
    } else {
        complete_jump();
    }
}

void PlayerStudyViewModel::complete_jump() {
    if (!state_->pending_jump) {
        return;
    }
    const MiningHistoryItem item = std::move(*state_->pending_jump);
    state_->pending_jump.reset();
    const QUrl subtitle = item.subtitle_source_path ? local_url(*item.subtitle_source_path) : QUrl{};
    emit historySubtitleRequested(subtitle, item.embedded_subtitle_track_id.value_or(-1),
                                  std::max(0.0, item.cue_start + state_->delay));
}

void PlayerStudyViewModel::copyHistory(const QString& id) const {
    const auto item = state_->find_item(id);
    if (item != state_->snapshot.items.cend()) {
        if (auto* clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(QString::fromStdString(item->subtitle_text));
        }
    }
}

void PlayerStudyViewModel::removeHistory(const QString& id) {
    if (!state_->history_store.remove(id.toStdString())) {
        set_notice(tr("Could not delete the saved subtitle."));
    } else {
        refreshHistory();
        set_notice({});
    }
}

void PlayerStudyViewModel::clearHistory() {
    if (!state_->history_store.clear()) {
        set_notice(tr("Could not clear mining history."));
    } else {
        refreshHistory();
        set_notice({});
    }
}

void PlayerStudyViewModel::refreshHistory() {
    auto snapshot = state_->history_store.load_snapshot();
    QVariantList history;
    const auto now = QDateTime::currentDateTime();
    for (auto item = snapshot.items.crbegin(); item != snapshot.items.crend(); ++item) {
        history.append(QVariantMap{
            {QStringLiteral("id"), QString::fromStdString(item->id)},
            {QStringLiteral("text"), QString::fromStdString(item->subtitle_text)},
            {QStringLiteral("videoTitle"), QString::fromStdString(item->video_title)},
            {QStringLiteral("start"), item->cue_start},
            {QStringLiteral("relativeDate"), player::mining_history_date_label(item->created_at, now)},
        });
    }
    const bool changed = state_->snapshot.limit != snapshot.limit || state_->history != history;
    state_->snapshot = std::move(snapshot);
    if (changed) {
        state_->history = std::move(history);
        emit historyChanged();
    }
    update_can_mine();
}

void PlayerStudyViewModel::reportSubtitleLoadFailure() {
    set_notice(tr("Could not load the subtitle file."));
}

void PlayerStudyViewModel::set_notice(const QString& notice) {
    if (state_->notice != notice) {
        state_->notice = notice;
        emit lastMiningNoticeChanged();
    }
}

}  // namespace niratan::viewmodels

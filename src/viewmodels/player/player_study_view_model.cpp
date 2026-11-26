// SPDX-License-Identifier: GPL-3.0-or-later

#include "player_study_view_model_state.h"

#include "services/player/mining_history_store.h"
#include "services/player/subtitle_document.h"

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

// SubtitleFormat raw values, as the mining history stores them.
QString subtitle_format_value(player::SubtitleFormat format) {
    switch (format) {
        case player::SubtitleFormat::srt:
            return QStringLiteral("srt");
        case player::SubtitleFormat::web_vtt:
            return QStringLiteral("webVTT");
        case player::SubtitleFormat::ass:
            return QStringLiteral("ass");
        case player::SubtitleFormat::ssa:
            return QStringLiteral("ssa");
    }
    return {};
}

QList<Cue> cues_from_document(const player::SubtitleDocument& document) {
    QList<Cue> cues;
    cues.reserve(document.cues.size());
    for (const auto& cue : document.cues) {
        cues.append({cue.start_time, cue.end_time, cue.text});
    }
    return cues;
}

QUrl local_url(const std::string& path) {
    return QUrl::fromLocalFile(QString::fromStdString(path));
}

QString search_key(const QString& text) {
    static const QRegularExpression marks(QStringLiteral(R"(\p{M}+)"));
    return text.normalized(QString::NormalizationForm_KD).toCaseFolded().remove(marks);
}

}  // namespace

PlayerStudyViewModel::PlayerStudyViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    refreshHistory();
}

PlayerStudyViewModel::~PlayerStudyViewModel() = default;

QVariantList PlayerStudyViewModel::transcript() const { return state_->transcript; }
QVariantList PlayerStudyViewModel::primary_cues() const {
    QVariantList rows;
    rows.reserve(state_->cues.size());
    for (const Cue& cue : state_->cues) {
        rows.append(
            QVariantMap{{QStringLiteral("start"), cue.start}, {QStringLiteral("end"), cue.end}});
    }
    return rows;
}
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
    state_->subtitle_format.clear();
    state_->transcript_error.clear();
    state_->cues.clear();
    if (!file.isEmpty()) {
        if (!readable) {
            state_->transcript_error = tr("Could not read the subtitle file.");
        } else {
            // The source path's extension decides the format
            // (SubtitleParser.format(for:)), so no hint is passed.
            const auto parsed = player::parse_subtitle_document(bytes, file);
            if (parsed.document) {
                state_->cues = cues_from_document(*parsed.document);
                state_->subtitle_format = subtitle_format_value(parsed.document->format);
            } else if (parsed.error == player::SubtitleParseError::unsupported_format) {
                state_->transcript_error =
                    tr("This subtitle format is not supported for transcripts.");
            } else {
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
    state_->cues.clear();
    if (!bytes.isEmpty()) {
        const auto parsed =
            player::parse_subtitle_document(bytes, QUrl{}, player::SubtitleFormat::srt);
        if (parsed.document) {
            state_->cues = cues_from_document(*parsed.document);
        } else {
            state_->transcript_error =
                tr("No readable cues were found in the embedded subtitle track.");
        }
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
    player::SubtitleDocument document;
    document.source = state_->subtitle_file;
    document.cues.reserve(state_->cues.size());
    for (qsizetype i = 0; i < state_->cues.size(); ++i) {
        const Cue& cue = state_->cues[i];
        document.cues.append({QString::number(i), cue.start, cue.end, cue.text});
    }
    state_->alignment_store.emplace(std::move(document));
    update_alignment_availability();
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
    update_alignment_availability();
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
    // Mining uses every cue covering adjusted time, so an ended overlapping row
    // must not hide an earlier cue that still runs (SubtitleCueStore.cues_at).
    const bool has_active_cue =
        state_->alignment_store && !state_->alignment_store->cues_at(adjusted_time()).isEmpty();
    const bool can_mine = state_->ready && state_->visible && state_->snapshot.limit > 0 &&
        !state_->video.isEmpty() &&
        (state_->cues.isEmpty() ? !state_->native.text.isEmpty() && state_->native.start >= 0.0
                                : has_active_cue);
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
    item.video_file_name = state_->video.fileName().section(u'.', 0, -2).toStdString();
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

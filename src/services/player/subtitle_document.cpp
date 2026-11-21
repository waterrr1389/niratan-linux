// SPDX-License-Identifier: GPL-3.0-or-later

#include "subtitle_document.h"

#include "subtitle_document_internal.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace niratan::player {
namespace {

constexpr double kLongCueThreshold = 30.0;

std::optional<qint64> timestamp_milliseconds(double time) {
    if (!std::isfinite(time) || time < 0) {
        return std::nullopt;
    }
    const double milliseconds = std::round(time * 1000.0);
    if (!std::isfinite(milliseconds) || milliseconds < 0 ||
        milliseconds >= static_cast<double>(std::numeric_limits<qint64>::max())) {
        return std::nullopt;
    }
    return static_cast<qint64>(milliseconds);
}

QString first_token(const QString& text) {
    qsizetype begin = 0;
    while (begin < text.size() && detail::is_whitespace(text[begin])) {
        ++begin;
    }
    qsizetype end = begin;
    while (end < text.size() && !detail::is_whitespace(text[end])) {
        ++end;
    }
    return begin == end ? text : text.mid(begin, end - begin);
}

QString clean_web_vtt_text(QString text) {
    static const QRegularExpression tags(QStringLiteral(R"(<[^>\n]+>)"));
    text.remove(tags);
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.remove(QStringLiteral("&lrm;"));
    text.remove(QStringLiteral("&rlm;"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    QStringList lines;
    for (const QString& line : text.split(QLatin1Char('\n'), Qt::KeepEmptyParts)) {
        const QString cleaned = detail::trimmed(line, detail::is_line_whitespace);
        if (!cleaned.isEmpty()) {
            lines.append(cleaned);
        }
    }
    return detail::trim_all(lines.join(QLatin1Char('\n')));
}

std::optional<SubtitleCue> parse_block(const QString& block, int index, SubtitleFormat format) {
    QStringList lines = block.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    while (!lines.isEmpty() && detail::trim_all(lines.first()).isEmpty()) {
        lines.removeFirst();
    }
    if (lines.isEmpty()) {
        return std::nullopt;
    }
    if (format == SubtitleFormat::web_vtt && lines.first().startsWith(QStringLiteral("WEBVTT"))) {
        lines.removeFirst();
    }
    if (lines.isEmpty()) {
        return std::nullopt;
    }
    qsizetype timing_index = -1;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (lines.at(i).contains(QStringLiteral("-->"))) {
            timing_index = i;
            break;
        }
    }
    if (timing_index < 0) {
        return std::nullopt;
    }
    const QStringList timing =
        lines.at(timing_index).split(QStringLiteral("-->"), Qt::KeepEmptyParts);
    if (timing.size() != 2) {
        return std::nullopt;
    }
    const auto start = detail::parse_timestamp(timing.at(0));
    const auto end = detail::parse_timestamp(first_token(timing.at(1)));
    if (!start || !end || *end < *start) {
        return std::nullopt;
    }
    QString raw_text;
    for (qsizetype i = timing_index + 1; i < lines.size(); ++i) {
        if (i > timing_index + 1) {
            raw_text.append(QLatin1Char('\n'));
        }
        raw_text.append(lines.at(i));
    }
    raw_text = detail::trim_all(raw_text);
    const QString text =
        format == SubtitleFormat::web_vtt ? clean_web_vtt_text(raw_text) : raw_text;
    if (text.isEmpty()) {
        return std::nullopt;
    }
    QString declared_id;
    if (timing_index > 0) {
        declared_id = detail::trim_all(lines.at(timing_index - 1));
    }
    return SubtitleCue{declared_id.isEmpty() ? QString::number(index) : declared_id, *start, *end,
                       text};
}

bool ignored_block(const QString& block) {
    const QString text = detail::trim_all(block);
    return text.isEmpty() || text == QStringLiteral("WEBVTT") ||
        text.startsWith(QStringLiteral("NOTE"));
}

auto by_start(const SubtitleCue& lhs, const SubtitleCue& rhs) {
    return std::tie(lhs.start_time, lhs.end_time) < std::tie(rhs.start_time, rhs.end_time);
}

auto by_end(const SubtitleCue& lhs, const SubtitleCue& rhs) {
    return std::tie(lhs.end_time, lhs.start_time) < std::tie(rhs.end_time, rhs.start_time);
}

}  // namespace

namespace detail {

bool is_whitespace(QChar character) {
    switch (character.unicode()) {
        case 0x0009:
        case 0x000A:
        case 0x000B:
        case 0x000C:
        case 0x000D:
        case 0x0020:
        case 0x0085:
        case 0x00A0:
        case 0x1680:
        case 0x2028:
        case 0x2029:
        case 0x202F:
        case 0x205F:
        case 0x3000:
            return true;
        default:
            return character.unicode() >= 0x2000 && character.unicode() <= 0x200A;
    }
}

bool is_line_whitespace(QChar character) {
    switch (character.unicode()) {
        case 0x0009:
        case 0x0020:
        case 0x00A0:
        case 0x1680:
        case 0x202F:
        case 0x205F:
        case 0x3000:
            return true;
        default:
            return character.unicode() >= 0x2000 && character.unicode() <= 0x200A;
    }
}

QString trimmed(QStringView text, bool (*is_space)(QChar)) {
    qsizetype begin = 0;
    qsizetype end = text.size();
    while (begin < end && is_space(text[begin])) {
        ++begin;
    }
    while (end > begin && is_space(text[end - 1])) {
        --end;
    }
    return text.mid(begin, end - begin).toString();
}

QString trim_all(QStringView text) {
    return trimmed(text, is_whitespace);
}

QString decode_document(const QByteArray& data) {
    QStringDecoder utf8(QStringDecoder::Utf8);
    const QString decoded = utf8.decode(data);
    if (!utf8.hasError()) {
        QString text = decoded;
        text.remove(QChar(0xFEFF));
        return text;
    }
    const auto byte = [&data](qsizetype index) { return quint8(data.at(index)); };
    if (data.size() >= 2 &&
        ((byte(0) == 0xFF && byte(1) == 0xFE) || (byte(0) == 0xFE && byte(1) == 0xFF))) {
        QStringDecoder utf16(byte(0) == 0xFF ? QStringDecoder::Utf16LE : QStringDecoder::Utf16BE);
        const QString text = utf16.decode(data);
        if (!utf16.hasError()) {
            QString cleaned = text;
            cleaned.remove(QChar(0xFEFF));
            return cleaned;
        }
    }
    return QString::fromUtf8(data);
}

QString normalize_crlf(QString text) {
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString normalize_blocks(QString text) {
    text = normalize_crlf(std::move(text));
    QString collapsed;
    collapsed.reserve(text.size());
    int newlines = 0;
    for (const QChar character : text) {
        if (character == QLatin1Char('\n')) {
            ++newlines;
            continue;
        }
        if (newlines > 0) {
            collapsed.append(newlines >= 2 ? QStringLiteral("\n\n") : QStringLiteral("\n"));
            newlines = 0;
        }
        collapsed.append(character);
    }
    if (newlines > 0) {
        collapsed.append(newlines >= 2 ? QStringLiteral("\n\n") : QStringLiteral("\n"));
    }
    return collapsed;
}

std::optional<double> parse_timestamp(QStringView raw) {
    QString timestamp = trim_all(raw);
    timestamp.replace(QLatin1Char(','), QLatin1Char('.'));
    QStringList fields;
    for (const QString& field : timestamp.split(QLatin1Char(':'))) {
        if (!field.isEmpty()) {
            fields.append(field);
        }
    }
    if (fields.size() != 2 && fields.size() != 3) {
        return std::nullopt;
    }
    double hours = 0;
    qsizetype index = 0;
    auto next = [&fields, &index]() -> std::optional<double> {
        const QString& field = fields.at(index++);
        // Swift's Double(String) rejects surrounding whitespace; toDouble
        // accepts it, so the field must already be the whole token.
        if (field != trim_all(field)) {
            return std::nullopt;
        }
        bool ok = false;
        const double value = field.toDouble(&ok);
        return ok ? std::optional(value) : std::nullopt;
    };
    if (fields.size() == 3) {
        const auto parsed = next();
        if (!parsed) {
            return std::nullopt;
        }
        hours = *parsed;
    }
    const auto minutes = next();
    const auto seconds = next();
    if (!minutes || !seconds) {
        return std::nullopt;
    }
    const double result = hours * 3600.0 + *minutes * 60.0 + *seconds;
    if (!timestamp_milliseconds(result)) {
        return std::nullopt;
    }
    return result;
}

}  // namespace detail

SubtitleParseResult parse_subtitle_document(const QByteArray& data, const QUrl& source,
                                            std::optional<SubtitleFormat> format_hint) {
    SubtitleParseResult result;
    SubtitleFormat format = SubtitleFormat::srt;
    if (format_hint) {
        format = *format_hint;
    } else {
        const QString suffix = QFileInfo(source.toLocalFile()).suffix().toLower();
        if (suffix == QStringLiteral("srt")) {
            format = SubtitleFormat::srt;
        } else if (suffix == QStringLiteral("vtt")) {
            format = SubtitleFormat::web_vtt;
        } else if (suffix == QStringLiteral("ass")) {
            format = SubtitleFormat::ass;
        } else if (suffix == QStringLiteral("ssa")) {
            format = SubtitleFormat::ssa;
        } else {
            result.error = SubtitleParseError::unsupported_format;
            return result;
        }
    }

    const QString text = detail::decode_document(data);
    if (format == SubtitleFormat::ass || format == SubtitleFormat::ssa) {
        return detail::parse_ass_document(detail::normalize_crlf(text), source, format);
    }

    const QStringList blocks =
        detail::normalize_blocks(text).split(QStringLiteral("\n\n"), Qt::KeepEmptyParts);
    QList<SubtitleCue> cues;
    QStringList warnings;
    for (qsizetype i = 0; i < blocks.size(); ++i) {
        const auto cue = parse_block(blocks.at(i), int(i), format);
        if (cue) {
            cues.append(*cue);
        } else if (!ignored_block(blocks.at(i))) {
            warnings.append(QStringLiteral("Skipped subtitle block %1.").arg(i + 1));
        }
    }
    if (cues.isEmpty()) {
        result.error = SubtitleParseError::no_valid_cues;
        return result;
    }
    std::stable_sort(cues.begin(), cues.end(), by_start);
    result.document = SubtitleDocument{source, format, cues, warnings};
    return result;
}

SubtitleCueStore::SubtitleCueStore(SubtitleDocument document) : document_(std::move(document)) {
    cues_by_start_ = document_.cues;
    std::stable_sort(cues_by_start_.begin(), cues_by_start_.end(), by_start);
    for (const SubtitleCue& cue : std::as_const(cues_by_start_)) {
        const double duration = cue.end_time - cue.start_time;
        if (duration > kLongCueThreshold) {
            long_cues_.append(cue);
        } else {
            timed_cues_.append(cue);
            max_timed_cue_duration_ = std::max(max_timed_cue_duration_, duration);
        }
    }
    cues_by_end_ = cues_by_start_;
    std::stable_sort(cues_by_end_.begin(), cues_by_end_.end(), by_end);
}

QList<SubtitleCue> SubtitleCueStore::cues_at(double time) const {
    QList<SubtitleCue> matches;
    for (const SubtitleCue& cue : long_cues_) {
        if (cue.start_time <= time && time <= cue.end_time) {
            matches.append(cue);
        }
    }
    qsizetype low = 0;
    qsizetype high = timed_cues_.size();
    while (low < high) {
        const qsizetype middle = (low + high) / 2;
        if (timed_cues_.at(middle).start_time <= time) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    const double earliest_possible_start = time - max_timed_cue_duration_;
    for (qsizetype index = low - 1; index >= 0; --index) {
        const SubtitleCue& cue = timed_cues_.at(index);
        if (cue.start_time < earliest_possible_start) {
            break;
        }
        if (time <= cue.end_time) {
            matches.append(cue);
        }
    }
    std::stable_sort(matches.begin(), matches.end(), by_start);
    return matches;
}

QList<SubtitleCue> SubtitleCueStore::cues_at_playback(double playback_time,
                                                      double subtitle_delay) const {
    return cues_at(playback_time - subtitle_delay);
}

namespace {

QList<SubtitleCue> latest_cues_ending_before(const QList<SubtitleCue>& cues_by_end, double time) {
    qsizetype low = 0;
    qsizetype high = cues_by_end.size();
    while (low < high) {
        const qsizetype middle = (low + high) / 2;
        if (cues_by_end.at(middle).end_time < time) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    const qsizetype last = low - 1;
    if (last < 0) {
        return {};
    }
    const double end_time = cues_by_end.at(last).end_time;
    QList<SubtitleCue> result;
    for (qsizetype index = last; index >= 0 && cues_by_end.at(index).end_time == end_time;
         --index) {
        result.append(cues_by_end.at(index));
    }
    std::stable_sort(result.begin(), result.end(), by_start);
    return result;
}

QList<SubtitleCue> earliest_cues_starting_after(const QList<SubtitleCue>& cues_by_start,
                                                double time) {
    qsizetype low = 0;
    qsizetype high = cues_by_start.size();
    while (low < high) {
        const qsizetype middle = (low + high) / 2;
        if (cues_by_start.at(middle).start_time <= time) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    if (low >= cues_by_start.size()) {
        return {};
    }
    const double start_time = cues_by_start.at(low).start_time;
    QList<SubtitleCue> result;
    for (qsizetype index = low;
         index < cues_by_start.size() && cues_by_start.at(index).start_time == start_time;
         ++index) {
        result.append(cues_by_start.at(index));
    }
    return result;
}

}  // namespace

SubtitleCueSlice SubtitleCueStore::slice_at(double time) const {
    return SubtitleCueSlice{cues_at(time), latest_cues_ending_before(cues_by_end_, time),
                            earliest_cues_starting_after(cues_by_start_, time)};
}

SubtitleCueSlice SubtitleCueStore::slice_at_playback(double playback_time,
                                                     double subtitle_delay) const {
    return slice_at(playback_time - subtitle_delay);
}

std::optional<double>
SubtitleCueStore::delay_aligning_adjacent_cue(double playback_time, double subtitle_delay,
                                              SubtitleOffsetDirection direction) const {
    const SubtitleCueSlice slice = slice_at_playback(playback_time, subtitle_delay);
    const QList<SubtitleCue>& adjacent =
        direction == SubtitleOffsetDirection::previous ? slice.last_shown : slice.next_to_show;
    if (adjacent.isEmpty()) {
        return std::nullopt;
    }
    const SubtitleCue& cue =
        direction == SubtitleOffsetDirection::previous ? adjacent.last() : adjacent.first();
    return playback_time - cue.start_time;
}

}  // namespace niratan::player

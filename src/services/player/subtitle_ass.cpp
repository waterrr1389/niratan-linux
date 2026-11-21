// SPDX-License-Identifier: GPL-3.0-or-later
//
// The ASS/SSA plain-text cue path of Features/Video/Subtitles/
// SubtitleParser.swift (parseAdvancedSubStationAlpha's event scan,
// parseASSFields, splitASSValues, the cue parseASSEvent builds) and
// ASSSubtitleTextSanitizer.clean / updateDrawingScale from
// Features/Video/Subtitles/EmbeddedSubtitlePayloadParser.swift.
//
// Only cues are produced. The render plan, style fields, event markers,
// duplicate-key walk and effects split stay with libass, so style lines,
// comment lines and metadata are read only as far as they affect the section
// or the dialogue field order.

#include "subtitle_document_internal.h"

#include <algorithm>
#include <optional>
#include <tuple>

namespace niratan::player::detail {
namespace {

const QStringList kDefaultAssDialogueFields = {
    QStringLiteral("layer"),   QStringLiteral("start"),   QStringLiteral("end"),
    QStringLiteral("style"),   QStringLiteral("name"),    QStringLiteral("marginl"),
    QStringLiteral("marginr"), QStringLiteral("marginv"), QStringLiteral("effect"),
    QStringLiteral("text"),
};

const QStringList kDefaultSsaDialogueFields = {
    QStringLiteral("marked"),  QStringLiteral("start"),   QStringLiteral("end"),
    QStringLiteral("style"),   QStringLiteral("name"),    QStringLiteral("marginl"),
    QStringLiteral("marginr"), QStringLiteral("marginv"), QStringLiteral("effect"),
    QStringLiteral("text"),
};

bool starts_with_ci(QStringView text, QStringView prefix) {
    return text.size() >= prefix.size() &&
        text.first(prefix.size()).compare(prefix, Qt::CaseInsensitive) == 0;
}

QStringList parse_ass_fields(const QString& raw) {
    QStringList fields;
    for (const QString& piece : raw.split(QLatin1Char(','), Qt::KeepEmptyParts)) {
        fields.append(trim_all(piece).toLower());
    }
    return fields;
}

QStringList split_ass_values(const QString& raw, const QStringList& fields) {
    if (fields.isEmpty()) {
        return {};
    }
    QStringList pieces = raw.split(QLatin1Char(','), Qt::KeepEmptyParts);
    const qsizetype text_index = fields.indexOf(QStringLiteral("text"));
    if (text_index < 0 || pieces.size() <= fields.size()) {
        if (pieces.size() >= fields.size()) {
            return pieces.mid(0, fields.size());
        }
        while (pieces.size() < fields.size()) {
            pieces.append(QString());
        }
        return pieces;
    }
    const qsizetype surplus = pieces.size() - fields.size();
    QStringList result;
    result.reserve(fields.size());
    for (qsizetype field_index = 0; field_index < fields.size(); ++field_index) {
        if (field_index < text_index) {
            result.append(pieces.at(field_index));
        } else if (field_index == text_index) {
            QStringList joined;
            for (qsizetype piece = field_index; piece <= field_index + surplus; ++piece) {
                joined.append(pieces.at(piece));
            }
            result.append(joined.join(QLatin1Char(',')));
        } else {
            result.append(pieces.at(field_index + surplus));
        }
    }
    return result;
}

int update_drawing_scale(const QString& block, int scale) {
    for (const QString& raw_tag : block.split(QLatin1Char('\\'), Qt::SkipEmptyParts)) {
        const QString tag = trim_all(raw_tag);
        if (tag.isEmpty() || tag.front().toLower() != QLatin1Char('p')) {
            continue;
        }
        const QStringView value_text = QStringView(tag).mid(1);
        qsizetype digits = 0;
        while (digits < value_text.size() && value_text.at(digits).isNumber()) {
            ++digits;
        }
        if (digits == 0) {
            continue;
        }
        bool ok = false;
        const int value = value_text.first(digits).toString().toInt(&ok);
        if (!ok) {
            continue;
        }
        scale = std::max(value, 0);
    }
    return scale;
}

QString sanitize_ass_text(QStringView raw) {
    QString output;
    output.reserve(raw.size());
    int drawing_scale = 0;
    qsizetype index = 0;
    while (index < raw.size()) {
        if (raw.at(index) == QLatin1Char('{')) {
            const qsizetype close = raw.indexOf(QLatin1Char('}'), index);
            if (close >= 0) {
                drawing_scale = update_drawing_scale(
                    raw.mid(index + 1, close - index - 1).toString(), drawing_scale);
                index = close + 1;
                continue;
            }
        }
        if (drawing_scale == 0) {
            output.append(raw.at(index));
        }
        ++index;
    }
    output.replace(QStringLiteral("\\N"), QStringLiteral("\n"));
    output.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    output.replace(QStringLiteral("\\h"), QStringLiteral(" "));
    return trim_all(output);
}

std::optional<SubtitleCue> parse_ass_event(const QString& raw_event, const QStringList& fields,
                                           int line_index) {
    const qsizetype start_index = fields.indexOf(QStringLiteral("start"));
    const qsizetype end_index = fields.indexOf(QStringLiteral("end"));
    const qsizetype text_index = fields.indexOf(QStringLiteral("text"));
    if (start_index < 0 || end_index < 0 || text_index < 0) {
        return std::nullopt;
    }
    const QStringList values = split_ass_values(raw_event, fields);
    if (values.size() <= start_index || values.size() <= end_index || values.size() <= text_index) {
        return std::nullopt;
    }
    const auto start = parse_timestamp(values.at(start_index));
    const auto end = parse_timestamp(values.at(end_index));
    if (!start || !end || *end < *start) {
        return std::nullopt;
    }
    const QString text = sanitize_ass_text(values.at(text_index));
    if (text.isEmpty()) {
        return std::nullopt;
    }
    return SubtitleCue{QStringLiteral("ass-%1").arg(line_index + 1), *start, *end, text};
}

auto by_start(const SubtitleCue& lhs, const SubtitleCue& rhs) {
    return std::tie(lhs.start_time, lhs.end_time) < std::tie(rhs.start_time, rhs.end_time);
}

}  // namespace

SubtitleParseResult parse_ass_document(const QString& text, const QUrl& source,
                                       SubtitleFormat format) {
    SubtitleParseResult result;
    QStringList event_fields =
        format == SubtitleFormat::ssa ? kDefaultSsaDialogueFields : kDefaultAssDialogueFields;
    QString section;
    QList<SubtitleCue> cues;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (qsizetype line_index = 0; line_index < lines.size(); ++line_index) {
        const QString line = trim_all(lines.at(line_index));
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            section = line.toLower();
            continue;
        }
        if (section != QStringLiteral("[events]")) {
            continue;
        }
        if (starts_with_ci(line, QStringLiteral("Format:"))) {
            const QStringList parsed = parse_ass_fields(line.mid(7));
            if (parsed.contains(QStringLiteral("start")) &&
                parsed.contains(QStringLiteral("end")) && parsed.contains(QStringLiteral("text"))) {
                event_fields = parsed;
            }
            continue;
        }
        if (starts_with_ci(line, QStringLiteral("Dialogue:"))) {
            const auto cue =
                parse_ass_event(trim_all(QStringView(line).mid(9)), event_fields, int(line_index));
            if (cue) {
                cues.append(*cue);
            }
        }
    }
    if (cues.isEmpty()) {
        result.error = SubtitleParseError::no_valid_cues;
        return result;
    }
    std::stable_sort(cues.begin(), cues.end(), by_start);
    result.document = SubtitleDocument{source, format, cues, QStringList{}};
    return result;
}

}  // namespace niratan::player::detail

// SPDX-License-Identifier: GPL-3.0-or-later

#include "srt_parser.h"

#include <QChar>
#include <QLatin1Char>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace niratan::sasayaki {
namespace {

constexpr double kTimestampCap = 359999999.0;
constexpr double kMinimumDuration = 0.001;

// Foundation's .whitespaces is U+0009 plus the Zs category.
bool is_space_character(QChar character) {
    return character == QChar(u'\t') || character.category() == QChar::Separator_Space;
}

// Foundation's .newlines is U+000A..U+000D, U+0085, U+2028 and U+2029.
bool is_newline_character(QChar character) {
    switch (character.unicode()) {
        case 0x000A:
        case 0x000B:
        case 0x000C:
        case 0x000D:
        case 0x0085:
        case 0x2028:
        case 0x2029:
            return true;
        default:
            return false;
    }
}

QString trim_whitespace(QStringView value) {
    qsizetype begin = 0;
    qsizetype end = value.size();
    while (begin < end && is_space_character(value[begin])) {
        ++begin;
    }
    while (end > begin && is_space_character(value[end - 1])) {
        --end;
    }
    return value.mid(begin, end - begin).toString();
}

QString trim_whitespace_and_newlines(QStringView value) {
    const auto is_trimmed = [](QChar character) {
        return is_space_character(character) || is_newline_character(character);
    };

    qsizetype begin = 0;
    qsizetype end = value.size();
    while (begin < end && is_trimmed(value[begin])) {
        ++begin;
    }
    while (end > begin && is_trimmed(value[end - 1])) {
        --end;
    }
    return value.mid(begin, end - begin).toString();
}

// Replaces every line break with one space, dropping the empty parts between
// adjacent breaks, following String.split(whereSeparator:).
QString flatten_newlines(QStringView value) {
    QString flattened;
    flattened.reserve(value.size());

    qsizetype run_start = -1;
    bool has_part = false;
    for (qsizetype index = 0; index < value.size(); ++index) {
        if (is_newline_character(value[index])) {
            if (run_start >= 0) {
                if (has_part) {
                    flattened += u' ';
                }
                flattened += value.mid(run_start, index - run_start);
                has_part = true;
                run_start = -1;
            }
        } else if (run_start < 0) {
            run_start = index;
        }
    }
    if (run_start >= 0) {
        if (has_part) {
            flattened += u' ';
        }
        flattened += value.mid(run_start, value.size() - run_start);
    }
    return flattened;
}

// SasayakiParser collapses a blank line that holds only spaces or tabs, not
// several consecutive newlines; the differences decide which empty blocks the
// split produces and therefore the cue ids.
QString collapse_blank_lines(const QString& value) {
    QString collapsed;
    collapsed.reserve(value.size());
    for (qsizetype index = 0; index < value.size(); ++index) {
        if (value[index] == u'\n') {
            qsizetype next = index + 1;
            while (next < value.size() && (value[next] == u'\t' || value[next] == u' ')) {
                ++next;
            }
            if (next < value.size() && value[next] == u'\n') {
                collapsed += u'\n';
                collapsed += u'\n';
                index = next;
                continue;
            }
        }
        collapsed += value[index];
    }
    return collapsed;
}

// Swift's Double(String) reads the whole token as a decimal number, in any
// locale, with an optional sign, fraction and exponent. Hex spellings and the
// words for infinity and NaN are not read; the caller rejects non-finite sums.
std::optional<double> parse_number(QStringView value) {
    qsizetype index = 0;
    if (index < value.size() && (value[index] == u'+' || value[index] == u'-')) {
        ++index;
    }

    qsizetype digits = 0;
    while (index < value.size() && value[index] >= u'0' && value[index] <= u'9') {
        ++index;
        ++digits;
    }
    if (index < value.size() && value[index] == u'.') {
        ++index;
        while (index < value.size() && value[index] >= u'0' && value[index] <= u'9') {
            ++index;
            ++digits;
        }
    }
    if (digits == 0) {
        return std::nullopt;
    }

    if (index < value.size() && (value[index] == u'e' || value[index] == u'E')) {
        ++index;
        if (index < value.size() && (value[index] == u'+' || value[index] == u'-')) {
            ++index;
        }
        qsizetype exponent_digits = 0;
        while (index < value.size() && value[index] >= u'0' && value[index] <= u'9') {
            ++index;
            ++exponent_digits;
        }
        if (exponent_digits == 0) {
            return std::nullopt;
        }
    }
    if (index != value.size()) {
        return std::nullopt;
    }

    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok || !std::isfinite(number)) {
        return std::nullopt;
    }
    return number;
}

std::optional<double> parse_timestamp(const QString& field) {
    const QStringView value(field);
    qsizetype begin = 0;
    while (begin < value.size() && value[begin].isSpace()) {
        ++begin;
    }
    if (begin == value.size()) {
        return std::nullopt;
    }
    qsizetype end = begin;
    while (end < value.size() && !value[end].isSpace()) {
        ++end;
    }

    QString token = value.mid(begin, end - begin).toString();
    token.replace(u',', u'.');
    const QStringList parts = token.split(u':');
    if (parts.size() != 3) {
        return std::nullopt;
    }

    const std::optional<double> hours = parse_number(parts[0]);
    const std::optional<double> minutes = parse_number(parts[1]);
    const std::optional<double> seconds = parse_number(parts[2]);
    if (!hours || !minutes || !seconds) {
        return std::nullopt;
    }
    if (*hours < 0.0 || *minutes < 0.0 || *minutes >= 60.0 || *seconds < 0.0 ||
        *seconds >= 60.0) {
        return std::nullopt;
    }

    const double total = *hours * 3600.0 + *minutes * 60.0 + *seconds;
    if (!std::isfinite(total)) {
        return std::nullopt;
    }
    return total;
}

QString timestamp(double seconds) {
    const double capped = std::min(seconds, kTimestampCap);
    const auto milliseconds = static_cast<long long>(std::round(capped * 1000.0));
    const long long hours = milliseconds / 3600000;
    const long long minutes = milliseconds / 60000 % 60;
    const long long seconds_part = milliseconds / 1000 % 60;
    const long long remainder = milliseconds % 1000;
    return QStringLiteral("%1:%2:%3,%4")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds_part, 2, 10, QLatin1Char('0'))
        .arg(remainder, 3, 10, QLatin1Char('0'));
}

}  // namespace

std::vector<SasayakiCue> parse_cues(std::string_view text) {
    // String(decoding:as: UTF8.self) substitutes U+FFFD for an invalid sequence,
    // which is what QString::fromUtf8 does as well.
    QString normalized = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    normalized.remove(QChar(0xFEFF));
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(u'\r', u'\n');
    normalized = collapse_blank_lines(normalized);

    std::vector<SasayakiCue> cues;
    const QStringList blocks = normalized.split(QStringLiteral("\n\n"));
    for (qsizetype block_index = 0; block_index < blocks.size(); ++block_index) {
        const QStringList lines = blocks[block_index].split(u'\n');

        qsizetype timing_index = -1;
        for (qsizetype index = 0; index < lines.size(); ++index) {
            if (lines[index].contains(QStringLiteral("-->"))) {
                timing_index = index;
                break;
            }
        }
        if (timing_index < 0 || timing_index + 1 >= lines.size()) {
            continue;
        }

        const QStringList times = lines[timing_index].split(QStringLiteral("-->"));
        if (times.size() != 2) {
            continue;
        }
        const std::optional<double> start = parse_timestamp(times[0]);
        const std::optional<double> end = parse_timestamp(times[1]);
        if (!start || !end || *end <= *start) {
            continue;
        }

        QStringList text_lines;
        for (qsizetype index = timing_index + 1; index < lines.size(); ++index) {
            text_lines.append(trim_whitespace(lines[index]));
        }
        const QString cue_text = trim_whitespace_and_newlines(text_lines.join(u' '));
        if (cue_text.isEmpty()) {
            continue;
        }

        SasayakiCue cue;
        cue.id = QString::number(block_index).toStdString();
        cue.start_time = *start;
        cue.end_time = *end;
        cue.text = cue_text.toStdString();
        cues.push_back(std::move(cue));
    }
    return cues;
}

std::string encode_cues(const std::vector<SasayakiCue>& cues) {
    std::vector<const SasayakiCue*> ordered;
    ordered.reserve(cues.size());
    for (const SasayakiCue& cue : cues) {
        if (!std::isfinite(cue.start_time) || !std::isfinite(cue.end_time) ||
            cue.start_time < 0.0 || !(cue.end_time > cue.start_time)) {
            continue;
        }
        ordered.push_back(&cue);
    }
    // Swift leaves cues with equal starts in an unspecified order.
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const SasayakiCue* left, const SasayakiCue* right) {
                         return left->start_time < right->start_time;
                     });

    QStringList entries;
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        const SasayakiCue& cue = *ordered[index];
        QString entry = QString::number(static_cast<qlonglong>(index + 1));
        entry += u'\n';
        entry += timestamp(cue.start_time);
        entry += QStringLiteral(" --> ");
        entry += timestamp(std::max(cue.end_time, cue.start_time + kMinimumDuration));
        entry += u'\n';
        entry += flatten_newlines(QString::fromStdString(cue.text));
        entry += u'\n';
        entries.append(entry);
    }
    return entries.join(QStringLiteral("\n")).toStdString();
}

}  // namespace niratan::sasayaki

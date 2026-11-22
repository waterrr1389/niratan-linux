// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_presentation.h"

#include <QCoreApplication>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace niratan::viewmodels::reader_presentation {

namespace {

// String(format: "%.<n>f%%"): POSIX formatting, never the host locale.
QString percent(int current, int total, int decimals) {
    const double value = total > 0 ? static_cast<double>(current) / total * 100 : 0;
    return QString::number(value, 'f', decimals) + QLatin1Char('%');
}

}  // namespace

int display_count(int raw_characters, bool english) {
    const int count = std::max(0, raw_characters);
    if (!english) {
        return count;
    }
    if (count <= 0) {
        return 0;
    }
    return static_cast<int>(std::ceil(static_cast<double>(count) / 5.0));
}

int raw_characters(int display_count, bool english) {
    const int count = std::max(0, display_count);
    return english ? count * 5 : count;
}

QString progress_line(int current, int total, std::optional<PageCount> pages,
                      const ProgressLineOptions& options) {
    QStringList parts;
    if (options.shows_page_count) {
        parts.append(pages ? QCoreApplication::translate("ReaderPresentation", "%1 of %2")
                                 .arg(pages->current)
                                 .arg(pages->total)
                           : QStringLiteral("…"));
    } else if (!options.count_off) {
        parts.append(QStringLiteral("%1 / %2")
                         .arg(display_count(current, options.english))
                         .arg(display_count(total, options.english)));
    }
    if (options.show_percentage) {
        parts.append(percent(current, total, 2));
    }
    return parts.join(QLatin1Char(' '));
}

QString progress_string(const ProgressInput& input) {
    QStringList lines;
    if (input.show_book) {
        const auto pages = input.pages
            ? std::optional<PageCount>(PageCount{input.pages->page, input.pages->total})
            : std::nullopt;
        const QString line = progress_line(input.book_current, input.book_total, pages, input.line);
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    if (input.show_chapter) {
        const auto pages = input.pages ? std::optional<PageCount>(PageCount{
                                             input.pages->chapter_page, input.pages->chapter_total})
                                       : std::nullopt;
        const QString line =
            progress_line(input.chapter_current, input.chapter_total, pages, input.line);
        if (!line.isEmpty()) {
            lines.append(QLatin1Char('(') + line + QLatin1Char(')'));
        }
    }
    return lines.join(input.top ? QStringLiteral(" ") : QStringLiteral("\n"));
}

QString statistics_string(bool enabled, bool show_speed, bool show_time, int raw_speed,
                          double reading_seconds, bool english) {
    if (!enabled) {
        return {};
    }
    QStringList parts;
    if (show_speed) {
        parts.append(QString::number(display_count(raw_speed, english)) + QStringLiteral(" / h"));
    }
    if (show_time) {
        // Duration's hourMinute pattern rounds the seconds into minutes with
        // .toNearestOrEven.
        const double minutes = std::nearbyint(std::max(reading_seconds, 0.0) / 60.0);
        const auto total = static_cast<long long>(minutes);
        parts.append(
            QStringLiteral("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QLatin1Char('0')));
    }
    return parts.join(QLatin1Char(' '));
}

QString position_text(int raw_character, int raw_total, bool english) {
    return QStringLiteral("%1 / %2 (%3)")
        .arg(display_count(raw_character, english))
        .arg(display_count(raw_total, english))
        .arg(percent(raw_character, raw_total, 1));
}

std::optional<int> parse_jump_count(const QString& input) {
    qsizetype index = 0;
    bool negative = false;
    if (index < input.size() &&
        (input[index] == QLatin1Char('+') || input[index] == QLatin1Char('-'))) {
        negative = input[index] == QLatin1Char('-');
        ++index;
    }
    if (index == input.size()) {
        return std::nullopt;
    }
    long long value = 0;
    for (; index < input.size(); ++index) {
        const QChar character = input[index];
        if (character < QLatin1Char('0') || character > QLatin1Char('9')) {
            return std::nullopt;
        }
        value = value * 10 + (character.unicode() - u'0');
        if (value > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
    }
    if (negative && value != 0) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

}  // namespace niratan::viewmodels::reader_presentation

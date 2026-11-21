// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_statistics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace niratan::statistics {
namespace {

constexpr int seconds_per_hour = 3600;

QString em_dash() { return QStringLiteral("\u2014"); }

QString display_number(int raw_characters, profiles::ContentLanguage language,
                       const QLocale& locale) {
    const int count = profiles::display_count(language, raw_characters);
    return locale.toString(static_cast<qlonglong>(count));
}

// Duration.seconds(seconds).formatted(.time(pattern: .hourMinuteSecond)):
// whole seconds truncated toward zero, hours unpadded, minutes and seconds two
// digits.
QString duration_text(double seconds) {
    if (!std::isfinite(seconds)) {
        seconds = 0.0;
    }
    const std::int64_t total = static_cast<std::int64_t>(seconds);
    const bool negative = total < 0;
    const std::uint64_t magnitude =
        static_cast<std::uint64_t>(negative ? -total : total);
    const std::uint64_t hours = magnitude / seconds_per_hour;
    const std::uint64_t minutes = (magnitude / 60) % 60;
    const std::uint64_t rest = magnitude % 60;
    const auto two_digits = [](std::uint64_t value) {
        return QStringLiteral("%1").arg(value, 2, 10, QLatin1Char('0'));
    };
    return (negative ? QStringLiteral("-") : QString()) + QString::number(hours) + ":" +
           two_digits(minutes) + ":" + two_digits(rest);
}

QString remaining_text(const ReadingTotal& session, int characters) {
    const double seconds = session.time_to_read(characters);
    return seconds > 0 ? duration_text(seconds) : em_dash();
}

ReaderStatisticsPeriodText period_text(const ReadingTotal& total,
                                       profiles::ContentLanguage language,
                                       const QLocale& locale) {
    return ReaderStatisticsPeriodText{
        display_number(total.characters_read, language, locale),
        display_number(total.reading_speed(), language, locale) + QStringLiteral("/h"),
        duration_text(total.reading_time)};
}

}  // namespace

ReaderStatisticsInspectorText present_reader_statistics(
    const ReadingTotal& session, const ReadingTotal& today, const ReadingTotal& all_time,
    profiles::ContentLanguage language, int book_characters, int current_character,
    int chapter_characters_remaining, const QLocale& locale) {
    ReaderStatisticsInspectorText text;
    text.session = period_text(session, language, locale);
    text.today = period_text(today, language, locale);
    text.all_time = period_text(all_time, language, locale);

    text.book_fraction =
        book_characters > 0
            ? std::clamp(static_cast<double>(current_character) /
                             static_cast<double>(book_characters),
                         0.0, 1.0)
            : 0.0;
    text.book_percent = locale.toString(text.book_fraction * 100.0, 'f', 1) + locale.percent();

    text.book_remaining = remaining_text(session, book_characters - current_character);
    text.chapter_remaining = remaining_text(session, chapter_characters_remaining);
    return text;
}

QVariantMap reader_statistics_map(const ReaderStatisticsInspectorText& text,
                                  profiles::ContentLanguage language) {
    const auto period = [](const ReaderStatisticsPeriodText& value) {
        return QVariantMap{{QStringLiteral("characters"), value.characters},
                           {QStringLiteral("speed"), value.speed},
                           {QStringLiteral("duration"), value.duration}};
    };
    return {{QStringLiteral("english"), language == profiles::ContentLanguage::english},
            {QStringLiteral("session"), period(text.session)},
            {QStringLiteral("today"), period(text.today)},
            {QStringLiteral("allTime"), period(text.all_time)},
            {QStringLiteral("bookPercent"), text.book_percent},
            {QStringLiteral("bookFraction"), text.book_fraction},
            {QStringLiteral("bookRemaining"), text.book_remaining},
            {QStringLiteral("chapterRemaining"), text.chapter_remaining}};
}

}  // namespace niratan::statistics

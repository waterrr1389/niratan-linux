// SPDX-License-Identifier: GPL-3.0-or-later
//
// The text the reader's Statistics inspector shows, following
// Features/Reader/StatisticsView.swift (ReaderStatisticsContentView.count,
// speed, duration, countLabel is the view's, bookProgress, remainingRow,
// historyRow, timeToFinishBook and timeToFinishChapter), the totals in
// NativeMac/NativeReaderView.swift (sessionStatistics, todaysStatistics,
// allTimeStatistics, displayedCharacterCount,
// displayedChapterCharactersRemaining) and Models/Profile.swift
// (ContentLanguageProfile.displayCount(forRawCharacters:)).
//
// This module is pure: the caller passes the three reading totals, the book
// and chapter positions, the content language and a locale, and gets the
// strings the inspector draws. Nothing reads a clock, a file or a tracker.
//
// Units
// -----
// Every character count and every speed goes through display_count() before
// formatting: Japanese keeps the raw count, English divides by five and
// rounds up. A speed is the total's truncating reading_speed (characters per
// hour), so English speeds are approximate words per hour. The formatted
// number uses the locale's group separator and digits; the "/h" suffix is
// literal.
//
// Time text
// ---------
// duration is Duration.seconds(seconds).formatted(.time(pattern:
// .hourMinuteSecond)): hours with at least one digit and no padding, then
// two-digit minutes and seconds. Whole seconds are truncated toward zero, and
// a negative duration keeps its sign.
//
// A remaining time uses the live session's speed, not the period's own speed,
// as timeToFinishBook and timeToFinishChapter do: remaining characters times
// 3600 / speed. A time that is not positive prints an em dash, so a finished
// book, a finished chapter and a session without a pace all read "—" instead
// of "0:00:00". Reading time itself always shows a clock, zero included.
//
// Book progress
// -------------
// The fraction is current / book characters clamped to 0..1, or 0 when the
// book has no character count. Its text has one fraction digit and the
// locale's percent sign, as .percent.precision(.fractionLength(1)) does.
//
// Known deviations from the Swift source
// --------------------------------------
// * Number text uses the locale's digits and group separator, but not its
//   numbering system for non-Latin locales. Duration digits and the ":"
//   separators are ASCII.
// * The percent sign always follows the number. A locale that separates the
//   sign with a space or prefixes it is not reproduced.
// * A non-finite reading time is treated as zero, while Swift's Duration
//   conversion of such a value traps. A tracker cannot produce one.

#pragma once

#include <QLocale>
#include <QString>
#include <QVariantMap>

#include "models/profiles/content_language.h"
#include "models/statistics/statistics.h"

namespace niratan::statistics {

// One half of the History table: the count column, the speed column and the
// duration column of Today or All Time.
struct ReaderStatisticsPeriodText {
    QString characters;
    QString speed;
    QString duration;

    bool operator==(const ReaderStatisticsPeriodText&) const = default;
};

// Everything the inspector derives. Session is the live session alone; the
// remaining times are clock text or an em dash.
struct ReaderStatisticsInspectorText {
    ReaderStatisticsPeriodText session;
    ReaderStatisticsPeriodText today;
    ReaderStatisticsPeriodText all_time;
    QString book_percent;
    double book_fraction = 0.0;
    QString book_remaining;
    QString chapter_remaining;

    bool operator==(const ReaderStatisticsInspectorText&) const = default;
};

// `book_characters` is the book's character total (0 when unknown),
// `current_character` the reading position in the same unit and
// `chapter_characters_remaining` the characters left in the TOC chapter that
// holds the position. Locale decides grouping, decimal separator and the
// percent sign.
ReaderStatisticsInspectorText present_reader_statistics(
    const ReadingTotal& session, const ReadingTotal& today, const ReadingTotal& all_time,
    profiles::ContentLanguage language, int book_characters, int current_character,
    int chapter_characters_remaining, const QLocale& locale);

// The map the reader page binds to. Keys are "english" (whether the profile
// counts words), "session", "today" and "allTime" (each with "characters",
// "speed" and "duration"), "bookPercent", "bookFraction", "bookRemaining" and
// "chapterRemaining". The shape is what ReaderStatisticsInspector.qml reads;
// no text is decided here beyond what present_reader_statistics returns.
QVariantMap reader_statistics_map(const ReaderStatisticsInspectorText& text,
                                  profiles::ContentLanguage language);

}  // namespace niratan::statistics

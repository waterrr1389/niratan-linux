// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader Statistics inspector's text, following
// Features/Reader/StatisticsView.swift (ReaderStatisticsContentView.count,
// speed, duration, bookProgress, remainingRow, historyRow, timeToFinishBook
// and timeToFinishChapter) and Models/Profile.swift
// (ContentLanguageProfile.displayCount(forRawCharacters:)).
//
// Expected strings are written as Swift produces them for an en_US locale:
// grouped thousands with a comma, a dot decimal separator and a trailing
// percent sign. The Japanese and English cases share the locale; only the
// count unit changes.

#include <QLocale>
#include <QTest>

#include "models/statistics/reader_statistics.h"

using niratan::profiles::ContentLanguage;
using niratan::statistics::present_reader_statistics;
using niratan::statistics::reader_statistics_map;
using niratan::statistics::ReaderStatisticsInspectorText;
using niratan::statistics::ReadingTotal;

namespace {

QLocale en_us() { return QLocale(QLocale::English, QLocale::UnitedStates); }

QLocale c_locale() { return QLocale::c(); }

ReaderStatisticsInspectorText present(const ReadingTotal& session, const ReadingTotal& today,
                                      const ReadingTotal& all_time,
                                      ContentLanguage language = ContentLanguage::japanese,
                                      int book_characters = 0, int current_character = 0,
                                      int chapter_characters_remaining = 0,
                                      const QLocale& locale = en_us()) {
    return present_reader_statistics(session, today, all_time, language, book_characters,
                                     current_character, chapter_characters_remaining, locale);
}

}  // namespace

class ReaderStatisticsTest : public QObject {
    Q_OBJECT

   private slots:
    void periods_show_grouped_characters_speed_and_clock();
    void english_counts_five_characters_per_word();
    void speed_is_truncated_before_the_display_unit();
    void grouping_follows_the_locale();
    void durations_have_unpadded_hours_and_whole_seconds();
    void book_progress_is_clamped_and_has_one_fraction_digit();
    void remaining_times_use_the_session_speed();
    void remaining_times_are_dashes_without_a_pace_or_characters();
    void map_exposes_the_text_the_view_binds();
};

void ReaderStatisticsTest::periods_show_grouped_characters_speed_and_clock() {
    const ReadingTotal session{12345, 3600.0};
    const ReaderStatisticsInspectorText text = present(session, ReadingTotal{}, session);

    QCOMPARE(text.session.characters, QStringLiteral("12,345"));
    QCOMPARE(text.session.speed, QStringLiteral("12,345/h"));
    QCOMPARE(text.session.duration, QStringLiteral("1:00:00"));

    QCOMPARE(text.today.characters, QStringLiteral("0"));
    QCOMPARE(text.today.speed, QStringLiteral("0/h"));
    QCOMPARE(text.today.duration, QStringLiteral("0:00:00"));

    QCOMPARE(text.all_time.characters, QStringLiteral("12,345"));
}

void ReaderStatisticsTest::english_counts_five_characters_per_word() {
    const ReadingTotal session{12345, 3600.0};
    const ReaderStatisticsInspectorText text =
        present(session, session, session, ContentLanguage::english);

    QCOMPARE(text.session.characters, QStringLiteral("2,469"));
    QCOMPARE(text.session.speed, QStringLiteral("2,469/h"));
    QCOMPARE(text.today.characters, QStringLiteral("2,469"));

    const ReaderStatisticsInspectorText one_word =
        present(ReadingTotal{6, 3600.0}, ReadingTotal{}, ReadingTotal{},
                ContentLanguage::english);
    QCOMPARE(one_word.session.characters, QStringLiteral("2"));

    const ReaderStatisticsInspectorText zero =
        present(ReadingTotal{}, ReadingTotal{}, ReadingTotal{}, ContentLanguage::english);
    QCOMPARE(zero.session.characters, QStringLiteral("0"));
    QCOMPARE(zero.session.speed, QStringLiteral("0/h"));
}

void ReaderStatisticsTest::speed_is_truncated_before_the_display_unit() {
    // ReadingTotal.speed: int(1000 / 3601 * 3600) == 999.
    const ReaderStatisticsInspectorText text =
        present(ReadingTotal{1000, 3601.0}, ReadingTotal{}, ReadingTotal{});
    QCOMPARE(text.session.speed, QStringLiteral("999/h"));
}

void ReaderStatisticsTest::grouping_follows_the_locale() {
    const ReadingTotal session{12345, 3600.0};
    const ReaderStatisticsInspectorText plain =
        present(session, ReadingTotal{}, ReadingTotal{}, ContentLanguage::japanese, 0, 0, 0,
                c_locale());
    QCOMPARE(plain.session.characters, QStringLiteral("12345"));
    QCOMPARE(plain.session.speed, QStringLiteral("12345/h"));
}

void ReaderStatisticsTest::durations_have_unpadded_hours_and_whole_seconds() {
    const QList<double> seconds{0.0, 59.9, 3600.0, 3661.9, 90061.0};
    const QList<QString> expected{QStringLiteral("0:00:00"), QStringLiteral("0:00:59"),
                                  QStringLiteral("1:00:00"), QStringLiteral("1:01:01"),
                                  QStringLiteral("25:01:01")};
    for (qsizetype index = 0; index < seconds.size(); ++index) {
        const ReaderStatisticsInspectorText text =
            present(ReadingTotal{0, seconds.at(index)}, ReadingTotal{}, ReadingTotal{});
        QCOMPARE(text.session.duration, expected.at(index));
    }
}

void ReaderStatisticsTest::book_progress_is_clamped_and_has_one_fraction_digit() {
    const ReaderStatisticsInspectorText inside =
        present(ReadingTotal{}, ReadingTotal{}, ReadingTotal{}, ContentLanguage::japanese, 1000,
                423);
    QCOMPARE(inside.book_percent, QStringLiteral("42.3%"));
    QCOMPARE(inside.book_fraction, 0.423);

    const ReaderStatisticsInspectorText past =
        present(ReadingTotal{}, ReadingTotal{}, ReadingTotal{}, ContentLanguage::japanese, 1000,
                1500);
    QCOMPARE(past.book_percent, QStringLiteral("100.0%"));
    QCOMPARE(past.book_fraction, 1.0);

    const ReaderStatisticsInspectorText negative =
        present(ReadingTotal{}, ReadingTotal{}, ReadingTotal{}, ContentLanguage::japanese, 1000,
                -100);
    QCOMPARE(negative.book_percent, QStringLiteral("0.0%"));
    QCOMPARE(negative.book_fraction, 0.0);

    const ReaderStatisticsInspectorText without_book =
        present(ReadingTotal{}, ReadingTotal{}, ReadingTotal{}, ContentLanguage::japanese, 0, 100);
    QCOMPARE(without_book.book_percent, QStringLiteral("0.0%"));
    QCOMPARE(without_book.book_fraction, 0.0);
}

void ReaderStatisticsTest::remaining_times_use_the_session_speed() {
    // The live session reads 1000 characters per hour; Today and All Time have
    // no pace and must not change the estimates.
    const ReadingTotal session{1000, 3600.0};
    const ReaderStatisticsInspectorText text =
        present(session, ReadingTotal{}, session, ContentLanguage::japanese, 10000, 1000, 500);

    QCOMPARE(text.book_remaining, QStringLiteral("9:00:00"));
    QCOMPARE(text.chapter_remaining, QStringLiteral("0:30:00"));
}

void ReaderStatisticsTest::remaining_times_are_dashes_without_a_pace_or_characters() {
    const ReaderStatisticsInspectorText no_pace =
        present(ReadingTotal{1000, 0.0}, ReadingTotal{}, ReadingTotal{},
                ContentLanguage::japanese, 10000, 1000, 500);
    QCOMPARE(no_pace.book_remaining, QStringLiteral("—"));
    QCOMPARE(no_pace.chapter_remaining, QStringLiteral("—"));

    const ReaderStatisticsInspectorText finished =
        present(ReadingTotal{1000, 3600.0}, ReadingTotal{}, ReadingTotal{},
                ContentLanguage::japanese, 1000, 1000, 0);
    QCOMPARE(finished.book_remaining, QStringLiteral("—"));
    QCOMPARE(finished.chapter_remaining, QStringLiteral("—"));

    const ReaderStatisticsInspectorText beyond =
        present(ReadingTotal{1000, 3600.0}, ReadingTotal{}, ReadingTotal{},
                ContentLanguage::japanese, 1000, 2000, -10);
    QCOMPARE(beyond.book_remaining, QStringLiteral("—"));
    QCOMPARE(beyond.chapter_remaining, QStringLiteral("—"));
}

void ReaderStatisticsTest::map_exposes_the_text_the_view_binds() {
    const ReadingTotal session{12345, 3600.0};
    const ReaderStatisticsInspectorText text =
        present(session, ReadingTotal{}, session, ContentLanguage::english, 20000, 12345, 500);
    const QVariantMap map = reader_statistics_map(text, ContentLanguage::english);

    QCOMPARE(map.value(QStringLiteral("english")).toBool(), true);
    QCOMPARE(map.value(QStringLiteral("session")).toMap().value(QStringLiteral("characters"))
                 .toString(),
             QStringLiteral("2,469"));
    QCOMPARE(map.value(QStringLiteral("allTime")).toMap().value(QStringLiteral("duration"))
                 .toString(),
             QStringLiteral("1:00:00"));
    QCOMPARE(map.value(QStringLiteral("bookPercent")).toString(), QStringLiteral("61.7%"));
    QCOMPARE(map.value(QStringLiteral("bookFraction")).toDouble(), 0.61725);
    QCOMPARE(map.value(QStringLiteral("bookRemaining")).toString(), QStringLiteral("0:37:12"));
    QCOMPARE(map.value(QStringLiteral("chapterRemaining")).toString(),
             QStringLiteral("0:02:25"));
}

QTEST_APPLESS_MAIN(ReaderStatisticsTest)

#include "tst_reader_statistics.moc"

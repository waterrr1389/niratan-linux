// SPDX-License-Identifier: GPL-3.0-or-later
// Port of script/test_ttu_sync_naming.swift.

#include "services/sync/ttu_naming.h"

#include <QtTest>

using namespace niratan::sync;

class TtuNamingTest : public QObject {
    Q_OBJECT

   private slots:
    void sanitize_data() {
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("encoded");
        for (const auto& [title, encoded] :
             std::initializer_list<std::pair<const char*, const char*>>{
                 {"吾輩は猫である", "吾輩は猫である"},
                 {"a/b", "a%2Fb"},
                 {"Why?", "Why%3F"},
                 {"<tag>", "%3Ctag%3E"},
                 {"C:\\path", "C%3A%5Cpath"},
                 {"a|b", "a%7Cb"},
                 {"100%", "100%25"},
                 {"say \"hi\"", "say %22hi%22"},
                 {"a*b", "a~ttu-star~b"},
                 {"ends with dot.", "ends with dot~ttu-dend~"},
                 {"ends with space ", "ends with space~ttu-spc~"},
                 {"ends with dot and space. ", "ends with dot and space.~ttu-spc~"},
                 {"two dots..", "two dots.~ttu-dend~"},
                 {"two spaces  ", "two spaces ~ttu-spc~"},
             }) {
            QTest::newRow(title) << QString::fromUtf8(title) << QString::fromUtf8(encoded);
        }
    }

    void sanitize() {
        QFETCH(QString, title);
        QFETCH(QString, encoded);
        QCOMPARE(sanitize_ttu_title(title.toStdString()), encoded.toStdString());
    }

    void sanitize_keeps_existing_star_marker() {
        QCOMPARE(sanitize_ttu_title("a~ttu-star~b"), std::string("a~ttu-star~b"));
        QCOMPARE(desanitize_ttu_title("a~ttu-star~b"), std::string("a*b"));
    }

    void round_trip_data() { sanitize_data(); }

    void round_trip() {
        QFETCH(QString, title);
        QFETCH(QString, encoded);
        const auto name = sanitize_ttu_title(title.toStdString());
        QCOMPARE(name, encoded.toStdString());
        QCOMPARE(desanitize_ttu_title(name), title.toStdString());
    }

    void desanitize_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("title");
        QTest::newRow("bad escape") << QString("%2f%~ttu-dend~") << QString("%2f%.");
        QTest::newRow("invalid UTF-8") << QString("%FF%2f") << QString("%FF%2f");
        QTest::newRow("valid UTF-8") << QString("%E6%9C%AC") << QString("本");
        QTest::newRow("escaped marker") << QString("%7Ettu-star%7E") << QString("*");
        QTest::newRow("lowercase hex") << QString("%2f") << QString("/");
    }

    void desanitize() {
        QFETCH(QString, name);
        QFETCH(QString, title);
        QCOMPARE(desanitize_ttu_title(name.toStdString()), title.toStdString());
    }

    void statistics_name() {
        const niratan::statistics::Statistics rows[]{
            {"本", "2026-10-01", 1200, 600, 6000, 5000, 7200, 7200, 111},
            {"本", "2026-10-02", 3000, 1200, 8000, 7000, 9000, 9000, 222},
        };
        QCOMPARE(ttu_statistics_file_name(rows),
                 "statistics_1_6_222_4200_1800.0_6000_5000_8400.0_9000_900.0_1029.0_2100.0_2400.0_"
                 "8400.0_8397.0_na.json");
    }

    void empty_statistics_should_not_divide_by_zero() {
        QCOMPARE(ttu_statistics_file_name({}),
                 "statistics_1_6_0_0_0.0_0_0_0.0_0_0.0_0.0_0.0_0.0_0.0_0.0_na.json");
    }

    void statistics_with_zero_time_day() {
        const niratan::statistics::Statistics rows[]{
            {"本", "2026-10-01", 100, 60, 2000, 1000, 3600, 3600, 100},
            {"本", "2026-10-02", 50, 0, 0, 0, 0, 0, 90},
        };
        QCOMPARE(ttu_statistics_file_name(rows),
                 "statistics_1_6_100_150_60.0_0_0_9000.0_3600_60.0_40.0_150.0_100.0_9000.0_"
                 "9000.0_na.json");
    }

    void statistics_maximum_uses_last_speed() {
        const niratan::statistics::Statistics rows[]{
            {"本", "2026-10-01", 10, 10, 100, 100, 50, 9999, 7},
        };
        QCOMPARE(ttu_statistics_file_name(rows),
                 "statistics_1_6_7_10_10.0_100_100_3600.0_50_10.0_10.0_10.0_10.0_3600.0_"
                 "3600.0_na.json");
    }

    void statistics_minimum_restarts_after_zero() {
        const niratan::statistics::Statistics rows[]{
            {"本", "2026-10-01", 0, 0, 5000, 4000, 0, 0, 1},
            {"本", "2026-10-02", 0, 0, 0, 0, 0, 0, 2},
            {"本", "2026-10-03", 0, 0, 300, 200, 0, 0, 3},
        };
        QCOMPARE(ttu_statistics_file_name(rows),
                 "statistics_1_6_3_0_0.0_300_200_0.0_0_0.0_0.0_0.0_0.0_0.0_0.0_na.json");
    }

    void statistics_fractional_time_round_trips() {
        const niratan::statistics::Statistics rows[]{
            {"本", "2026-10-01", 1, 0.1, 0, 0, 0, 0, 5},
            {"本", "2026-10-02", 1, 0.2, 0, 0, 0, 0, 6},
        };
        QCOMPARE(ttu_statistics_file_name(rows),
                 "statistics_1_6_6_2_0.30000000000000004_0_0_24000.0_0_1.0_0.0_1.0_0.0_3600.0_"
                 "0.0_na.json");
    }

    void statistics_averages_round_up() {
        const niratan::statistics::Statistics rows[]{
            {"本", "2026-10-01", 10, 10, 0, 0, 0, 0, 1},
            {"本", "2026-10-02", 11, 20, 0, 0, 0, 0, 2},
            {"本", "2026-10-03", 10, 10, 0, 0, 0, 0, 3},
        };
        QCOMPARE(ttu_statistics_file_name(rows),
                 "statistics_1_6_3_31_40.0_0_0_2790.0_0_14.0_14.0_11.0_11.0_2829.0_2829.0_na.json");
    }
};

QTEST_APPLESS_MAIN(TtuNamingTest)
#include "tst_ttu_naming.moc"

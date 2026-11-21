// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for GlobalSettings, following UserConfig.init and the didSet
// observers of the statistics keys in Core/UserConfig.swift, and
// StatisticsResetTimePreference.save in Models/Statistics.swift.

#include "services/settings/global_settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "models/statistics/statistics.h"

using niratan::settings::autostart_mode_from_id;
using niratan::settings::GlobalSettings;
using niratan::settings::LoadStatus;
using niratan::settings::StatisticsPreferences;
using niratan::settings::StoreError;
using niratan::statistics::DailyTargetType;
using niratan::statistics::StatisticsAutostartMode;
using niratan::statistics::StatisticsTargetSettings;

namespace {

std::filesystem::path settings_file(const QTemporaryDir& dir) {
    return std::filesystem::path(dir.path().toStdString()) / "settings.json";
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

QJsonObject read_object(const std::filesystem::path& path) {
    return QJsonDocument::fromJson(QByteArray::fromStdString(read_text(path))).object();
}

}  // namespace

class GlobalSettingsTest : public QObject {
    Q_OBJECT

   private slots:
    void autostartModeFromId_data() {
        QTest::addColumn<QString>("id");
        QTest::addColumn<int>("expected");
        QTest::newRow("off") << "Off" << int(StatisticsAutostartMode::off);
        QTest::newRow("page turn") << "Page Turn" << int(StatisticsAutostartMode::page_turn);
        QTest::newRow("on") << "On" << int(StatisticsAutostartMode::on);
        QTest::newRow("lower case") << "on" << int(StatisticsAutostartMode::off);
        QTest::newRow("empty") << "" << int(StatisticsAutostartMode::off);
    }

    void autostartModeFromId() {
        QFETCH(QString, id);
        QFETCH(int, expected);
        QCOMPARE(int(autostart_mode_from_id(id.toStdString())), expected);
    }

    void preferences_missingFile_areDefaults() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto load = GlobalSettings(settings_file(dir)).statistics_preferences();
        QCOMPARE(load.status, LoadStatus::missing);
        QCOMPARE(load.preferences, StatisticsPreferences{});
        QVERIFY(!std::filesystem::exists(settings_file(dir)));
    }

    void preferences_storedValues() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir),
                   R"({"enableStatistics":true,"statisticsAutostartMode":"Page Turn",)"
                   R"("statisticsResetTime":90,"other":1})");
        const auto load = GlobalSettings(settings_file(dir)).statistics_preferences();
        QCOMPARE(load.status, LoadStatus::success);
        QCOMPARE(load.preferences,
                 (StatisticsPreferences{true, StatisticsAutostartMode::page_turn, 90}));
    }

    void preferences_resetTimeIsNormalizedWithoutWriting() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const std::string text = R"({"statisticsResetTime":5000})";
        write_text(settings_file(dir), text);
        const auto load = GlobalSettings(settings_file(dir)).statistics_preferences();
        QCOMPARE(load.preferences.reset_minutes, 1439);
        QCOMPARE(read_text(settings_file(dir)), text);

        write_text(settings_file(dir), R"({"statisticsResetTime":-3})");
        QCOMPARE(GlobalSettings(settings_file(dir)).statistics_preferences().preferences.reset_minutes,
                 0);
    }

    // `as? Bool`, `as? Int` and `String` reads fail on another JSON type, and
    // the UserConfig.init fallback applies.
    void preferences_wrongTypesFallBack() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir),
                   R"({"enableStatistics":"yes","statisticsAutostartMode":1,)"
                   R"("statisticsResetTime":"90"})");
        const auto load = GlobalSettings(settings_file(dir)).statistics_preferences();
        QCOMPARE(load.status, LoadStatus::success);
        QCOMPARE(load.preferences, StatisticsPreferences{});

        write_text(settings_file(dir), R"({"statisticsResetTime":90.5})");
        QCOMPARE(GlobalSettings(settings_file(dir)).statistics_preferences().preferences.reset_minutes,
                 0);
    }

    void preferences_damagedFileIsInvalidAndKept() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir), "{bad json");
        const auto load = GlobalSettings(settings_file(dir)).statistics_preferences();
        QCOMPARE(load.status, LoadStatus::invalid);
        QVERIFY(!load.error.empty());
        QCOMPARE(load.preferences, StatisticsPreferences{});
        QCOMPARE(read_text(settings_file(dir)), std::string("{bad json"));
    }

    void preferences_nonObjectIsInvalid() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir), "[1,2]");
        QCOMPARE(GlobalSettings(settings_file(dir)).statistics_preferences().status,
                 LoadStatus::invalid);
    }

    void targets_missingFile_areDefaults() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const StatisticsTargetSettings targets =
            GlobalSettings(settings_file(dir)).statistics_targets();
        QCOMPARE(targets.daily_target_type, DailyTargetType::characters);
        QCOMPARE(targets.daily_character_target, 5000);
        QCOMPARE(targets.daily_duration_target_minutes, 30);
        QCOMPARE(targets.weekly_target_days, 4);
    }

    void targets_storedValuesAreSnapped() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir),
                   R"({"dailyStatisticsTargetType":"duration",)"
                   R"("dailyStatisticsCharacterTarget":1260,)"
                   R"("dailyStatisticsDurationTargetMinutes":999,)"
                   R"("weeklyStatisticsTargetDays":0})");
        const StatisticsTargetSettings targets =
            GlobalSettings(settings_file(dir)).statistics_targets();
        QCOMPARE(targets.daily_target_type, DailyTargetType::duration);
        QCOMPARE(targets.daily_character_target, StatisticsTargetSettings::snap_character_target(1260));
        QCOMPARE(targets.daily_duration_target_minutes, 240);
        QCOMPARE(targets.weekly_target_days, 1);
    }

    void targets_unknownTypeIsCharacters() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir), R"({"dailyStatisticsTargetType":"Duration"})");
        QCOMPARE(GlobalSettings(settings_file(dir)).statistics_targets().daily_target_type,
                 DailyTargetType::characters);
    }

    void save_createsDocument() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        GlobalSettings settings(settings_file(dir));
        QCOMPARE(settings.save_statistics_enabled(true), StoreError::none);
        QCOMPARE(settings.save_statistics_autostart_mode(StatisticsAutostartMode::on),
                 StoreError::none);
        const QJsonObject object = read_object(settings_file(dir));
        QCOMPARE(object.value(QStringLiteral("enableStatistics")).toBool(), true);
        QCOMPARE(object.value(QStringLiteral("statisticsAutostartMode")).toString(),
                 QStringLiteral("On"));
        QCOMPARE(settings.statistics_preferences().preferences,
                 (StatisticsPreferences{true, StatisticsAutostartMode::on, 0}));
    }

    void save_keepsOtherKeys() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir), R"({"customCSS":"p{}","enableStatistics":false})");
        GlobalSettings settings(settings_file(dir));
        QCOMPARE(settings.save_statistics_enabled(true), StoreError::none);
        const QJsonObject object = read_object(settings_file(dir));
        QCOMPARE(object.value(QStringLiteral("customCSS")).toString(), QStringLiteral("p{}"));
        QCOMPARE(object.value(QStringLiteral("enableStatistics")).toBool(), true);
    }

    // StatisticsResetTimePreference.save normalizes and sets the migration flag.
    void saveResetMinutes_normalizesAndMarksMigrated() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        GlobalSettings settings(settings_file(dir));
        QCOMPARE(settings.save_statistics_reset_minutes(2000), StoreError::none);
        const QJsonObject object = read_object(settings_file(dir));
        QCOMPARE(object.value(QStringLiteral("statisticsResetTime")).toInt(), 1439);
        QCOMPARE(object.value(QStringLiteral("statisticsResetTimeMigratedToMinutes")).toBool(),
                 true);
    }

    void saveTargets_writesClampedValues() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        GlobalSettings settings(settings_file(dir));
        StatisticsTargetSettings targets;
        targets.daily_target_type = DailyTargetType::duration;
        targets.daily_character_target = 100;
        targets.daily_duration_target_minutes = 45;
        targets.weekly_target_days = 9;
        QCOMPARE(settings.save_statistics_targets(targets), StoreError::none);
        const QJsonObject object = read_object(settings_file(dir));
        QCOMPARE(object.value(QStringLiteral("dailyStatisticsTargetType")).toString(),
                 QStringLiteral("duration"));
        QCOMPARE(object.value(QStringLiteral("dailyStatisticsCharacterTarget")).toInt(), 500);
        QCOMPARE(object.value(QStringLiteral("dailyStatisticsDurationTargetMinutes")).toInt(), 45);
        QCOMPARE(object.value(QStringLiteral("weeklyStatisticsTargetDays")).toInt(), 7);
    }

    void save_damagedFileFailsAndKeepsBytes() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir), "{bad json");
        GlobalSettings settings(settings_file(dir));
        QCOMPARE(settings.save_statistics_enabled(true), StoreError::io_error);
        QCOMPARE(settings.save_statistics_targets({}), StoreError::io_error);
        QCOMPARE(read_text(settings_file(dir)), std::string("{bad json"));

        write_text(settings_file(dir), "[1]");
        QCOMPARE(settings.save_statistics_reset_minutes(10), StoreError::io_error);
        QCOMPARE(read_text(settings_file(dir)), std::string("[1]"));
    }
};

QTEST_APPLESS_MAIN(GlobalSettingsTest)
#include "tst_global_settings.moc"

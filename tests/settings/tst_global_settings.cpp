// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for GlobalSettings, following UserConfig.init and the didSet
// observers of the statistics and sasayaki keys in Core/UserConfig.swift,
// ColorHexCodec (sasayaki colours), and StatisticsResetTimePreference.save in
// Models/Statistics.swift.

#include "services/settings/global_settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "models/sasayaki/sasayaki_settings.h"
#include "models/statistics/statistics.h"

using niratan::sasayaki::SasayakiSettings;
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

    // B1, B2, B8: a missing document reads as the UserConfig.init defaults and
    // is not created.
    void sasayaki_missingFileGivesDefaults() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QCOMPARE(GlobalSettings(settings_file(dir)).sasayaki_settings(), SasayakiSettings{});
        QVERIFY(!std::filesystem::exists(settings_file(dir)));
    }

    void sasayaki_storedValues() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QJsonObject stored{
            {"enableSasayaki", true},
            {"sasayakiAutoScroll", false},
            {"sasayakiAutoPause", false},
            {"sasayakiSkipControls", true},
            {"sasayakiTextColor", "#112233"},
            {"sasayakiBackgroundColor", "#44556677"},
            {"sasayakiDarkTextColor", "#FFFFFF"},
            {"sasayakiDarkBackgroundColor", "#00000000"},
            {"other", 1},
        };
        write_text(settings_file(dir),
                   QJsonDocument(stored).toJson(QJsonDocument::Compact).toStdString());
        const SasayakiSettings settings = GlobalSettings(settings_file(dir)).sasayaki_settings();
        QCOMPARE(settings.enable, true);
        QCOMPARE(settings.auto_scroll, false);
        QCOMPARE(settings.auto_pause, false);
        QCOMPARE(settings.skip_controls, true);
        QCOMPARE(QString::fromStdString(settings.text_color), QStringLiteral("#112233"));
        QCOMPARE(QString::fromStdString(settings.background_color), QStringLiteral("#44556677"));
        QCOMPARE(QString::fromStdString(settings.dark_text_color), QStringLiteral("#FFFFFF"));
        QCOMPARE(QString::fromStdString(settings.dark_background_color),
                 QStringLiteral("#00000000"));
    }

    // B1, B2, B3: `as? Bool` and a non-string colour read as absent.
    void sasayaki_wrongTypesFallBack() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir),
                   R"({"enableSasayaki":"true","sasayakiAutoScroll":1,)"
                   R"("sasayakiAutoPause":null,"sasayakiSkipControls":0,)"
                   R"("sasayakiTextColor":12,"sasayakiBackgroundColor":{},)"
                   R"("sasayakiDarkTextColor":true,"sasayakiDarkBackgroundColor":[]})");
        QCOMPARE(GlobalSettings(settings_file(dir)).sasayaki_settings(), SasayakiSettings{});
    }

    // B3, B8: stored colours are normalized on read and the file is not
    // rewritten.
    void sasayaki_colorsAreNormalized() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QJsonObject source{
            {"sasayakiTextColor", "  #abc\n"},
            {"sasayakiBackgroundColor", "#112233FF"},
            {"sasayakiDarkTextColor", "ffff"},
            {"sasayakiDarkBackgroundColor", "1234"},
        };
        const std::string text = QJsonDocument(source).toJson(QJsonDocument::Compact).toStdString();
        write_text(settings_file(dir), text);
        const SasayakiSettings settings = GlobalSettings(settings_file(dir)).sasayaki_settings();
        QCOMPARE(QString::fromStdString(settings.text_color), QStringLiteral("#AABBCC"));
        QCOMPARE(QString::fromStdString(settings.background_color), QStringLiteral("#112233"));
        QCOMPARE(QString::fromStdString(settings.dark_text_color), QStringLiteral("#FFFFFF"));
        QCOMPARE(QString::fromStdString(settings.dark_background_color),
                 QStringLiteral("#11223344"));
        QCOMPARE(read_text(settings_file(dir)), text);

        const QJsonObject invalidColors{
            {"sasayakiTextColor", "nope"},
            {"sasayakiDarkTextColor", "#1234567"},
        };
        write_text(settings_file(dir),
                   QJsonDocument(invalidColors).toJson(QJsonDocument::Compact).toStdString());
        const SasayakiSettings fallback = GlobalSettings(settings_file(dir)).sasayaki_settings();
        QCOMPARE(QString::fromStdString(fallback.text_color), QStringLiteral("#000000"));
        QCOMPARE(QString::fromStdString(fallback.dark_text_color), QStringLiteral("#FFFFFF"));
    }

    // B1-B4, B8: a damaged or non-object document reads as the defaults and is
    // never rewritten.
    void sasayaki_damagedFileGivesDefaultsAndKeepsBytes() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir), "{bad json");
        QCOMPARE(GlobalSettings(settings_file(dir)).sasayaki_settings(), SasayakiSettings{});
        QCOMPARE(read_text(settings_file(dir)), std::string("{bad json"));

        write_text(settings_file(dir), "[1,2]");
        QCOMPARE(GlobalSettings(settings_file(dir)).sasayaki_settings(), SasayakiSettings{});
        QCOMPARE(read_text(settings_file(dir)), std::string("[1,2]"));
    }

    // B6: each toggle writes its own JSON boolean and keeps every other key.
    void saveSasayakiToggles_createDocumentAndKeepOtherKeys() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir), R"({"customCSS":"p{}"})");
        GlobalSettings settings(settings_file(dir));
        QCOMPARE(settings.save_enable_sasayaki(true), StoreError::none);
        QCOMPARE(settings.save_sasayaki_auto_scroll(false), StoreError::none);
        QCOMPARE(settings.save_sasayaki_auto_pause(false), StoreError::none);
        QCOMPARE(settings.save_sasayaki_skip_controls(true), StoreError::none);
        const QJsonObject object = read_object(settings_file(dir));
        QVERIFY(object.value(QStringLiteral("enableSasayaki")).isBool());
        QCOMPARE(object.value(QStringLiteral("enableSasayaki")).toBool(), true);
        QVERIFY(object.value(QStringLiteral("sasayakiAutoScroll")).isBool());
        QCOMPARE(object.value(QStringLiteral("sasayakiAutoScroll")).toBool(), false);
        QVERIFY(object.value(QStringLiteral("sasayakiAutoPause")).isBool());
        QCOMPARE(object.value(QStringLiteral("sasayakiAutoPause")).toBool(), false);
        QVERIFY(object.value(QStringLiteral("sasayakiSkipControls")).isBool());
        QCOMPARE(object.value(QStringLiteral("sasayakiSkipControls")).toBool(), true);
        QCOMPARE(object.value(QStringLiteral("customCSS")).toString(), QStringLiteral("p{}"));
        const SasayakiSettings stored = settings.sasayaki_settings();
        QCOMPARE(stored.enable, true);
        QCOMPARE(stored.auto_scroll, false);
        QCOMPARE(stored.auto_pause, false);
        QCOMPARE(stored.skip_controls, true);
    }

    // B6: a toggle save fails on a damaged or non-object document and leaves
    // its bytes alone.
    void saveSasayakiToggles_damagedFileFailsAndKeepsBytes() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir), "{bad json");
        GlobalSettings settings(settings_file(dir));
        QCOMPARE(settings.save_enable_sasayaki(true), StoreError::io_error);
        QCOMPARE(settings.save_sasayaki_auto_scroll(true), StoreError::io_error);
        QCOMPARE(settings.save_sasayaki_auto_pause(true), StoreError::io_error);
        QCOMPARE(settings.save_sasayaki_skip_controls(true), StoreError::io_error);
        QCOMPARE(read_text(settings_file(dir)), std::string("{bad json"));

        write_text(settings_file(dir), "[1]");
        QCOMPARE(settings.save_enable_sasayaki(true), StoreError::io_error);
        QCOMPARE(read_text(settings_file(dir)), std::string("[1]"));
    }

    // B7: a colour save writes the normalized value, not its input, and keeps
    // every other key.
    void saveSasayakiColors_writeNormalizedValues() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        write_text(settings_file(dir), R"({"customCSS":"p{}"})");
        GlobalSettings settings(settings_file(dir));
        QCOMPARE(settings.save_sasayaki_text_color("  #abc\n"), StoreError::none);
        QCOMPARE(settings.save_sasayaki_background_color("#112233FF"), StoreError::none);
        QCOMPARE(settings.save_sasayaki_dark_text_color("ffff"), StoreError::none);
        QCOMPARE(settings.save_sasayaki_dark_background_color("#00000000"), StoreError::none);
        const QJsonObject object = read_object(settings_file(dir));
        QCOMPARE(object.value(QStringLiteral("sasayakiTextColor")).toString(),
                 QStringLiteral("#AABBCC"));
        QCOMPARE(object.value(QStringLiteral("sasayakiBackgroundColor")).toString(),
                 QStringLiteral("#112233"));
        QCOMPARE(object.value(QStringLiteral("sasayakiDarkTextColor")).toString(),
                 QStringLiteral("#FFFFFF"));
        QCOMPARE(object.value(QStringLiteral("sasayakiDarkBackgroundColor")).toString(),
                 QStringLiteral("#00000000"));
        QCOMPARE(object.value(QStringLiteral("customCSS")).toString(), QStringLiteral("p{}"));
        QCOMPARE(settings.sasayaki_settings(),
                 (SasayakiSettings{false, true, true, false, "#AABBCC", "#112233", "#FFFFFF",
                                   "#00000000"}));
    }

    // B7: an input that does not decode writes nothing and fails.
    void saveSasayakiColors_rejectUndecodableValues() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const std::string text = R"({"customCSS":"p{}"})";
        write_text(settings_file(dir), text);
        GlobalSettings settings(settings_file(dir));
        QCOMPARE(settings.save_sasayaki_text_color("nope"), StoreError::io_error);
        QCOMPARE(settings.save_sasayaki_background_color("#12345"), StoreError::io_error);
        QCOMPARE(settings.save_sasayaki_dark_text_color("#GGGGGG"), StoreError::io_error);
        QCOMPARE(settings.save_sasayaki_dark_background_color(""), StoreError::io_error);
        QCOMPARE(read_text(settings_file(dir)), text);

        QTemporaryDir empty;
        QVERIFY(empty.isValid());
        GlobalSettings fresh(settings_file(empty));
        QCOMPARE(fresh.save_sasayaki_text_color("#1234567"), StoreError::io_error);
        QVERIFY(!std::filesystem::exists(settings_file(empty)));
    }
};

QTEST_APPLESS_MAIN(GlobalSettingsTest)
#include "tst_global_settings.moc"

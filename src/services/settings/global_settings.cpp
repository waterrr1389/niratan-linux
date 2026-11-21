// SPDX-License-Identifier: GPL-3.0-or-later

#include "global_settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <optional>
#include <string>
#include <utility>

#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

namespace niratan::settings {
namespace {

using statistics::DailyTargetType;
using statistics::StatisticsAutostartMode;
using statistics::StatisticsTargetSettings;

const auto kEnableStatistics = QStringLiteral("enableStatistics");
const auto kAutostartMode = QStringLiteral("statisticsAutostartMode");
const auto kResetTime = QStringLiteral("statisticsResetTime");
const auto kResetMigrated = QStringLiteral("statisticsResetTimeMigratedToMinutes");
const auto kTargetType = QStringLiteral("dailyStatisticsTargetType");
const auto kCharacterTarget = QStringLiteral("dailyStatisticsCharacterTarget");
const auto kDurationTarget = QStringLiteral("dailyStatisticsDurationTargetMinutes");
const auto kWeeklyDays = QStringLiteral("weeklyStatisticsTargetDays");

QString id_text(std::string_view id) { return QString::fromUtf8(id.data(), qsizetype(id.size())); }

struct DocumentRead {
    LoadStatus status = LoadStatus::missing;
    QJsonObject object;
    std::string error;
};

DocumentRead read_document(const std::filesystem::path& file) {
    const storage::JsonReadResult read = storage::read_json_file(file);
    DocumentRead result;
    if (read.status == storage::JsonReadStatus::missing) {
        return result;
    }
    if (read.status == storage::JsonReadStatus::invalid) {
        result.status = LoadStatus::invalid;
        result.error = read.error;
        return result;
    }
    if (!read.document.isObject()) {
        result.status = LoadStatus::invalid;
        result.error = "expected a JSON object";
        return result;
    }
    result.status = LoadStatus::success;
    result.object = read.document.object();
    return result;
}

// `as? Int`: only an integral number decodes.
std::optional<int> int_value(const QJsonObject& object, const QString& key) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    const int integer = value.toInt();
    if (double(integer) != number) {
        return std::nullopt;
    }
    return integer;
}

}  // namespace

StatisticsAutostartMode autostart_mode_from_id(std::string_view id) {
    for (const auto mode : {StatisticsAutostartMode::page_turn, StatisticsAutostartMode::on}) {
        if (id == statistics::autostart_mode_id(mode)) {
            return mode;
        }
    }
    return StatisticsAutostartMode::off;
}

GlobalSettings GlobalSettings::at_data_root() {
    return GlobalSettings(storage::data_root() / "settings.json");
}

GlobalSettings::GlobalSettings(std::filesystem::path file) : file_(std::move(file)) {}

StatisticsPreferencesLoad GlobalSettings::statistics_preferences() const {
    const DocumentRead read = read_document(file_);
    StatisticsPreferencesLoad load;
    load.status = read.status;
    load.error = read.error;
    const QJsonObject& object = read.object;
    load.preferences.enabled = object.value(kEnableStatistics).toBool(false);
    load.preferences.autostart_mode =
        autostart_mode_from_id(object.value(kAutostartMode).toString().toStdString());
    load.preferences.reset_minutes =
        statistics::normalized_reset_minutes(int_value(object, kResetTime).value_or(0));
    return load;
}

StatisticsTargetSettings GlobalSettings::statistics_targets() const {
    const QJsonObject object = read_document(file_).object;
    StatisticsTargetSettings targets;
    if (object.value(kTargetType).toString() ==
        id_text(statistics::daily_target_type_id(DailyTargetType::duration))) {
        targets.daily_target_type = DailyTargetType::duration;
    }
    targets.daily_character_target =
        int_value(object, kCharacterTarget).value_or(targets.daily_character_target);
    targets.daily_duration_target_minutes =
        int_value(object, kDurationTarget).value_or(targets.daily_duration_target_minutes);
    targets.weekly_target_days =
        int_value(object, kWeeklyDays).value_or(targets.weekly_target_days);
    return targets.clamped();
}

StoreError GlobalSettings::save_statistics_enabled(bool enabled) {
    return write(QJsonObject{{kEnableStatistics, enabled}});
}

StoreError GlobalSettings::save_statistics_autostart_mode(StatisticsAutostartMode mode) {
    return write(QJsonObject{{kAutostartMode, id_text(statistics::autostart_mode_id(mode))}});
}

StoreError GlobalSettings::save_statistics_reset_minutes(int minutes) {
    return write(QJsonObject{{kResetTime, statistics::normalized_reset_minutes(minutes)},
                             {kResetMigrated, true}});
}

StoreError GlobalSettings::save_statistics_targets(const StatisticsTargetSettings& targets) {
    const StatisticsTargetSettings clamped = targets.clamped();
    return write(QJsonObject{
        {kTargetType, id_text(statistics::daily_target_type_id(clamped.daily_target_type))},
        {kCharacterTarget, clamped.daily_character_target},
        {kDurationTarget, clamped.daily_duration_target_minutes},
        {kWeeklyDays, clamped.weekly_target_days},
    });
}

StoreError GlobalSettings::write(const QJsonObject& values) {
    DocumentRead read = read_document(file_);
    if (read.status == LoadStatus::invalid) {
        return StoreError::io_error;
    }
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        read.object.insert(it.key(), it.value());
    }
    return storage::write_json_file(file_, QJsonDocument(read.object)) ? StoreError::none
                                                                         : StoreError::io_error;
}

}  // namespace niratan::settings

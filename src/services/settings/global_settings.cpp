// SPDX-License-Identifier: GPL-3.0-or-later

#include "global_settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QUrl>

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
const auto kEnableSasayaki = QStringLiteral("enableSasayaki");
const auto kSasayakiAutoScroll = QStringLiteral("sasayakiAutoScroll");
const auto kSasayakiAutoPause = QStringLiteral("sasayakiAutoPause");
const auto kSasayakiSkipControls = QStringLiteral("sasayakiSkipControls");
const auto kSasayakiTextColor = QStringLiteral("sasayakiTextColor");
const auto kSasayakiBackgroundColor = QStringLiteral("sasayakiBackgroundColor");
const auto kSasayakiDarkTextColor = QStringLiteral("sasayakiDarkTextColor");
const auto kSasayakiDarkBackgroundColor = QStringLiteral("sasayakiDarkBackgroundColor");
const auto kPreferredAudioSource = QStringLiteral("preferredAudioSource");

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

// UserConfig.loadColor: a stored string is normalized with ColorHexCodec; a
// missing key or any other JSON type gives the key's default.
std::string color_value(const QJsonObject& object, const QString& key,
                        const std::string& fallback) {
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return fallback;
    }
    return sasayaki::sasayaki_color_hex(value.toString().toStdString()).value_or(fallback);
}

// The value a colour save writes: the codec's normalization of the input, or
// nothing when it does not decode, in which case the save fails.
std::optional<QString> normalized_color(std::string_view hex) {
    const std::optional<std::string> normalized = sasayaki::sasayaki_color_hex(hex);
    if (!normalized.has_value()) {
        return std::nullopt;
    }
    return QString::fromStdString(*normalized);
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
    const auto root = storage::data_root();
    // An empty root means HOME is unavailable; disable storage rather than
    // reading and writing a relative path in the process working directory,
    // matching the same guard in PlaybackHistory::at_data_root.
    return GlobalSettings(root.empty() ? std::filesystem::path{} : root / "settings.json");
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

sasayaki::SasayakiSettings GlobalSettings::sasayaki_settings() const {
    const QJsonObject object = read_document(file_).object;
    sasayaki::SasayakiSettings settings;
    settings.enable = object.value(kEnableSasayaki).toBool(settings.enable);
    settings.auto_scroll = object.value(kSasayakiAutoScroll).toBool(settings.auto_scroll);
    settings.auto_pause = object.value(kSasayakiAutoPause).toBool(settings.auto_pause);
    settings.skip_controls = object.value(kSasayakiSkipControls).toBool(settings.skip_controls);
    settings.text_color = color_value(object, kSasayakiTextColor, settings.text_color);
    settings.background_color =
        color_value(object, kSasayakiBackgroundColor, settings.background_color);
    settings.dark_text_color =
        color_value(object, kSasayakiDarkTextColor, settings.dark_text_color);
    settings.dark_background_color =
        color_value(object, kSasayakiDarkBackgroundColor, settings.dark_background_color);
    return settings;
}

StoreError GlobalSettings::save_enable_sasayaki(bool enabled) {
    return write(QJsonObject{{kEnableSasayaki, enabled}});
}

StoreError GlobalSettings::save_sasayaki_auto_scroll(bool enabled) {
    return write(QJsonObject{{kSasayakiAutoScroll, enabled}});
}

StoreError GlobalSettings::save_sasayaki_auto_pause(bool enabled) {
    return write(QJsonObject{{kSasayakiAutoPause, enabled}});
}

StoreError GlobalSettings::save_sasayaki_skip_controls(bool enabled) {
    return write(QJsonObject{{kSasayakiSkipControls, enabled}});
}

StoreError GlobalSettings::save_sasayaki_text_color(std::string_view hex) {
    const std::optional<QString> normalized = normalized_color(hex);
    if (!normalized.has_value()) {
        return StoreError::io_error;
    }
    return write(QJsonObject{{kSasayakiTextColor, *normalized}});
}

StoreError GlobalSettings::save_sasayaki_background_color(std::string_view hex) {
    const std::optional<QString> normalized = normalized_color(hex);
    if (!normalized.has_value()) {
        return StoreError::io_error;
    }
    return write(QJsonObject{{kSasayakiBackgroundColor, *normalized}});
}

StoreError GlobalSettings::save_sasayaki_dark_text_color(std::string_view hex) {
    const std::optional<QString> normalized = normalized_color(hex);
    if (!normalized.has_value()) {
        return StoreError::io_error;
    }
    return write(QJsonObject{{kSasayakiDarkTextColor, *normalized}});
}

StoreError GlobalSettings::save_sasayaki_dark_background_color(std::string_view hex) {
    const std::optional<QString> normalized = normalized_color(hex);
    if (!normalized.has_value()) {
        return StoreError::io_error;
    }
    return write(QJsonObject{{kSasayakiDarkBackgroundColor, *normalized}});
}

std::string GlobalSettings::preferred_audio_source() const {
    const auto source = read_document(file_).object.value(kPreferredAudioSource).toString();
    const auto text = source.toStdString();
    return valid_preferred_audio_source(text) ? text : std::string{};
}

bool GlobalSettings::valid_preferred_audio_source(std::string_view source) {
    if (source.empty()) {
        return true;
    }
    if (!source.contains("{term}")) {
        return false;
    }
    QString expanded = id_text(source);
    expanded.replace(QStringLiteral("{term}"), QStringLiteral("word"));
    expanded.replace(QStringLiteral("{reading}"), QStringLiteral("reading"));
    const QUrl url(expanded, QUrl::StrictMode);
    if (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")) {
        return false;
    }
    return url.isValid() && !url.host().isEmpty() && url.userInfo().isEmpty() && !url.hasFragment();
}

StoreError GlobalSettings::save_preferred_audio_source(std::string_view source) {
    if (!valid_preferred_audio_source(source)) {
        return StoreError::io_error;
    }
    return write(QJsonObject{{kPreferredAudioSource, id_text(source)}});
}

StoreError GlobalSettings::write(const QJsonObject& values) {
    if (file_.empty()) {
        return StoreError::io_error;
    }
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

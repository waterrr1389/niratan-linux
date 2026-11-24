// SPDX-License-Identifier: GPL-3.0-or-later
//
// The global settings document, <data root>/settings.json, following the
// UserDefaults reads and writes in Core/UserConfig.swift (UserConfig.init and
// the didSet observers of enableStatistics, statisticsAutostartMode,
// statisticsResetTime, dailyStatisticsTargetType,
// dailyStatisticsCharacterTarget, dailyStatisticsDurationTargetMinutes and
// weeklyStatisticsTargetDays, the four sasayaki booleans and the four sasayaki
// colours) and StatisticsResetTimePreference.load/save in
// Models/Statistics.swift.
//
// This is the only code that reads or writes the statistics keys of
// settings.json; the video keys in the same document belong to
// player/video_settings.h, which follows the same merge and damaged-document
// rules. Each key is decoded here once, with UserConfig.init's fallback, so
// every caller sees the same value:
//
//   enableStatistics                      bool, default false
//   statisticsAutostartMode               "Off" | "Page Turn" | "On", default
//                                         "Off"; any other string is "Off"
//   statisticsResetTime                   int minutes, normalized to 0..1439,
//                                         default 0
//   dailyStatisticsTargetType             "characters" | "duration", default
//                                         "characters"
//   dailyStatisticsCharacterTarget        int, default 5000, snapped
//   dailyStatisticsDurationTargetMinutes  int, default 30, snapped
//   weeklyStatisticsTargetDays            int, default 4, clamped to 1..7
//   enableSasayaki                        bool, default false
//   sasayakiAutoScroll                    bool, default true
//   sasayakiAutoPause                     bool, default true
//   sasayakiSkipControls                  bool, default false
//   sasayakiTextColor                     hex string, default "#000000"
//   sasayakiBackgroundColor               hex string, default "#87CFFA66"
//   sasayakiDarkTextColor                 hex string, default "#FFFFFF"
//   sasayakiDarkBackgroundColor           hex string, default "#87CFFA66"
//
// The colour keys hold strings; sasayaki_settings() normalizes them with
// sasayaki::sasayaki_color_hex and applies the key's default when one does not
// decode. A colour save writes the normalized value of its input, not the
// input string; an input that does not decode writes nothing and fails. That
// rejection has no Swift counterpart, where a Color always encodes.
//
// A key holding the wrong JSON type reads as absent, as `as? Int` and
// `as? Bool` do. A missing document reads as the defaults. A damaged document,
// or one that is not a JSON object, reads as the defaults with status invalid
// and is never rewritten: every write fails until the user repairs it. A
// write replaces only the keys it names and keeps every other key; the whole
// document is written atomically.
//
// Reads never write. Swift's load rewrites a reset time whose normalized
// value differs and migrates an unflagged value from hours to minutes. No
// Linux build ever stored hours, so an unflagged value is read as minutes and
// nothing is migrated; saving the reset time still sets
// statisticsResetTimeMigratedToMinutes, as StatisticsResetTimePreference.save
// does.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "models/sasayaki/sasayaki_settings.h"
#include "models/statistics/statistics.h"
#include "services/settings/settings_status.h"

class QJsonObject;

namespace niratan::settings {

struct StatisticsPreferences {
    bool enabled = false;
    statistics::StatisticsAutostartMode autostart_mode = statistics::StatisticsAutostartMode::off;
    int reset_minutes = 0;

    bool operator==(const StatisticsPreferences&) const = default;
};

struct StatisticsPreferencesLoad {
    LoadStatus status = LoadStatus::missing;
    StatisticsPreferences preferences;
    // Empty unless status is invalid.
    std::string error;
};

// The persisted autostart raw value; an unknown value is off, as
// `StatisticsAutostartMode(rawValue:) ?? .off`.
statistics::StatisticsAutostartMode autostart_mode_from_id(std::string_view id);

enum class SyncMode { automatic, manual };
enum class SyncProvider { gdrive, ttu };
std::string_view sync_mode_id(SyncMode mode);
std::string_view sync_provider_id(SyncProvider provider);

// UserConfig.init and its sync-key didSet observers.
struct SyncPreferences {
    bool enabled = false;
    SyncMode mode = SyncMode::automatic;
    bool auto_sync = false;
    std::string google_client_id;
    SyncProvider provider = SyncProvider::gdrive;
    bool upload_books = true;
    bool statistics_sync = false;
    statistics::StatisticsSyncMode statistics_mode = statistics::StatisticsSyncMode::merge;
    bool sasayaki_sync = false;

    bool operator==(const SyncPreferences&) const = default;
};

class GlobalSettings {
   public:
    // <data root>/settings.json under storage::data_root().
    static GlobalSettings at_data_root();

    explicit GlobalSettings(std::filesystem::path file);

    StatisticsPreferencesLoad statistics_preferences() const;
    // Always clamped; a missing or damaged document gives the defaults.
    statistics::StatisticsTargetSettings statistics_targets() const;

    StoreError save_statistics_enabled(bool enabled);
    StoreError save_statistics_autostart_mode(statistics::StatisticsAutostartMode mode);
    // Normalizes before writing and sets the minutes migration flag.
    StoreError save_statistics_reset_minutes(int minutes);
    // Clamps before writing; the four target keys are written together.
    StoreError save_statistics_targets(const statistics::StatisticsTargetSettings& targets);

    // Always the B1-B4 defaults of a missing or damaged document.
    sasayaki::SasayakiSettings sasayaki_settings() const;
    StoreError save_enable_sasayaki(bool enabled);
    StoreError save_sasayaki_auto_scroll(bool enabled);
    StoreError save_sasayaki_auto_pause(bool enabled);
    StoreError save_sasayaki_skip_controls(bool enabled);
    // Writes the normalized hex value of `hex`; a value that does not decode
    // is rejected without writing.
    StoreError save_sasayaki_text_color(std::string_view hex);
    StoreError save_sasayaki_background_color(std::string_view hex);
    StoreError save_sasayaki_dark_text_color(std::string_view hex);
    StoreError save_sasayaki_dark_background_color(std::string_view hex);

    // One optional Yomitan JSON source, tried before the built-in source.
    // Empty disables it. Nonempty templates need HTTP(S), a host and {term}.
    std::string preferred_audio_source() const;
    static bool valid_preferred_audio_source(std::string_view source);
    StoreError save_preferred_audio_source(std::string_view source);
    SyncPreferences sync_preferences() const;
    StoreError save_sync_enabled(bool enabled);
    StoreError save_sync_mode(SyncMode mode);
    StoreError save_auto_sync(bool enabled);
    StoreError save_google_client_id(std::string_view id);
    StoreError save_sync_provider(SyncProvider provider);
    StoreError save_sync_upload_books(bool enabled);
    StoreError save_statistics_enable_sync(bool enabled);
    StoreError save_statistics_sync_mode(statistics::StatisticsSyncMode mode);
    StoreError save_sasayaki_enable_sync(bool enabled);

   private:
    StoreError write(const QJsonObject& values);

    std::filesystem::path file_;
};

}  // namespace niratan::settings

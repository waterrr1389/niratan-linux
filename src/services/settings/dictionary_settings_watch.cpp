// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_settings_watch.h"

#include <QFileInfo>
#include <QString>
#include <QStringList>

#include <fstream>
#include <iterator>
#include <utility>

#include "models/profiles/content_language.h"
#include "models/profiles/profile_index.h"
#include "models/profiles/profile_index_json.h"
#include "models/profiles/profile_ops.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

namespace niratan::settings {
namespace {

// The nearest existing directory, so that a file that does not exist yet is
// noticed when it is created.
QString existing_directory(std::filesystem::path directory) {
    while (!directory.empty()) {
        const auto name = QString::fromStdString(directory.string());
        if (QFileInfo(name).isDir()) {
            return name;
        }
        const auto parent = directory.parent_path();
        if (parent == directory) {
            break;
        }
        directory = parent;
    }
    return {};
}

// The file bytes; empty for a missing or unreadable file, which is how the
// popup behaves without either file.
std::string read_bytes(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return {};
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

}  // namespace

DictionarySettingsWatch::DictionarySettingsWatch(QObject* parent)
    : DictionarySettingsWatch(storage::data_root(), parent) {}

DictionarySettingsWatch::DictionarySettingsWatch(std::filesystem::path data_root, QObject* parent)
    : QObject(parent), data_root_(std::move(data_root)) {
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this,
            &DictionarySettingsWatch::on_path_changed);
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this,
            &DictionarySettingsWatch::on_path_changed);
}

bool DictionarySettingsWatch::reload() {
    if (data_root_.empty()) {
        return false;
    }

    const QStringList previous = watcher_.files() + watcher_.directories();
    QStringList watched;
    const auto watch = [&](const QString& path) {
        if (!path.isEmpty() && !watched.contains(path)) {
            watched.append(path);
            if (!previous.contains(path) && QFileInfo::exists(path)) {
                watcher_.addPath(path);
            }
        }
    };

    const auto profiles_dir = data_root_ / "Profiles";
    const auto index_path = profiles_dir / "profiles.json";
    watch(existing_directory(profiles_dir));
    watch(QString::fromStdString(index_path.string()));
    const auto index_file = storage::read_json_file(index_path);
    const auto index = profiles::profile_index_from_json(index_file.document);
    const auto profile = profiles::resolve_profile(index ? profiles::normalize_profile_index(*index)
                                                         : profiles::ProfileIndex::initial());
    const auto language = profiles::language_id(profile.language());

    std::filesystem::path settings_file;
    std::filesystem::path config_file;
    std::filesystem::path collapsed_file;
    std::string config_bytes;
    std::string collapsed_bytes;
    auto settings = profiles::default_dictionary_settings();
    if (profiles::is_safe_profile_id(profile.id)) {
        const auto profile_dir = profiles_dir / profile.id;
        settings_file = profile_dir / "dictionary_settings.json";
        config_file = profile_dir / "dictionary_config.json";
        collapsed_file = profile_dir / "collapsed.json";
        watch(existing_directory(profile_dir));
        watch(QString::fromStdString(settings_file.string()));
        watch(QString::fromStdString(config_file.string()));
        watch(QString::fromStdString(collapsed_file.string()));
        const auto read = storage::read_json_file(settings_file);
        if (const auto decoded = profiles::dictionary_settings_from_json(read.document)) {
            settings = profiles::resolved_dictionary_settings(*decoded);
        } else if (read.status != storage::JsonReadStatus::missing &&
                   settings_file == settings_file_) {
            settings = settings_;
        }
        config_bytes = read_bytes(config_file);
        collapsed_bytes = read_bytes(collapsed_file);
    }
    for (const auto& path : previous) {
        if (!watched.contains(path)) {
            watcher_.removePath(path);
        }
    }

    const bool changed = settings_file != settings_file_ || settings != settings_ ||
        language != language_id_ || config_file != config_file_ ||
        collapsed_file != collapsed_file_ || config_bytes != config_bytes_ ||
        collapsed_bytes != collapsed_bytes_;
    settings_file_ = std::move(settings_file);
    config_file_ = std::move(config_file);
    collapsed_file_ = std::move(collapsed_file);
    config_bytes_ = std::move(config_bytes);
    collapsed_bytes_ = std::move(collapsed_bytes);
    settings_ = std::move(settings);
    language_id_ = language;
    return changed;
}

void DictionarySettingsWatch::on_path_changed() {
    if (reload()) {
        emit changed();
    }
}

}  // namespace niratan::settings

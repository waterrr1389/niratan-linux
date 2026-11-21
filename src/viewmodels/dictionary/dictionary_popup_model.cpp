// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_popup_model.h"

#include <QColor>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "models/dictionary/audio_source.h"
#include "models/profiles/dictionary_settings.h"
#include "models/profiles/profile_index_json.h"
#include "models/profiles/profile_ops.h"
#include "services/dictionary/dictionary_query.h"
#include "services/dictionary/dictionary_store.h"
#include "services/dictionary/popup_assets.h"
#include "services/dictionary/popup_entries.h"
#include "services/dictionary/popup_html.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

namespace niratan::viewmodels {
namespace {

// A directory watch survives atomic replacement of files and detects creation
// when the file (or its containing directory) does not exist yet.
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

}  // namespace

using niratan::dictionary::DictionaryQueryService;
using niratan::dictionary::DictionaryStore;
using niratan::dictionary::LookupOutcome;
using niratan::dictionary::PopupAssets;
using niratan::dictionary::PopupConfig;
using niratan::dictionary::load_popup_assets;
using niratan::dictionary::lookup_entries_json;
using niratan::dictionary::popup_shell_html;

struct DictionaryPopupModel::State {
    std::vector<std::filesystem::path> dictionaries;
    PopupAssets assets;
    std::unique_ptr<DictionaryQueryService> query;
    QFileSystemWatcher profile_watcher;
    std::filesystem::path settings_path;
    profiles::DictionaryProfileSettings settings = profiles::default_dictionary_settings();
    std::string_view language_id = "ja";
    QString last_query;
    QString html;
    QString error;
    bool dark = false;
    QColor background = QColor(QStringLiteral("#FFFFFF"));
    QColor text_color = QColor(QStringLiteral("#000000"));
};

DictionaryPopupModel::DictionaryPopupModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    state_->assets = load_popup_assets();
    const DictionaryStore store(storage::dictionaries_root());
    state_->dictionaries = store.installed();
    const auto reload = [this] {
        if (reload_profile_settings() && !state_->last_query.isEmpty()) {
            run_lookup();
        }
    };
    connect(&state_->profile_watcher, &QFileSystemWatcher::fileChanged, this, reload);
    connect(&state_->profile_watcher, &QFileSystemWatcher::directoryChanged, this, reload);
    reload_profile_settings();
}

DictionaryPopupModel::~DictionaryPopupModel() = default;

QString DictionaryPopupModel::html() const {
    return state_->html;
}

QString DictionaryPopupModel::error() const {
    return state_->error;
}

bool DictionaryPopupModel::has_results() const {
    return !state_->html.isEmpty();
}

bool DictionaryPopupModel::dark() const {
    return state_->dark;
}

void DictionaryPopupModel::setDark(bool dark) {
    if (state_->dark == dark) {
        return;
    }
    state_->dark = dark;
    emit themeChanged();
}

QColor DictionaryPopupModel::background() const {
    return state_->background;
}

void DictionaryPopupModel::setBackground(QColor background) {
    if (state_->background == background) {
        return;
    }
    state_->background = background;
    emit themeChanged();
}

QColor DictionaryPopupModel::textColor() const {
    return state_->text_color;
}

void DictionaryPopupModel::setTextColor(QColor text_color) {
    if (state_->text_color == text_color) {
        return;
    }
    state_->text_color = text_color;
    emit themeChanged();
}

bool DictionaryPopupModel::reload_profile_settings() {
    const auto root = storage::data_root();
    if (root.empty()) {
        return false;
    }

    auto& watcher = state_->profile_watcher;
    const QStringList previous = watcher.files() + watcher.directories();
    QStringList watched;
    const auto watch = [&](const QString& path) {
        if (!path.isEmpty() && !watched.contains(path)) {
            watched.append(path);
            if (!previous.contains(path) && QFileInfo::exists(path)) {
                watcher.addPath(path);
            }
        }
    };

    // Resolve the same index as ProfileRepository without constructing it:
    // construction can rewrite the index, which would notify this watcher.
    const auto profiles_dir = root / "Profiles";
    const auto index_path = profiles_dir / "profiles.json";
    watch(existing_directory(profiles_dir));
    watch(QString::fromStdString(index_path.string()));
    const auto index_file = storage::read_json_file(index_path);
    const auto index = profiles::profile_index_from_json(index_file.document);
    const auto profile = profiles::resolve_profile(
        index ? profiles::normalize_profile_index(*index) : profiles::ProfileIndex::initial());
    const auto language = profiles::language_id(profile.language());

    std::filesystem::path settings_path;
    auto settings = profiles::default_dictionary_settings();
    if (profiles::is_safe_profile_id(profile.id)) {
        const auto profile_dir = profiles_dir / profile.id;
        settings_path = profile_dir / "dictionary_settings.json";
        watch(existing_directory(profile_dir));
        watch(QString::fromStdString(settings_path.string()));
        const auto read = storage::read_json_file(settings_path);
        if (const auto decoded = profiles::dictionary_settings_from_json(read.document)) {
            settings = profiles::resolved_dictionary_settings(*decoded);
        } else if (read.status != storage::JsonReadStatus::missing &&
                   settings_path == state_->settings_path) {
            settings = state_->settings;
        }
    }
    for (const auto& path : previous) {
        if (!watched.contains(path)) {
            watcher.removePath(path);
        }
    }

    const bool changed = settings_path != state_->settings_path || settings != state_->settings ||
                         language != state_->language_id;
    if (language != state_->language_id) {
        state_->query.reset();
        state_->language_id = language;
    }
    state_->settings_path = std::move(settings_path);
    state_->settings = std::move(settings);
    return changed;
}

void DictionaryPopupModel::lookup(const QString& text) {
    state_->last_query = text.trimmed();
    if (state_->last_query.isEmpty()) {
        clear();
        return;
    }
    reload_profile_settings();
    run_lookup();
}

void DictionaryPopupModel::run_lookup() {
    if (!state_->query) {
        state_->query =
            std::make_unique<DictionaryQueryService>(state_->language_id, state_->dictionaries);
    }

    const auto& settings = state_->settings;
    const LookupOutcome outcome = state_->query->lookup(
        state_->last_query.toStdString(), settings.max_results, settings.scan_length);
    if (!outcome.ok) {
        state_->html.clear();
        state_->error = QString::fromStdString(outcome.error);
        emit errorChanged();
        emit htmlChanged();
        return;
    }
    if (outcome.entries.empty()) {
        clear_results();
        return;
    }

    const QJsonArray entries = lookup_entries_json(outcome.entries);
    QJsonObject styles;
    for (const auto& [name, css] : state_->query->dictionary_styles()) {
        styles.insert(QString::fromStdString(name), QString::fromStdString(css));
    }

    PopupConfig config;
    config.display = {
        .content_language_id = std::string(state_->language_id),
        .max_results = settings.max_results,
        .scan_length = settings.scan_length,
        .collapse_mode = settings.collapse_mode,
        .expand_first_dictionary = settings.expand_first_dictionary,
        .two_column_layout = *settings.two_column_layout,
        .compact_glossaries = settings.compact_glossaries,
        .show_expression_tags = settings.show_expression_tags,
        .harmonic_frequency = settings.harmonic_frequency,
        .deduplicate_pitch_accents = settings.deduplicate_pitch_accents,
        .compact_pitch_accents = settings.compact_pitch_accents,
        .custom_css = settings.custom_css,
    };
    const dictionary::AudioSource source = dictionary::default_audio_source();
    config.audio.sources = {source.url};
    config.audio.source_names = {source.name};
    config.theme.dark = state_->dark;
    config.theme.background = state_->background.name(QColor::HexRgb).toStdString();
    config.theme.text = state_->text_color.name(QColor::HexRgb).toStdString();

    state_->html = QString::fromStdString(
        popup_shell_html(entries, styles, config, state_->assets.css, state_->assets.js));
    state_->error.clear();
    emit errorChanged();
    emit htmlChanged();
}

void DictionaryPopupModel::clear() {
    state_->last_query.clear();
    clear_results();
}

void DictionaryPopupModel::clear_results() {
    state_->html.clear();
    state_->error.clear();
    emit errorChanged();
    emit htmlChanged();
}

}  // namespace niratan::viewmodels

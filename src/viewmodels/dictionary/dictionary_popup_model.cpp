// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_popup_model.h"

#include <QAudioOutput>
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QUrl>

#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "models/anki/mining_context.h"
#include "models/dictionary/audio_source.h"
#include "models/dictionary/dictionary_config.h"
#include "services/anki/anki_config.h"
#include "services/anki/popup_options.h"
#include "services/dictionary/dictionary_config_store.h"
#include "services/dictionary/dictionary_query.h"
#include "services/dictionary/dictionary_store.h"
#include "services/dictionary/lookup_configuration.h"
#include "services/dictionary/popup_assets.h"
#include "services/dictionary/popup_entries.h"
#include "services/dictionary/popup_html.h"
#include "services/profiles/profile_repository.h"
#include "services/settings/dictionary_settings_watch.h"
#include "services/settings/global_settings.h"
#include "services/storage/app_paths.h"

namespace niratan::viewmodels {

using niratan::dictionary::active_lookup_configuration;
using niratan::dictionary::DictionaryInfo;
using niratan::dictionary::DictionaryQueryService;
using niratan::dictionary::DictionaryStore;
using niratan::dictionary::DictionaryType;
using niratan::dictionary::load_collapsed_dictionaries;
using niratan::dictionary::load_dictionary_config;
using niratan::dictionary::load_popup_assets;
using niratan::dictionary::lookup_entries_json;
using niratan::dictionary::LookupConfiguration;
using niratan::dictionary::LookupOutcome;
using niratan::dictionary::popup_shell_html;
using niratan::dictionary::PopupAssets;
using niratan::dictionary::PopupConfig;
using niratan::dictionary::resolved_dictionary_list;

struct DictionaryPopupModel::State {
    DictionaryStore store{storage::dictionaries_root()};
    PopupAssets assets;
    std::unique_ptr<DictionaryQueryService> query;
    std::optional<LookupConfiguration> query_configuration;
    settings::DictionarySettingsWatch settings_watch;
    QString last_query;
    QString html;
    QString matched_text;
    QString error;
    QString audio_error;
    // The entry maps the popup script fetches with getEntries.
    QJsonArray entries;
    int search_text_size = 26;
    bool page = false;
    QAudioOutput audio_output;
    QMediaPlayer audio_player;
    bool dark = false;
    QColor background = QColor(QStringLiteral("#FFFFFF"));
    QColor text_color = QColor(QStringLiteral("#000000"));
};

namespace {

// The Unicode scalar count of a match, which popup.js uses to highlight the
// selected characters (Swift's matchedText.count counts grapheme clusters).
int matched_character_count(const std::string& matched) {
    return static_cast<int>(QString::fromStdString(matched).toUcs4().size());
}

// Rebuilds the profile query when its configuration changed and runs the
// lookup. lookup, lookupRedirect and queryTextSelected share it.
template <typename State> LookupOutcome perform_lookup(State& state, const QString& text) {
    const auto& settings = state.settings_watch.settings();
    LookupConfiguration configuration;
    if (const auto repository = profiles::ProfileRepository::at_data_root()) {
        configuration = active_lookup_configuration(*repository, settings);
    } else {
        for (const auto& dictionary : state.store.installed(DictionaryType::term)) {
            configuration.term_dictionaries.push_back(dictionary.path);
        }
        configuration.language_id = std::string(state.settings_watch.language_id());
    }
    if (!state.query || !state.query_configuration.has_value() ||
        *state.query_configuration != configuration) {
        state.query = std::make_unique<DictionaryQueryService>(configuration);
        state.query_configuration = std::move(configuration);
    }
    return state.query->lookup(text.toStdString(), settings.max_results, settings.scan_length);
}

struct DictionarySets {
    std::vector<std::string> collapsed;
    std::vector<std::string> excluded;
};

// DictionaryManager.loadCollapsedDictionaries and excludedDictionaries for the
// active profile: the collapse set only in Custom mode, and the resolved term
// dictionaries whose category is exclude in list order, disabled ones included.
DictionarySets dictionary_sets(const DictionaryStore& store,
                               const profiles::DictionaryProfileSettings& settings) {
    DictionarySets sets;
    const auto repository = profiles::ProfileRepository::at_data_root();
    if (!repository) {
        return sets;
    }
    const auto profile = repository->active_profile();
    const auto directory = repository->profile_directory(profile.id);
    if (!directory.has_value()) {
        return sets;
    }
    const bool default_profile = profile.id == repository->index().default_profile_id;
    const auto config = load_dictionary_config(*directory / "dictionary_config.json");
    for (const DictionaryInfo& dictionary :
         resolved_dictionary_list(store.installed(DictionaryType::term), config,
                                  DictionaryType::term, default_profile)) {
        if (dictionary.category == anki::DictionaryCategory::exclude) {
            sets.excluded.push_back(dictionary.title);
        }
    }
    if (settings.collapse_mode == "Custom") {
        const std::set<std::string> titles =
            load_collapsed_dictionaries(*directory / "collapsed.json");
        sets.collapsed.assign(titles.begin(), titles.end());
    }
    return sets;
}

}  // namespace

DictionaryPopupModel::DictionaryPopupModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    state_->assets = load_popup_assets();
    state_->audio_player.setAudioOutput(&state_->audio_output);
    connect(&state_->audio_player, &QMediaPlayer::errorOccurred, this,
            [this] { set_audio_error(tr("Could not play word audio")); });
    connect(&state_->settings_watch, &settings::DictionarySettingsWatch::changed, this, [this] {
        const int text_size = state_->settings_watch.settings().search_text_size.value_or(26);
        if (text_size != state_->search_text_size) {
            state_->search_text_size = text_size;
            emit searchTextSizeChanged();
        }
        if (!state_->last_query.isEmpty()) {
            run_lookup();
        }
    });
    state_->settings_watch.reload();
    state_->search_text_size = state_->settings_watch.settings().search_text_size.value_or(26);
}

DictionaryPopupModel::~DictionaryPopupModel() = default;

QString DictionaryPopupModel::html() const {
    return state_->html;
}

QString DictionaryPopupModel::bridge_script() const {
    return QString::fromStdString(state_->assets.web_channel + '\n' + state_->assets.bridge);
}

QString DictionaryPopupModel::error() const {
    return state_->error;
}

QString DictionaryPopupModel::audio_error() const {
    return state_->audio_error;
}

QString DictionaryPopupModel::matched_text() const {
    return state_->matched_text;
}

int DictionaryPopupModel::search_text_size() const {
    return state_->search_text_size;
}

bool DictionaryPopupModel::page() const {
    return state_->page;
}

void DictionaryPopupModel::setPage(bool page) {
    if (state_->page == page) {
        return;
    }
    state_->page = page;
    emit pageChanged();
    if (!state_->last_query.isEmpty()) {
        run_lookup();
    }
}

void DictionaryPopupModel::set_audio_error(const QString& error) {
    if (state_->audio_error != error) {
        state_->audio_error = error;
        emit audioErrorChanged();
    }
}

void DictionaryPopupModel::playWordAudio(const QString& audioUrl) {
    const QUrl source(audioUrl, QUrl::StrictMode);
    if (!source.isValid() || source.host().isEmpty() || !source.userInfo().isEmpty() ||
        (source.scheme() != QStringLiteral("http") && source.scheme() != QStringLiteral("https"))) {
        set_audio_error(tr("Could not play word audio"));
        return;
    }
    set_audio_error({});
    state_->audio_player.stop();
    if (state_->audio_player.error() != QMediaPlayer::NoError) {
        state_->audio_player.setSource({});
    }
    state_->audio_player.setSource(source);
    state_->audio_player.play();
}

void DictionaryPopupModel::stopWordAudio() {
    state_->audio_player.stop();
}

void DictionaryPopupModel::showAudioSourceMenu(const QVariantMap& data) {
    bool valid_entry = false;
    const int entry = data.value(QStringLiteral("entryIndex")).toInt(&valid_entry);
    const double x = data.value(QStringLiteral("x")).toDouble();
    const double y = data.value(QStringLiteral("y")).toDouble();
    if (!has_results() || !valid_entry || entry < 0 || !std::isfinite(x) || !std::isfinite(y)) {
        return;
    }
    const auto names = data.value(QStringLiteral("names")).toStringList();
    const int selected = data.value(QStringLiteral("selected"), -1).toInt();
    emit audioSourceMenuRequested(names, selected, x, y, entry);
}

void DictionaryPopupModel::selectAudioSource(int entryIndex, int sourceIndex) {
    if (has_results() && entryIndex >= 0 && sourceIndex >= 0) {
        emit audioSourceSelected(entryIndex, sourceIndex);
    }
}

int DictionaryPopupModel::lookupRedirect(const QString& query) {
    const QString text = query.trimmed();
    if (text.isEmpty()) {
        state_->entries = {};
        return 0;
    }
    state_->settings_watch.reload();
    const LookupOutcome outcome = perform_lookup(*state_, text);
    if (!outcome.ok || outcome.entries.empty()) {
        state_->entries = {};
        return 0;
    }
    state_->entries = lookup_entries_json(outcome.entries);
    return static_cast<int>(state_->entries.size());
}

QVariantMap DictionaryPopupModel::queryTextSelected(const QString& text) {
    QVariantMap result{{QStringLiteral("entryCount"), 0},
                       {QStringLiteral("matchedCharacterCount"), 0}};
    const QString query = text.trimmed();
    if (query.isEmpty()) {
        return result;
    }
    state_->settings_watch.reload();
    const LookupOutcome outcome = perform_lookup(*state_, query);
    if (!outcome.ok || outcome.entries.empty()) {
        return result;
    }
    state_->entries = lookup_entries_json(outcome.entries);
    result.insert(QStringLiteral("entryCount"), state_->entries.size());
    result.insert(QStringLiteral("matchedCharacterCount"),
                  matched_character_count(outcome.entries.front().matched));
    return result;
}

QVariantList DictionaryPopupModel::getEntries(int start, int count) {
    if (start < 0 || count < 0 || start + count > state_->entries.size()) {
        return {};
    }
    QVariantList entries;
    entries.reserve(count);
    for (int index = start; index < start + count; ++index) {
        entries.append(state_->entries.at(index).toVariant());
    }
    return entries;
}

int DictionaryPopupModel::textSelected(const QString& text) {
    lookup(text);
    return matched_character_count(state_->matched_text.toStdString());
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

void DictionaryPopupModel::lookup(const QString& text) {
    state_->last_query = text.trimmed();
    if (state_->last_query.isEmpty()) {
        clear();
        return;
    }
    state_->settings_watch.reload();
    run_lookup();
}

void DictionaryPopupModel::run_lookup() {
    set_audio_error({});
    const LookupOutcome outcome = perform_lookup(*state_, state_->last_query);
    if (!outcome.ok) {
        state_->entries = {};
        state_->html.clear();
        state_->matched_text.clear();
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

    // The active profile is resolved again for every render, so a profile
    // switch reaches the page before the next lookup.
    anki::PopupOptions mining;
    if (auto repository = profiles::ProfileRepository::at_data_root()) {
        anki::AnkiConfigStore anki_store{storage::data_root(), *repository};
        anki_store.load();
        mining = anki::popup_options(anki_store.settings());
    }

    const auto& settings = state_->settings_watch.settings();
    DictionarySets sets = dictionary_sets(state_->store, settings);
    PopupConfig config;
    config.display = {
        .content_language_id = state_->query_configuration->language_id,
        .max_results = settings.max_results,
        .scan_length = settings.scan_length,
        .scan_non_japanese_text = settings.scan_non_japanese_text,
        .kanji_lookup_enabled = false,
        .collapse_mode = settings.collapse_mode,
        .expand_first_dictionary = settings.expand_first_dictionary,
        .two_column_layout = *settings.two_column_layout,
        .collapsed_dictionaries = std::move(sets.collapsed),
        .excluded_dictionaries = std::move(sets.excluded),
        .compact_glossaries = settings.compact_glossaries,
        .show_expression_tags = settings.show_expression_tags,
        .harmonic_frequency = settings.harmonic_frequency,
        .deduplicate_pitch_accents = settings.deduplicate_pitch_accents,
        .compact_pitch_accents = settings.compact_pitch_accents,
        .custom_css = settings.custom_css,
        .query_source = state_->page ? std::optional<std::string>(state_->last_query.toStdString())
                                     : std::nullopt,
        .search_text_size = state_->search_text_size,
    };
    // PopupWebView.swift passes enabledAudioSources in preference order.
    auto preferred = settings::GlobalSettings::at_data_root().preferred_audio_source();
    const auto fallback = dictionary::default_audio_source();
    if (!preferred.empty() && preferred != fallback.url) {
        config.audio.sources.push_back(std::move(preferred));
        config.audio.source_names.push_back(tr("Preferred").toStdString());
    }
    config.audio.sources.push_back(fallback.url);
    config.audio.source_names.push_back(fallback.name);
    config.mining = {.needs_audio = mining.needs_audio,
                     .allow_dupes = mining.allow_dupes,
                     .use_anki_connect = mining.use_anki_connect,
                     .embed_media = mining.embed_media,
                     .compact_glossaries = mining.compact_glossaries};
    config.theme.dark = state_->dark;
    config.theme.background = state_->background.name(QColor::HexRgb).toStdString();
    config.theme.text = state_->text_color.name(QColor::HexRgb).toStdString();

    // PopupPresentationCoordinator.present passes the first result's matched
    // input span to SelectionData.applyLookupMatch, so it is kept beside the
    // HTML and cleared with the results.
    state_->entries = entries;
    state_->matched_text = QString::fromStdString(outcome.entries.front().matched);
    state_->html = QString::fromStdString(popup_shell_html(
        entries, styles, config, state_->assets.css, state_->assets.selection, state_->assets.js));
    state_->error.clear();
    emit errorChanged();
    emit htmlChanged();
}

void DictionaryPopupModel::clear() {
    state_->last_query.clear();
    clear_results();
}

void DictionaryPopupModel::clear_results() {
    set_audio_error({});
    state_->entries = {};
    state_->html.clear();
    state_->matched_text.clear();
    state_->error.clear();
    emit errorChanged();
    emit htmlChanged();
}

}  // namespace niratan::viewmodels

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
#include <string>
#include <string_view>

#include "models/dictionary/audio_source.h"
#include "services/anki/anki_config.h"
#include "services/anki/popup_options.h"
#include "services/dictionary/dictionary_query.h"
#include "services/dictionary/dictionary_store.h"
#include "services/dictionary/lookup_configuration.h"
#include "services/dictionary/popup_assets.h"
#include "services/dictionary/popup_entries.h"
#include "services/dictionary/popup_html.h"
#include "services/profiles/profile_repository.h"
#include "services/settings/dictionary_settings_watch.h"
#include "services/storage/app_paths.h"

namespace niratan::viewmodels {

using niratan::dictionary::active_lookup_configuration;
using niratan::dictionary::DictionaryQueryService;
using niratan::dictionary::DictionaryStore;
using niratan::dictionary::DictionaryType;
using niratan::dictionary::load_popup_assets;
using niratan::dictionary::lookup_entries_json;
using niratan::dictionary::LookupConfiguration;
using niratan::dictionary::LookupOutcome;
using niratan::dictionary::popup_shell_html;
using niratan::dictionary::PopupAssets;
using niratan::dictionary::PopupConfig;

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
    QAudioOutput audio_output;
    QMediaPlayer audio_player;
    bool dark = false;
    QColor background = QColor(QStringLiteral("#FFFFFF"));
    QColor text_color = QColor(QStringLiteral("#000000"));
};

DictionaryPopupModel::DictionaryPopupModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    state_->assets = load_popup_assets();
    state_->audio_player.setAudioOutput(&state_->audio_output);
    connect(&state_->audio_player, &QMediaPlayer::errorOccurred, this,
            [this] { set_audio_error(tr("Could not play word audio")); });
    connect(&state_->settings_watch, &settings::DictionarySettingsWatch::changed, this, [this] {
        if (!state_->last_query.isEmpty()) {
            run_lookup();
        }
    });
    state_->settings_watch.reload();
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
    LookupConfiguration configuration;
    if (const auto repository = profiles::ProfileRepository::at_data_root()) {
        configuration = active_lookup_configuration(*repository, state_->settings_watch.settings());
    } else {
        for (const auto& dictionary : state_->store.installed(DictionaryType::term)) {
            configuration.term_dictionaries.push_back(dictionary.path);
        }
        configuration.language_id = std::string(state_->settings_watch.language_id());
    }
    if (!state_->query || !state_->query_configuration.has_value() ||
        *state_->query_configuration != configuration) {
        state_->query = std::make_unique<DictionaryQueryService>(configuration);
        state_->query_configuration = std::move(configuration);
    }

    const auto& settings = state_->settings_watch.settings();
    const LookupOutcome outcome = state_->query->lookup(state_->last_query.toStdString(),
                                                        settings.max_results, settings.scan_length);
    if (!outcome.ok) {
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

    PopupConfig config;
    config.display = {
        .content_language_id = state_->query_configuration->language_id,
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
    state_->matched_text = QString::fromStdString(outcome.entries.front().matched);
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
    set_audio_error({});
    state_->html.clear();
    state_->matched_text.clear();
    state_->error.clear();
    emit errorChanged();
    emit htmlChanged();
}

}  // namespace niratan::viewmodels

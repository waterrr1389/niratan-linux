// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_popup_model.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>

#include <memory>
#include <string>
#include <string_view>

#include "models/dictionary/audio_source.h"
#include "services/dictionary/dictionary_query.h"
#include "services/dictionary/dictionary_store.h"
#include "services/dictionary/popup_assets.h"
#include "services/dictionary/popup_entries.h"
#include "services/dictionary/popup_html.h"
#include "services/settings/dictionary_settings_watch.h"
#include "services/storage/app_paths.h"

namespace niratan::viewmodels {

using niratan::dictionary::DictionaryQueryService;
using niratan::dictionary::DictionaryStore;
using niratan::dictionary::LookupOutcome;
using niratan::dictionary::PopupAssets;
using niratan::dictionary::PopupConfig;
using niratan::dictionary::load_popup_assets;
using niratan::dictionary::lookup_entries_json;
using niratan::dictionary::popup_shell_html;

struct DictionaryPopupModel::State {
    DictionaryStore store{storage::dictionaries_root()};
    PopupAssets assets;
    std::unique_ptr<DictionaryQueryService> query;
    settings::DictionarySettingsWatch settings_watch;
    std::string language_id;
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
    const std::string_view language = state_->settings_watch.language_id();
    if (!state_->query || state_->language_id != language) {
        state_->language_id = std::string(language);
        state_->query =
            std::make_unique<DictionaryQueryService>(language, state_->store.installed());
    }

    const auto& settings = state_->settings_watch.settings();
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

// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_popup_model.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>

#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include "models/dictionary/audio_source.h"
#include "services/dictionary/dictionary_query.h"
#include "services/dictionary/dictionary_store.h"
#include "services/dictionary/popup_assets.h"
#include "services/dictionary/popup_entries.h"
#include "services/dictionary/popup_html.h"
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
    std::vector<std::filesystem::path> dictionaries;
    PopupAssets assets;
    std::unique_ptr<DictionaryQueryService> query;
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
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        clear();
        return;
    }

    if (!state_->query) {
        state_->query =
            std::make_unique<DictionaryQueryService>("ja", state_->dictionaries);
    }

    const LookupOutcome outcome = state_->query->lookup(trimmed.toStdString(), 16, 16);
    if (!outcome.ok) {
        state_->html.clear();
        state_->error = QString::fromStdString(outcome.error);
        emit errorChanged();
        emit htmlChanged();
        return;
    }
    if (outcome.entries.empty()) {
        clear();
        return;
    }

    const QJsonArray entries = lookup_entries_json(outcome.entries);
    QJsonObject styles;
    for (const auto& [name, css] : state_->query->dictionary_styles()) {
        styles.insert(QString::fromStdString(name), QString::fromStdString(css));
    }

    PopupConfig config;
    config.display.content_language_id = "ja";
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
    state_->html.clear();
    state_->error.clear();
    emit errorChanged();
    emit htmlChanged();
}

}  // namespace niratan::viewmodels

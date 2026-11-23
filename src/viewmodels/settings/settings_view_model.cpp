// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings_view_model.h"

#include "settings_view_model_state.h"

#include <QStringList>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "models/profiles/dictionary_settings.h"
#include "models/profiles/reader_settings.h"
#include "models/statistics/statistics.h"
#include "services/settings/global_settings.h"
#include "services/settings/settings_store.h"

namespace niratan::viewmodels {
namespace {

using profiles::DictionaryProfileSettings;
using profiles::ReaderProfileSettings;
using settings::GlobalSettings;
using settings::SettingsStore;
using settings::StoreError;

constexpr int kMinFontSize = 16;
constexpr int kMaxFontSize = 40;
constexpr int kWheelNotch = 120;

bool is_theme(std::string_view value) {
    return value == "System" || value == "Light" || value == "Dark" || value == "Sepia"
           || value == "Custom";
}

bool is_interface_theme(std::string_view value) {
    return value == "System" || value == "Light" || value == "Dark";
}

bool is_furigana_mode(std::string_view value) {
    return value == "Off" || value == "Dimmed" || value == "Toggle" || value == "Hidden";
}

bool is_progress_count(std::string_view value) {
    return value == "Off" || value == "Characters" || value == "Pages";
}

bool is_collapse_mode(std::string_view value) {
    return value == "Expand All" || value == "Collapse All" || value == "Custom";
}

QString autostart_text(statistics::StatisticsAutostartMode mode) {
    const std::string_view id = statistics::autostart_mode_id(mode);
    return QString::fromUtf8(id.data(), qsizetype(id.size()));
}

double snap(double value, double minimum, double maximum, int increments_per_unit) {
    return std::round(std::clamp(value, minimum, maximum) * increments_per_unit)
           / increments_per_unit;
}

}  // namespace

SettingsViewModel::SettingsViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    auto store = SettingsStore::create();
    if (!store) {
        state_->profile_error = tr("The Niratan data folder could not be located.");
        return;
    }

    {
        const auto result = store->active_reader();
        if (result.status == settings::LoadStatus::invalid) {
            state_->reader_error =
                tr("Could not load reader_settings.json: %1.")
                    .arg(QString::fromStdString(result.error));
        } else {
            state_->reader = result.settings;
            state_->reader_writable = true;
        }
    }

    {
        const auto result = store->active_dictionary();
        if (result.status == settings::LoadStatus::invalid) {
            state_->dictionary_error =
                tr("Could not load dictionary_settings.json: %1.")
                    .arg(QString::fromStdString(result.error));
        } else {
            state_->dictionary = result.settings;
            state_->dictionary_writable = true;
        }
    }

    {
        GlobalSettings global = GlobalSettings::at_data_root();
        const auto load = global.statistics_preferences();
        if (load.status == settings::LoadStatus::invalid) {
            state_->statistics_error = tr("Could not load settings.json: %1.")
                                           .arg(QString::fromStdString(load.error));
        }
        state_->statistics = load.preferences;
        state_->global = std::move(global);
    }

    state_->store = std::move(store);
}

SettingsViewModel::~SettingsViewModel() = default;

int SettingsViewModel::section() const { return section_; }

void SettingsViewModel::setSection(int value) {
    if (value >= 0 && value <= 3 && section_ != value) {
        section_ = value;
        emit sectionChanged();
    }
}

QString SettingsViewModel::error() const {
    QStringList errors;
    for (const auto* message : {&state_->profile_error, &state_->reader_error,
                                &state_->dictionary_error, &state_->statistics_error}) {
        if (!message->isEmpty()) {
            errors.append(*message);
        }
    }
    return errors.join(u'\n');
}

template <typename Field, typename Value>
void SettingsViewModel::update_dictionary(Field& field, Value value) {
    if (field != value) {
        field = std::move(value);
        save_dictionary();
        emit dictionaryChanged();
    }
}

QString SettingsViewModel::theme() const {
    const auto& value = state_->reader.theme;
    return is_theme(value) ? QString::fromStdString(value) : QStringLiteral("System");
}

void SettingsViewModel::setTheme(const QString& value) {
    const auto raw = value.toStdString();
    if (is_theme(raw)) {
        update_reader(state_->reader.theme, raw);
    }
}

QString SettingsViewModel::uiTheme() const {
    const auto& value = state_->reader.ui_theme;
    return is_interface_theme(value) ? QString::fromStdString(value) : QStringLiteral("System");
}

void SettingsViewModel::setUiTheme(const QString& value) {
    const auto raw = value.toStdString();
    if (is_interface_theme(raw)) {
        update_reader(state_->reader.ui_theme, raw);
    }
}

bool SettingsViewModel::sepiaInvertInDark() const { return state_->reader.sepia_invert_in_dark; }

void SettingsViewModel::setSepiaInvertInDark(bool value) {
    update_reader(state_->reader.sepia_invert_in_dark, value);
}

bool SettingsViewModel::systemLightSepia() const { return state_->reader.system_light_sepia; }

void SettingsViewModel::setSystemLightSepia(bool value) {
    update_reader(state_->reader.system_light_sepia, value);
}

bool SettingsViewModel::verticalWriting() const { return state_->reader.vertical_writing; }

void SettingsViewModel::setVerticalWriting(bool value) {
    update_reader(state_->reader.vertical_writing, value);
}

QString SettingsViewModel::selectedFont() const {
    return QString::fromStdString(state_->reader.selected_font);
}

void SettingsViewModel::setSelectedFont(const QString& value) {
    update_reader(state_->reader.selected_font, value.toStdString());
}

int SettingsViewModel::fontSize() const { return state_->reader.font_size; }

void SettingsViewModel::setFontSize(int value) {
    update_reader(state_->reader.font_size, std::clamp(value, kMinFontSize, kMaxFontSize));
}

void SettingsViewModel::adjustFontSizeFromWheel(int angleDelta) {
    const qint64 accumulated = qint64(state_->font_wheel_remainder) + angleDelta;
    state_->font_wheel_remainder = static_cast<int>(accumulated % kWheelNotch);
    const int steps = static_cast<int>(accumulated / kWheelNotch);
    if (steps != 0) {
        apply_font_size_steps(steps);
    }
}

void SettingsViewModel::stepFontSize(bool increase) {
    apply_font_size_steps(increase ? 1 : -1);
}

void SettingsViewModel::apply_font_size_steps(int steps) {
    const qint64 size = qint64(fontSize()) + steps;
    setFontSize(static_cast<int>(std::clamp<qint64>(size, kMinFontSize, kMaxFontSize)));
}

QString SettingsViewModel::furiganaMode() const {
    const auto& reader = state_->reader;
    if (reader.furigana_mode && is_furigana_mode(*reader.furigana_mode)) {
        return QString::fromStdString(*reader.furigana_mode);
    }
    return reader.hide_furigana ? QStringLiteral("Hidden") : QStringLiteral("Off");
}

void SettingsViewModel::setFuriganaMode(const QString& value) {
    const auto raw = value.toStdString();
    if (is_furigana_mode(raw)) {
        update_reader(state_->reader.furigana_mode, raw);
    }
}

bool SettingsViewModel::continuousMode() const { return state_->reader.continuous_mode; }

void SettingsViewModel::setContinuousMode(bool value) {
    update_reader(state_->reader.continuous_mode, value);
}

bool SettingsViewModel::paragraphMode() const {
    return state_->reader.paragraph_mode.value_or(false);
}

void SettingsViewModel::setParagraphMode(bool value) {
    update_reader(state_->reader.paragraph_mode, value);
}

bool SettingsViewModel::textAnimation() const { return state_->reader.text_animation.value_or(false); }

void SettingsViewModel::setTextAnimation(bool value) {
    update_reader(state_->reader.text_animation, value);
}

int SettingsViewModel::textSpeed() const { return state_->reader.text_speed.value_or(35); }

void SettingsViewModel::setTextSpeed(int value) {
    update_reader(state_->reader.text_speed, std::lround(std::clamp(value, 25, 100) / 5.0) * 5);
}

bool SettingsViewModel::twoColumnHorizontalPages() const {
    return state_->reader.two_column_horizontal_pages.value_or(false);
}

void SettingsViewModel::setTwoColumnHorizontalPages(bool value) {
    update_reader(state_->reader.two_column_horizontal_pages, value);
}

int SettingsViewModel::horizontalPadding() const { return state_->reader.horizontal_padding; }

void SettingsViewModel::setHorizontalPadding(int value) {
    update_reader(state_->reader.horizontal_padding, std::clamp(value, 0, 80));
}

int SettingsViewModel::verticalPadding() const { return state_->reader.vertical_padding; }

void SettingsViewModel::setVerticalPadding(int value) {
    update_reader(state_->reader.vertical_padding, std::clamp(value, 0, 50));
}

bool SettingsViewModel::avoidPageBreak() const { return state_->reader.avoid_page_break; }

void SettingsViewModel::setAvoidPageBreak(bool value) {
    update_reader(state_->reader.avoid_page_break, value);
}

bool SettingsViewModel::justifyText() const { return state_->reader.justify_text; }

void SettingsViewModel::setJustifyText(bool value) {
    update_reader(state_->reader.justify_text, value);
}

bool SettingsViewModel::blurImages() const { return state_->reader.blur_images; }

void SettingsViewModel::setBlurImages(bool value) {
    update_reader(state_->reader.blur_images, value);
}

bool SettingsViewModel::layoutAdvanced() const { return state_->reader.layout_advanced; }

void SettingsViewModel::setLayoutAdvanced(bool value) {
    update_reader(state_->reader.layout_advanced, value);
}

double SettingsViewModel::lineHeight() const { return state_->reader.line_height; }

void SettingsViewModel::setLineHeight(double value) {
    update_reader(state_->reader.line_height, snap(value, 1.0, 2.5, 20));
}

double SettingsViewModel::characterSpacing() const { return state_->reader.character_spacing; }

void SettingsViewModel::setCharacterSpacing(double value) {
    update_reader(state_->reader.character_spacing, snap(value, -10.0, 10.0, 1));
}

double SettingsViewModel::paragraphSpacing() const { return state_->reader.paragraph_spacing; }

void SettingsViewModel::setParagraphSpacing(double value) {
    update_reader(state_->reader.paragraph_spacing, snap(value, 0.0, 3.0, 10));
}

bool SettingsViewModel::showTitle() const { return state_->reader.show_title; }

void SettingsViewModel::setShowTitle(bool value) {
    update_reader(state_->reader.show_title, value);
}

bool SettingsViewModel::showProgress() const {
    return state_->reader.show_progress.value_or(true);
}

void SettingsViewModel::setShowProgress(bool value) {
    update_reader(state_->reader.show_progress, value);
}

bool SettingsViewModel::showChapterProgress() const {
    return state_->reader.show_chapter_progress.value_or(false);
}

void SettingsViewModel::setShowChapterProgress(bool value) {
    update_reader(state_->reader.show_chapter_progress, value);
}

QString SettingsViewModel::progressCount() const {
    if (state_->reader.progress_count && is_progress_count(*state_->reader.progress_count)) {
        return QString::fromStdString(*state_->reader.progress_count);
    }
    return state_->reader.show_characters ? QStringLiteral("Characters") : QStringLiteral("Off");
}

void SettingsViewModel::setProgressCount(const QString& value) {
    const auto raw = value.toStdString();
    if (is_progress_count(raw)) {
        update_reader(state_->reader.progress_count, raw);
    }
}

bool SettingsViewModel::showPercentage() const { return state_->reader.show_percentage; }

void SettingsViewModel::setShowPercentage(bool value) {
    update_reader(state_->reader.show_percentage, value);
}

bool SettingsViewModel::showProgressTop() const { return state_->reader.show_progress_top; }

void SettingsViewModel::setShowProgressTop(bool value) {
    update_reader(state_->reader.show_progress_top, value);
}

bool SettingsViewModel::dictionaryTabDefault() const {
    return state_->dictionary.dictionary_tab_default;
}

void SettingsViewModel::setDictionaryTabDefault(bool value) {
    update_dictionary(state_->dictionary.dictionary_tab_default, value);
}

bool SettingsViewModel::scanNonJapaneseText() const {
    return state_->dictionary.scan_non_japanese_text;
}

void SettingsViewModel::setScanNonJapaneseText(bool value) {
    update_dictionary(state_->dictionary.scan_non_japanese_text, value);
}

int SettingsViewModel::maxResults() const { return state_->dictionary.max_results; }

void SettingsViewModel::setMaxResults(int value) {
    update_dictionary(state_->dictionary.max_results, std::clamp(value, 1, 64));
}

int SettingsViewModel::scanLength() const { return state_->dictionary.scan_length; }

void SettingsViewModel::setScanLength(int value) {
    update_dictionary(state_->dictionary.scan_length, std::clamp(value, 1, 64));
}

QString SettingsViewModel::collapseMode() const {
    const auto& value = state_->dictionary.collapse_mode;
    return is_collapse_mode(value) ? QString::fromStdString(value)
                                   : QStringLiteral("Expand All");
}

void SettingsViewModel::setCollapseMode(const QString& value) {
    const auto raw = value.toStdString();
    if (is_collapse_mode(raw)) {
        update_dictionary(state_->dictionary.collapse_mode, raw);
    }
}

bool SettingsViewModel::twoColumnLayout() const {
    return state_->dictionary.two_column_layout.value_or(false);
}

void SettingsViewModel::setTwoColumnLayout(bool value) {
    if (state_->dictionary.two_column_layout != value) {
        state_->dictionary.two_column_layout = value;
        save_dictionary();
        emit dictionaryChanged();
    }
}

bool SettingsViewModel::compactGlossaries() const {
    return state_->dictionary.compact_glossaries;
}

void SettingsViewModel::setCompactGlossaries(bool value) {
    update_dictionary(state_->dictionary.compact_glossaries, value);
}

QString SettingsViewModel::customCSS() const {
    return QString::fromStdString(state_->dictionary.custom_css);
}

void SettingsViewModel::setCustomCSS(const QString& value) {
    update_dictionary(state_->dictionary.custom_css, value.toStdString());
}

bool SettingsViewModel::enableStatistics() const { return state_->statistics.enabled; }

void SettingsViewModel::setEnableStatistics(bool value) {
    if (state_->statistics.enabled != value) {
        state_->statistics.enabled = value;
        if (state_->global) {
            report_statistics_save(state_->global->save_statistics_enabled(value));
        }
        emit statisticsChanged();
    }
}

QString SettingsViewModel::statisticsAutostartMode() const {
    return autostart_text(state_->statistics.autostart_mode);
}

void SettingsViewModel::setStatisticsAutostartMode(const QString& value) {
    const auto mode = settings::autostart_mode_from_id(value.toStdString());
    if (autostart_text(mode) == value && state_->statistics.autostart_mode != mode) {
        state_->statistics.autostart_mode = mode;
        if (state_->global) {
            report_statistics_save(state_->global->save_statistics_autostart_mode(mode));
        }
        emit statisticsChanged();
    }
}

int SettingsViewModel::statisticsResetTime() const { return state_->statistics.reset_minutes; }

void SettingsViewModel::setStatisticsResetTime(int value) {
    const int clamped = statistics::normalized_reset_minutes(value);
    if (state_->statistics.reset_minutes != clamped) {
        state_->statistics.reset_minutes = clamped;
        if (state_->global) {
            report_statistics_save(state_->global->save_statistics_reset_minutes(clamped));
        }
        emit statisticsChanged();
    }
}

void SettingsViewModel::save_reader() {
    if (!state_->reader_writable || !state_->store) return;
    const auto err = state_->store->save_active_reader(state_->reader);
    if (err != StoreError::none)
        set_error(state_->reader_error, tr("Could not save reader_settings.json."));
    else
        set_error(state_->reader_error, {});
}

void SettingsViewModel::save_dictionary() {
    if (!state_->dictionary_writable || !state_->store) return;
    const auto err = state_->store->save_active_dictionary(state_->dictionary);
    if (err != StoreError::none)
        set_error(state_->dictionary_error, tr("Could not save dictionary_settings.json."));
    else
        set_error(state_->dictionary_error, {});
}

void SettingsViewModel::report_statistics_save(StoreError error) {
    set_error(state_->statistics_error,
              error == StoreError::none ? QString() : tr("Could not save settings.json."));
}

void SettingsViewModel::set_error(QString& slot, QString error) {
    if (slot != error) {
        slot = std::move(error);
        emit errorChanged();
    }
}

}  // namespace niratan::viewmodels

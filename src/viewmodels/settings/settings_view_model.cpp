// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings_view_model.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "models/profiles/dictionary_settings.h"
#include "models/profiles/reader_settings.h"
#include "models/statistics/statistics.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

namespace niratan::viewmodels {
namespace {

using profiles::DictionaryProfileSettings;
using profiles::ReaderProfileSettings;
using storage::JsonReadStatus;

const auto kEnableStatistics = QStringLiteral("enableStatistics");
const auto kAutostartMode = QStringLiteral("statisticsAutostartMode");
const auto kResetTime = QStringLiteral("statisticsResetTime");

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

bool is_autostart_mode(const QString& value) {
    return value == QStringLiteral("Off") || value == QStringLiteral("Page Turn")
           || value == QStringLiteral("On");
}

double snap(double value, double minimum, double maximum, int increments_per_unit) {
    return std::round(std::clamp(value, minimum, maximum) * increments_per_unit)
           / increments_per_unit;
}

QString read_error(const std::filesystem::path& path, const QString& detail) {
    return SettingsViewModel::tr("Could not load %1: %2.")
        .arg(QString::fromStdString(path.filename().string()), detail);
}

template <typename Settings, typename Decode>
bool load_profile_settings(const std::filesystem::path& path, Settings& settings, Decode decode,
                           QString& error) {
    const storage::JsonReadResult read = storage::read_json_file(path);
    if (read.status == JsonReadStatus::missing) {
        return true;
    }
    if (read.status == JsonReadStatus::success) {
        if (auto decoded = decode(read.document)) {
            settings = std::move(*decoded);
            return true;
        }
        error = read_error(path, SettingsViewModel::tr("unexpected settings format"));
    } else {
        error = read_error(path, QString::fromStdString(read.error));
    }
    return false;
}

}  // namespace

struct SettingsViewModel::State {
    ReaderProfileSettings reader = profiles::default_reader_settings();
    DictionaryProfileSettings dictionary = profiles::default_dictionary_settings();
    bool statistics_enabled = false;
    QString autostart_mode = QStringLiteral("Off");
    int reset_minutes = 0;

    std::filesystem::path reader_path;
    std::filesystem::path dictionary_path;
    std::filesystem::path global_path;
    bool reader_writable = false;
    bool dictionary_writable = false;
    QString profile_error;
    QString reader_error;
    QString dictionary_error;
    QString statistics_error;
};

SettingsViewModel::SettingsViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    const auto root = storage::data_root();
    if (root.empty()) {
        state_->profile_error = tr("The Niratan data folder could not be located.");
        return;
    }

    const profiles::ProfileRepository repository(root);
    if (const auto directory = repository.profile_directory(repository.active_profile().id)) {
        state_->reader_path = *directory / "reader_settings.json";
        state_->dictionary_path = *directory / "dictionary_settings.json";
        // Keep settings that could not be decoded intact, even if the JSON
        // syntax was valid. Defaults remain available in memory.
        state_->reader_writable = load_profile_settings(
            state_->reader_path, state_->reader, profiles::reader_settings_from_json,
            state_->reader_error);
        state_->dictionary_writable = load_profile_settings(
            state_->dictionary_path, state_->dictionary, profiles::dictionary_settings_from_json,
            state_->dictionary_error);
    } else {
        state_->profile_error = tr("The active profile has no valid settings folder.");
    }

    state_->global_path = root / "settings.json";
    const auto read = storage::read_json_file(state_->global_path);
    if (read.status == JsonReadStatus::success && read.document.isObject()) {
        const auto object = read.document.object();
        state_->statistics_enabled = object.value(kEnableStatistics).toBool();
        const QString mode = object.value(kAutostartMode).toString();
        if (is_autostart_mode(mode)) {
            state_->autostart_mode = mode;
        }
        state_->reset_minutes = statistics::normalized_reset_minutes(object.value(kResetTime).toInt());
    } else if (read.status != JsonReadStatus::missing) {
        state_->statistics_error = read_error(
            state_->global_path, read.status == JsonReadStatus::invalid
                                     ? QString::fromStdString(read.error)
                                     : tr("expected a JSON object"));
    }
}

SettingsViewModel::~SettingsViewModel() = default;

int SettingsViewModel::section() const { return section_; }

void SettingsViewModel::setSection(int value) {
    if (value >= 0 && value <= 2 && section_ != value) {
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
void SettingsViewModel::update_reader(Field& field, Value value) {
    if (field != value) {
        field = std::move(value);
        save_reader();
        emit readerChanged();
    }
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
    update_reader(state_->reader.font_size, std::clamp(value, 16, 40));
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
        const bool hidden = raw == "Hidden";
        if (state_->reader.furigana_mode != raw || state_->reader.hide_furigana != hidden) {
            state_->reader.furigana_mode = raw;
            state_->reader.hide_furigana = hidden;
            save_reader();
            emit readerChanged();
        }
    }
}

bool SettingsViewModel::continuousMode() const { return state_->reader.continuous_mode; }

void SettingsViewModel::setContinuousMode(bool value) {
    update_reader(state_->reader.continuous_mode, value);
}

bool SettingsViewModel::paragraphMode() const { return state_->reader.paragraph_mode.value_or(false); }

void SettingsViewModel::setParagraphMode(bool value) {
    update_reader(state_->reader.paragraph_mode, value);
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
    if (std::isfinite(value)) {
        update_reader(state_->reader.line_height, snap(value, 1.0, 2.5, 20));
    }
}

double SettingsViewModel::characterSpacing() const { return state_->reader.character_spacing; }

void SettingsViewModel::setCharacterSpacing(double value) {
    if (std::isfinite(value)) {
        update_reader(state_->reader.character_spacing, snap(value, -10.0, 10.0, 1));
    }
}

double SettingsViewModel::paragraphSpacing() const { return state_->reader.paragraph_spacing; }

void SettingsViewModel::setParagraphSpacing(double value) {
    if (std::isfinite(value)) {
        update_reader(state_->reader.paragraph_spacing, snap(value, 0.0, 3.0, 10));
    }
}

bool SettingsViewModel::showTitle() const { return state_->reader.show_title; }

void SettingsViewModel::setShowTitle(bool value) { update_reader(state_->reader.show_title, value); }

bool SettingsViewModel::showProgress() const { return state_->reader.show_progress.value_or(true); }

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
    const auto& reader = state_->reader;
    if (reader.progress_count && is_progress_count(*reader.progress_count)) {
        return QString::fromStdString(*reader.progress_count);
    }
    return reader.show_characters ? QStringLiteral("Characters") : QStringLiteral("Off");
}

void SettingsViewModel::setProgressCount(const QString& value) {
    const auto raw = value.toStdString();
    if (is_progress_count(raw)) {
        const bool show = raw != "Off";
        if (state_->reader.progress_count != raw || state_->reader.show_characters != show) {
            state_->reader.progress_count = raw;
            state_->reader.show_characters = show;
            save_reader();
            emit readerChanged();
        }
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
    update_dictionary(state_->dictionary.max_results, std::clamp(value, 1, 50));
}

int SettingsViewModel::scanLength() const { return state_->dictionary.scan_length; }

void SettingsViewModel::setScanLength(int value) {
    update_dictionary(state_->dictionary.scan_length, std::clamp(value, 1, 64));
}

QString SettingsViewModel::collapseMode() const {
    const auto& value = state_->dictionary.collapse_mode;
    return is_collapse_mode(value) ? QString::fromStdString(value) : QStringLiteral("Expand All");
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
    update_dictionary(state_->dictionary.two_column_layout, value);
}

bool SettingsViewModel::compactGlossaries() const { return state_->dictionary.compact_glossaries; }

void SettingsViewModel::setCompactGlossaries(bool value) {
    update_dictionary(state_->dictionary.compact_glossaries, value);
}

QString SettingsViewModel::customCSS() const {
    return QString::fromStdString(state_->dictionary.custom_css);
}

void SettingsViewModel::setCustomCSS(const QString& value) {
    update_dictionary(state_->dictionary.custom_css, value.toStdString());
}

bool SettingsViewModel::enableStatistics() const { return state_->statistics_enabled; }

void SettingsViewModel::setEnableStatistics(bool value) {
    if (state_->statistics_enabled != value) {
        state_->statistics_enabled = value;
        save_statistic(kEnableStatistics, value);
        emit statisticsChanged();
    }
}

QString SettingsViewModel::statisticsAutostartMode() const { return state_->autostart_mode; }

void SettingsViewModel::setStatisticsAutostartMode(const QString& value) {
    if (is_autostart_mode(value) && state_->autostart_mode != value) {
        state_->autostart_mode = value;
        save_statistic(kAutostartMode, value);
        emit statisticsChanged();
    }
}

int SettingsViewModel::statisticsResetTime() const { return state_->reset_minutes; }

void SettingsViewModel::setStatisticsResetTime(int value) {
    const int clamped = statistics::normalized_reset_minutes(value);
    if (state_->reset_minutes != clamped) {
        state_->reset_minutes = clamped;
        save_statistic(kResetTime, clamped);
        emit statisticsChanged();
    }
}

void SettingsViewModel::save_reader() {
    if (state_->reader_writable) {
        write_document(state_->reader_path, profiles::reader_settings_to_json(state_->reader),
                       state_->reader_error);
    }
}

void SettingsViewModel::save_dictionary() {
    if (state_->dictionary_writable) {
        write_document(state_->dictionary_path,
                       profiles::dictionary_settings_to_json(state_->dictionary),
                       state_->dictionary_error);
    }
}

void SettingsViewModel::save_statistic(const QString& key, const QJsonValue& value) {
    if (state_->global_path.empty()) {
        return;
    }

    const auto read = storage::read_json_file(state_->global_path);
    if (read.status == JsonReadStatus::invalid
        || (read.status == JsonReadStatus::success && !read.document.isObject())) {
        const QString detail = read.status == JsonReadStatus::invalid
                                   ? QString::fromStdString(read.error)
                                   : tr("expected a JSON object");
        set_error(state_->statistics_error, tr("Could not save settings.json: %1.").arg(detail));
        return;
    }

    QJsonObject object = read.document.object();
    object.insert(key, value);
    write_document(state_->global_path, QJsonDocument(object), state_->statistics_error);
}

void SettingsViewModel::write_document(const std::filesystem::path& path,
                                       const QJsonDocument& document, QString& error) {
    std::string detail;
    if (storage::write_json_file(path, document, &detail)) {
        set_error(error, {});
    } else {
        set_error(error, tr("Could not save %1: %2.")
                             .arg(QString::fromStdString(path.filename().string()),
                                  QString::fromStdString(detail)));
    }
}

void SettingsViewModel::set_error(QString& slot, QString error) {
    if (slot != error) {
        slot = std::move(error);
        emit errorChanged();
    }
}

}  // namespace niratan::viewmodels

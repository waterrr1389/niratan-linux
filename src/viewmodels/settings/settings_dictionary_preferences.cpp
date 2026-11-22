// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Dictionary section's frequency sort and search text preferences of
// SettingsViewModel, kept in their own translation unit so the main file stays
// under the size limit. Nothing outside src/viewmodels/settings/ includes the
// state header.
//
// The getters follow UserConfig.init and UserConfig.apply(dictionaryProfileSettings:):
// a missing or unknown frequency sort order reads Auto, a missing dictionary
// name reads empty and a missing text size reads 26. Setters write the active
// profile's dictionary_settings.json through the same save path as the other
// dictionary keys, which keeps unrelated keys intact. Only the setter applies
// a rule: the order must be one of LookupFrequencySortOrder's raw values and
// the size is clamped to the Search Text Stepper's 12...48. A stored value is
// never rewritten on load.

#include "settings_view_model.h"

#include "settings_view_model_state.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace niratan::viewmodels {
namespace {

// The raw values of LookupFrequencySortOrder in Core/LookupEngine.swift.
bool is_frequency_sort_order(std::string_view value) {
    return value == "Auto" || value == "Ascending" || value == "Descending";
}

}  // namespace

QString SettingsViewModel::frequencySortOrder() const {
    const auto& value = state_->dictionary.frequency_sort_order;
    if (value && is_frequency_sort_order(*value)) {
        return QString::fromStdString(*value);
    }
    return QStringLiteral("Auto");
}

void SettingsViewModel::setFrequencySortOrder(const QString& value) {
    const std::string raw = value.toStdString();
    if (!is_frequency_sort_order(raw)) {
        return;
    }
    const std::optional<std::string> next = raw;
    if (state_->dictionary.frequency_sort_order != next) {
        state_->dictionary.frequency_sort_order = next;
        save_dictionary();
        emit dictionaryChanged();
    }
}

QString SettingsViewModel::frequencySortDictionary() const {
    const auto& value = state_->dictionary.frequency_sort_dictionary;
    return QString::fromStdString(value.value_or(std::string()));
}

void SettingsViewModel::setFrequencySortDictionary(const QString& value) {
    const std::optional<std::string> next = value.toStdString();
    if (state_->dictionary.frequency_sort_dictionary != next) {
        state_->dictionary.frequency_sort_dictionary = next;
        save_dictionary();
        emit dictionaryChanged();
    }
}

int SettingsViewModel::searchTextSize() const {
    return state_->dictionary.search_text_size.value_or(26);
}

void SettingsViewModel::setSearchTextSize(int value) {
    const std::optional<int> next = std::clamp(value, 12, 48);
    if (state_->dictionary.search_text_size != next) {
        state_->dictionary.search_text_size = next;
        save_dictionary();
        emit dictionaryChanged();
    }
}

}  // namespace niratan::viewmodels

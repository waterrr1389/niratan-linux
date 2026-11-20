// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile_index.h"

#include <QCoreApplication>

namespace niratan::profiles {

// Raw-value match only: a tag such as "en-US" or an empty string is Japanese.
ContentLanguage HoshiProfile::language() const {
    return dictionary_language_id == "en" ? ContentLanguage::english
                                          : ContentLanguage::japanese;
}

std::string HoshiProfile::display_name() const {
    if (id == default_japanese_profile().id && (name == "Japanese" || name == "Japanese EPUB")) {
        return QCoreApplication::translate("HoshiProfile", "Japanese").toStdString();
    }
    return name;
}

ProfileIndex ProfileIndex::initial() {
    const HoshiProfile japanese = default_japanese_profile();
    ProfileIndex index;
    index.profiles = {japanese};
    index.default_profile_id = japanese.id;
    index.global_active_profile_id = japanese.id;
    index.primary_profile_ids_by_language = {{"ja", japanese.id}};
    return index;
}

HoshiProfile default_japanese_profile() {
    return HoshiProfile{"default-ja", "Japanese", "ja", true};
}

HoshiProfile resolve_profile(const ProfileIndex& index) {
    for (const HoshiProfile& candidate : index.profiles) {
        if (candidate.id == index.global_active_profile_id) {
            return candidate;
        }
    }
    for (const HoshiProfile& candidate : index.profiles) {
        if (candidate.id == index.default_profile_id) {
            return candidate;
        }
    }
    if (!index.profiles.empty()) {
        return index.profiles.front();
    }
    return default_japanese_profile();
}

}  // namespace niratan::profiles

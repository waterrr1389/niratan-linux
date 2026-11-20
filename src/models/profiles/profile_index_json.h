// SPDX-License-Identifier: GPL-3.0-or-later
//
// The profiles.json codec, following the Codable shape of ProfileIndex and
// HoshiProfile in Models/Profile.swift and the persistence in
// Core/ProfileRepository.swift (indexFileName, persistIndex).
//
// The document is an object with "profiles", "defaultProfileId",
// "globalActiveProfileId" and "primaryProfileIdsByLanguage". Each profile has
// "id", "name", "dictionaryLanguageId" and "isDefault". Swift's decoder
// requires every non-optional key, so a missing or wrongly typed key rejects
// the whole document instead of defaulting; extra keys are ignored. The
// returned index is not normalized; the repository does that.
//
// The writer emits the same keys. Key order is QJsonObject's, which the
// repository reads by name.

#pragma once

#include <QJsonDocument>

#include <optional>

#include "models/profiles/profile_index.h"

namespace niratan::profiles {

std::optional<ProfileIndex> profile_index_from_json(const QJsonDocument& document);

QJsonDocument profile_index_to_json(const ProfileIndex& index);

}  // namespace niratan::profiles

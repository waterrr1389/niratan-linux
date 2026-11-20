// SPDX-License-Identifier: GPL-3.0-or-later
//
// Decoding follows the synthesized Codable conformance in Models/Profile.swift:
// every key is required and a wrongly typed value rejects the whole document
// rather than falling back to a default. Extra keys are ignored at every
// level. Order and contents are preserved, because normalization belongs to
// the repository.

#include "profile_index_json.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>

#include <optional>
#include <string>
#include <utility>

namespace niratan::profiles {
namespace {

QString text(const std::string& value) {
    return QString::fromStdString(value);
}

// A missing key is indistinguishable from a wrongly typed one: Codable treats
// both as a decoding failure, so the caller only needs nullopt.
std::optional<std::string> string_at(const QJsonObject& object, QLatin1StringView key) {
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return std::nullopt;
    }
    return value.toString().toStdString();
}

std::optional<HoshiProfile> profile_from_json(const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();

    const std::optional<std::string> id = string_at(object, QLatin1StringView("id"));
    const std::optional<std::string> name = string_at(object, QLatin1StringView("name"));
    const std::optional<std::string> language =
        string_at(object, QLatin1StringView("dictionaryLanguageId"));
    const QJsonValue is_default = object.value(QLatin1StringView("isDefault"));
    if (!id || !name || !language || !is_default.isBool()) {
        return std::nullopt;
    }

    return HoshiProfile{*id, *name, *language, is_default.toBool()};
}

QJsonObject profile_to_json(const HoshiProfile& profile) {
    QJsonObject object;
    object.insert(QLatin1StringView("id"), text(profile.id));
    object.insert(QLatin1StringView("name"), text(profile.name));
    object.insert(QLatin1StringView("dictionaryLanguageId"), text(profile.dictionary_language_id));
    object.insert(QLatin1StringView("isDefault"), profile.is_default);
    return object;
}

}  // namespace

std::optional<ProfileIndex> profile_index_from_json(const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();

    const QJsonValue profiles = object.value(QLatin1StringView("profiles"));
    const std::optional<std::string> default_profile_id =
        string_at(object, QLatin1StringView("defaultProfileId"));
    const std::optional<std::string> global_active_profile_id =
        string_at(object, QLatin1StringView("globalActiveProfileId"));
    const QJsonValue primary_by_language =
        object.value(QLatin1StringView("primaryProfileIdsByLanguage"));

    if (!profiles.isArray() || !default_profile_id || !global_active_profile_id ||
        !primary_by_language.isObject()) {
        return std::nullopt;
    }

    ProfileIndex index;
    const QJsonArray profile_array = profiles.toArray();
    for (const QJsonValue& entry : profile_array) {
        std::optional<HoshiProfile> profile = profile_from_json(entry);
        if (!profile) {
            return std::nullopt;
        }
        index.profiles.push_back(std::move(*profile));
    }

    const QJsonObject map = primary_by_language.toObject();
    for (auto entry = map.begin(); entry != map.end(); ++entry) {
        if (!entry.value().isString()) {
            return std::nullopt;
        }
        index.primary_profile_ids_by_language.emplace(
            entry.key().toStdString(), entry.value().toString().toStdString());
    }

    index.default_profile_id = std::move(*default_profile_id);
    index.global_active_profile_id = std::move(*global_active_profile_id);
    return index;
}

QJsonDocument profile_index_to_json(const ProfileIndex& index) {
    QJsonArray profiles;
    for (const HoshiProfile& profile : index.profiles) {
        profiles.append(profile_to_json(profile));
    }

    QJsonObject primary_by_language;
    for (const auto& [language, profile_id] : index.primary_profile_ids_by_language) {
        primary_by_language.insert(text(language), text(profile_id));
    }

    QJsonObject object;
    object.insert(QLatin1StringView("profiles"), profiles);
    object.insert(QLatin1StringView("defaultProfileId"), text(index.default_profile_id));
    object.insert(QLatin1StringView("globalActiveProfileId"),
                  text(index.global_active_profile_id));
    object.insert(QLatin1StringView("primaryProfileIdsByLanguage"), primary_by_language);
    return QJsonDocument(object);
}

}  // namespace niratan::profiles

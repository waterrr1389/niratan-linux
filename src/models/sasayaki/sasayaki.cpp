// SPDX-License-Identifier: GPL-3.0-or-later

#include "sasayaki.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>

#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace niratan::sasayaki {
namespace {

QString text(const std::string& value) {
    return QString::fromStdString(value);
}

bool required_string(const QJsonObject& object, QLatin1StringView key, std::string* out) {
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return false;
    }
    *out = value.toString().toStdString();
    return true;
}

bool required_number(const QJsonObject& object, QLatin1StringView key, double* out) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }
    *out = value.toDouble();
    return true;
}

// An absent key and a null both leave the optional property empty, following
// decodeIfPresent; a value of another type rejects the document.
bool optional_number(const QJsonObject& object, QLatin1StringView key,
                     std::optional<double>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    *out = value.toDouble();
    return true;
}

// An integer property decodes only from a number without a fractional part that
// fits the property's range; anything else fails instead of saturating.
std::optional<int> integer_value(const QJsonValue& value) {
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    constexpr double kUpperBound = 2147483648.0;  // 2^31, one past INT_MAX
    if (!std::isfinite(number) || std::trunc(number) != number || number < -kUpperBound ||
        number >= kUpperBound) {
        return std::nullopt;
    }
    return static_cast<int>(number);
}

bool required_integer(const QJsonObject& object, QLatin1StringView key, int* out) {
    const std::optional<int> number = integer_value(object.value(key));
    if (!number) {
        return false;
    }
    *out = *number;
    return true;
}

// SasayakiMatch.contextInferred is Bool? in Swift; the producer writes only
// true, so the flat field reads an absent or null key as false.
bool optional_boolean(const QJsonObject& object, QLatin1StringView key, bool* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        *out = false;
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    *out = value.toBool();
    return true;
}

bool match_from_json(const QJsonValue& value, SasayakiMatch* out) {
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();

    SasayakiMatch match;
    if (!required_string(object, QLatin1StringView("id"), &match.id) ||
        !required_number(object, QLatin1StringView("startTime"), &match.start_time) ||
        !required_number(object, QLatin1StringView("endTime"), &match.end_time) ||
        !required_string(object, QLatin1StringView("text"), &match.text) ||
        !required_integer(object, QLatin1StringView("chapterIndex"), &match.chapter_index) ||
        !required_integer(object, QLatin1StringView("start"), &match.start) ||
        !required_integer(object, QLatin1StringView("length"), &match.length) ||
        !optional_boolean(object, QLatin1StringView("contextInferred"),
                          &match.context_inferred)) {
        return false;
    }
    *out = std::move(match);
    return true;
}

QJsonObject match_to_json(const SasayakiMatch& match) {
    QJsonObject object;
    object.insert(QLatin1StringView("id"), text(match.id));
    object.insert(QLatin1StringView("startTime"), match.start_time);
    object.insert(QLatin1StringView("endTime"), match.end_time);
    object.insert(QLatin1StringView("text"), text(match.text));
    object.insert(QLatin1StringView("chapterIndex"), match.chapter_index);
    object.insert(QLatin1StringView("start"), match.start);
    object.insert(QLatin1StringView("length"), match.length);
    if (match.context_inferred) {
        object.insert(QLatin1StringView("contextInferred"), true);
    }
    return object;
}

}  // namespace

std::optional<SasayakiMatchData> match_data_from_json(const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();

    const QJsonValue matches_value = object.value(QLatin1StringView("matches"));
    if (!matches_value.isArray()) {
        return std::nullopt;
    }

    SasayakiMatchData data;
    const QJsonArray matches = matches_value.toArray();
    for (const QJsonValue& entry : matches) {
        SasayakiMatch match;
        if (!match_from_json(entry, &match)) {
            return std::nullopt;
        }
        data.matches.push_back(std::move(match));
    }
    if (!required_integer(object, QLatin1StringView("unmatched"), &data.unmatched)) {
        return std::nullopt;
    }
    return data;
}

QJsonDocument match_data_to_json(const SasayakiMatchData& data) {
    QJsonArray matches;
    for (const SasayakiMatch& match : data.matches) {
        matches.append(match_to_json(match));
    }

    QJsonObject object;
    object.insert(QLatin1StringView("matches"), matches);
    object.insert(QLatin1StringView("unmatched"), data.unmatched);
    return QJsonDocument(object);
}

std::optional<SasayakiPlaybackData> playback_data_from_json(const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();

    SasayakiPlaybackData data;
    if (!required_number(object, QLatin1StringView("lastPosition"), &data.last_position)) {
        return std::nullopt;
    }

    std::optional<double> delay;
    std::optional<double> rate;
    if (!optional_number(object, QLatin1StringView("delay"), &delay) ||
        !optional_number(object, QLatin1StringView("rate"), &rate)) {
        return std::nullopt;
    }
    data.delay = delay.value_or(0.0);
    data.rate = static_cast<float>(rate.value_or(1.0));

    const QJsonValue bookmark = object.value(QLatin1StringView("audioBookmark"));
    if (bookmark.isUndefined() || bookmark.isNull()) {
        data.audio_bookmark.reset();
    } else if (bookmark.isString()) {
        data.audio_bookmark = QByteArray::fromBase64(bookmark.toString().toLatin1());
    } else {
        return std::nullopt;
    }
    return data;
}

QJsonDocument playback_data_to_json(const SasayakiPlaybackData& data) {
    QJsonObject object;
    object.insert(QLatin1StringView("lastPosition"), data.last_position);
    object.insert(QLatin1StringView("delay"), data.delay);
    object.insert(QLatin1StringView("rate"), static_cast<double>(data.rate));
    if (data.audio_bookmark) {
        object.insert(QLatin1StringView("audioBookmark"),
                      QString::fromLatin1(data.audio_bookmark->toBase64()));
    }
    return QJsonDocument(object);
}

}  // namespace niratan::sasayaki

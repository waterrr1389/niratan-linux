// SPDX-License-Identifier: GPL-3.0-or-later

#include "anki_requests.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace niratan::anki {

namespace {

QString utf8(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// NSNumber.int64Value truncates toward zero. A number outside the int64 range
// would trap there; it is dropped here instead.
std::optional<std::int64_t> json_int64(const QJsonValue& value) {
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number)) {
        return std::nullopt;
    }
    constexpr double kMin = -9223372036854775808.0;
    constexpr double kMaxExclusive = 9223372036854775808.0;
    if (number < kMin || number >= kMaxExclusive) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(number);
}

// Swift's Int64(string): an optional sign followed by decimal digits only.
std::optional<std::int64_t> decimal_int64(const QString& text) {
    if (text.isEmpty()) {
        return std::nullopt;
    }
    qsizetype index = 0;
    const bool negative = text.at(0) == u'-';
    if (negative || text.at(0) == u'+') {
        index = 1;
    }
    if (index == text.size()) {
        return std::nullopt;
    }

    const std::uint64_t limit = negative ? 9223372036854775808ULL : 9223372036854775807ULL;
    std::uint64_t magnitude = 0;
    for (; index < text.size(); ++index) {
        const char16_t digit = text.at(index).unicode();
        if (digit < u'0' || digit > u'9') {
            return std::nullopt;
        }
        const std::uint64_t value = digit - u'0';
        if (magnitude > (limit - value) / 10) {
            return std::nullopt;
        }
        magnitude = magnitude * 10 + value;
    }
    if (negative) {
        if (magnitude == 9223372036854775808ULL) {
            return std::numeric_limits<std::int64_t>::min();
        }
        return -static_cast<std::int64_t>(magnitude);
    }
    return static_cast<std::int64_t>(magnitude);
}

QJsonObject scope_to_json(const DuplicateOptions& options) {
    QJsonObject object;
    object.insert("duplicateScope", QString::fromStdString(options.scope));
    if (options.scope_options) {
        const DuplicateScopeOptions& scope = *options.scope_options;
        QJsonObject scope_object;
        if (scope.deck_name) {
            scope_object.insert("deckName", QString::fromStdString(*scope.deck_name));
        }
        if (scope.check_children) {
            scope_object.insert("checkChildren", true);
        }
        if (scope.check_all_models) {
            scope_object.insert("checkAllModels", true);
        }
        object.insert("duplicateScopeOptions", scope_object);
    }
    return object;
}

}  // namespace

Request version_request() {
    return {.action = "version", .params = std::nullopt};
}

Request media_dir_path_request() {
    return {.action = "getMediaDirPath", .params = std::nullopt};
}

Request deck_names_request() {
    return {.action = "deckNames", .params = std::nullopt};
}

Request model_names_request() {
    return {.action = "modelNames", .params = std::nullopt};
}

Request sync_request() {
    return {.action = "sync", .params = std::nullopt};
}

Request model_field_names_request(std::string_view model_name) {
    return {.action = "modelFieldNames", .params = QJsonObject{{"modelName", utf8(model_name)}}};
}

Request find_notes_request(std::string_view query) {
    return {.action = "findNotes", .params = QJsonObject{{"query", utf8(query)}}};
}

Request add_note_request(const QJsonObject& note) {
    return {.action = "addNote", .params = QJsonObject{{"note", note}}};
}

Request store_media_file_request(std::string_view filename, const QByteArray& data) {
    return {.action = "storeMediaFile",
            .params = QJsonObject{{"filename", utf8(filename)},
                                  {"data", QString::fromLatin1(data.toBase64())}}};
}

Request can_add_notes_request(std::string_view deck, std::string_view note_type,
                              std::string_view first_field, std::string_view word,
                              DuplicateScope scope, bool check_all_models) {
    // The duplicate check never carries allowDuplicate or the saved allowDupes
    // setting; only the scope options apply.
    const DuplicateOptions options =
        duplicate_options(scope, deck, /*allow_duplicates=*/false, check_all_models);
    QJsonObject note;
    note.insert("deckName", utf8(deck));
    note.insert("modelName", utf8(note_type));
    note.insert("fields", QJsonObject{{utf8(first_field), utf8(word)}});
    note.insert("options", scope_to_json(options));
    return {.action = "canAddNotesWithErrorDetail",
            .params = QJsonObject{{"notes", QJsonArray{note}}}};
}

std::optional<Request> gui_browse_request(const std::vector<std::int64_t>& note_ids) {
    std::vector<std::int64_t> valid;
    std::set<std::int64_t> seen;
    for (const std::int64_t note_id : note_ids) {
        if (note_id > 0 && seen.insert(note_id).second) {
            valid.push_back(note_id);
        }
    }
    if (valid.empty()) {
        return std::nullopt;
    }
    std::string query = "nid:";
    for (std::size_t index = 0; index < valid.size(); ++index) {
        if (index != 0) {
            query += ',';
        }
        query += std::to_string(valid[index]);
    }
    return Request{.action = "guiBrowse", .params = QJsonObject{{"query", utf8(query)}}};
}

AddNoteOutcome parse_add_note_result(const AnkiConnectResult& result) {
    if (!result.ok) {
        return {.note_id = std::nullopt, .request_failed = true, .error = result.error};
    }
    const std::optional<std::int64_t> note_id = json_int64(result.result);
    if (!note_id || *note_id <= 0) {
        return {};
    }
    return {.note_id = *note_id, .request_failed = false, .error = {}};
}

std::optional<std::vector<std::int64_t>> parse_find_notes_result(const AnkiConnectResult& result) {
    if (!result.ok || !result.result.isArray()) {
        return std::nullopt;
    }

    std::vector<std::int64_t> note_ids;
    std::set<std::int64_t> seen;
    for (const QJsonValue& value : result.result.toArray()) {
        std::optional<std::int64_t> note_id;
        if (value.isString()) {
            note_id = decimal_int64(value.toString());
        } else {
            note_id = json_int64(value);
        }
        if (!note_id || *note_id <= 0 || !seen.insert(*note_id).second) {
            continue;
        }
        note_ids.push_back(*note_id);
    }
    return note_ids;
}

CanAddVerdict parse_can_add_result(const AnkiConnectResult& result) {
    if (!result.ok || !result.result.isArray()) {
        return CanAddVerdict::indeterminate;
    }
    const QJsonArray elements = result.result.toArray();
    if (elements.isEmpty()) {
        return CanAddVerdict::indeterminate;
    }
    const QJsonValue first = elements.at(0);
    if (!first.isObject()) {
        return CanAddVerdict::indeterminate;
    }
    const QJsonValue can_add = first.toObject().value("canAdd");
    if (!can_add.isBool()) {
        return CanAddVerdict::indeterminate;
    }
    return can_add.toBool() ? CanAddVerdict::can_add : CanAddVerdict::cannot_add;
}

std::optional<std::vector<std::string>> parse_string_array_result(const AnkiConnectResult& result) {
    if (!result.ok || !result.result.isArray()) {
        return std::nullopt;
    }
    std::vector<std::string> values;
    for (const QJsonValue& value : result.result.toArray()) {
        if (!value.isString()) {
            return std::nullopt;
        }
        values.push_back(value.toString().toStdString());
    }
    return values;
}

std::optional<std::string> parse_media_dir_path_result(const AnkiConnectResult& result) {
    if (!result.ok || !result.result.isString()) {
        return std::nullopt;
    }
    return result.result.toString().toStdString();
}

bool request_succeeded(const AnkiConnectResult& result) {
    return result.ok;
}

}  // namespace niratan::anki

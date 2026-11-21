// SPDX-License-Identifier: GPL-3.0-or-later
//
// The AnkiConnect actions Niratan sends and the result of each, following
// Core/AnkiManager.swift (ankiConnectRequest(action:params:),
// refreshAnkiConnectStatus, getMediaDirPath, fetchAnkiConnect,
// addNoteAnkiConnect, duplicateLookup, duplicateNoteIDs, openNotesInAnki,
// syncAnkiConnect) and Models/Anki.swift (AnkiConnectConfig, DuplicateScope,
// AnkiNoteType, AnkiDuplicateLookupResult).
//
// A request is the action string and the params object, if any. The POST
// envelope around it (version, key, timeout, error/result decoding) belongs
// to services/anki/anki_connect.
//
// Actions:
//   - The action strings are exactly "version", "getMediaDirPath",
//     "deckNames", "modelNames", "modelFieldNames",
//     "canAddNotesWithErrorDetail", "findNotes", "addNote",
//     "storeMediaFile", "guiBrowse" and "sync".
//   - version, getMediaDirPath, deckNames, modelNames and sync send no params
//     member at all.
//   - modelFieldNames sends {"modelName": <model name>} unchanged, findNotes
//     {"query": <query>} unchanged and addNote {"note": <note>} unchanged.
//   - storeMediaFile sends {"filename": <name>, "data": <base64>}: standard
//     padded base64 without line breaks.
//   - canAddNotesWithErrorDetail sends {"notes": [<note>]} whose note has
//     exactly deckName, modelName, fields and options. deckName and modelName
//     are the given selected deck and note type, and fields has the single
//     member <first field>: <word>, the word unchanged. options follows
//     duplicate_options(scope, deck, allow_duplicates, check_all_models), but
//     allowDuplicate is never written: this request does not carry it and
//     neither does the saved allowDupes setting.
//   - guiBrowse keeps the note IDs greater than 0, drops repeats keeping the
//     first occurrence and sends {"query": "nid:<id>,<id>,..."} with decimal
//     IDs and no spaces. Without a valid ID there is no request, which counts
//     as a failure.
//
// Results:
//   - addNote yields a note ID only when the result is a JSON number whose
//     int64 value is greater than 0. A request failure, a missing or null
//     result, zero, a negative number or a string yields no ID, and the
//     request failure stays distinguishable from the unusable result.
//   - findNotes yields IDs when the result is a JSON array: a number member
//     becomes its int64 value, a string member is read as a decimal int64
//     with an optional sign, other members are dropped, values not greater
//     than 0 are dropped, and repeats keep the first occurrence. A failed,
//     missing or non-array result is a failure, distinct from a valid empty
//     array's empty list, so a malformed reply cannot pass for a clean
//     lookup.
//   - canAddNotesWithErrorDetail is read from the first element only, as an
//     object whose canAdd Boolean decides. A missing result, a non-array, a
//     non-object element, a non-Boolean canAdd or a failed request is
//     indeterminate rather than an error; the caller combines it with the
//     saved-word answer.
//   - deckNames, modelNames and modelFieldNames are used only when the
//     result is an array of strings. Any other shape yields nothing; the
//     fetch itself decides whether that stops it or only skips one model.
//   - getMediaDirPath is used only when the result is a string; any other
//     shape, a failed request included, counts as unavailable.
//   - The version, sync and guiBrowse results are never inspected. Success is
//     the request succeeding; what each caller does with a failure belongs
//     to the caller.
//
// Recorded differences: Swift reads note IDs and canAdd through `as?
// NSNumber`/`as? Bool`, so a JSON number or boolean bridges: a note ID
// accepts a number member and a boolean becomes 1/0, and canAdd accepts a
// number as its truthiness. Here only JSON numbers count as note IDs, so a
// boolean is dropped, and canAdd must be a JSON boolean; a number is
// indeterminate, since AnkiConnect always sends a boolean. Only the first
// canAdd element is read, while Swift's typed cast rejects the whole array
// when any element is not an object; the request carries one note, so the
// result has one element. A fractional number truncates
// toward zero as NSNumber.int64Value does. A number outside the int64 range
// yields no ID instead of the Swift runtime's trap, which normal input does
// not reach. String elements of findNotes accept the same forms as Swift's
// Int64 initializer (optional '+' or '-', decimal digits only) and an
// out-of-range value is dropped. Swift returns an empty list for a non-array
// findNotes result; here a missing or non-array result is a failure, so a
// caller can tell a failed lookup from one that found no notes.
//
// Every tier 1 branch is covered by the tests. Binding the selected deck,
// note type and first field into the duplicate request is caller wiring.

#pragma once

#include <QByteArray>
#include <QJsonObject>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "services/anki/anki_connect.h"
#include "services/anki/duplicate_scope.h"

namespace niratan::anki {

struct Request {
    std::string action;
    // Absent when the action sends no params member at all.
    std::optional<QJsonObject> params;

    bool operator==(const Request&) const = default;
};

Request version_request();
Request media_dir_path_request();
Request deck_names_request();
Request model_names_request();
Request sync_request();

Request model_field_names_request(std::string_view model_name);
Request find_notes_request(std::string_view query);
Request add_note_request(const QJsonObject& note);
Request store_media_file_request(std::string_view filename, const QByteArray& data);

Request can_add_notes_request(std::string_view deck, std::string_view note_type,
                              std::string_view first_field, std::string_view word,
                              DuplicateScope scope, bool check_all_models);

// Empty when no note ID is greater than 0.
std::optional<Request> gui_browse_request(const std::vector<std::int64_t>& note_ids);

struct AddNoteOutcome {
    // Present only when the request succeeded with a positive JSON number.
    std::optional<std::int64_t> note_id;
    // True when the AnkiConnect request itself failed, so that case can be
    // told apart from a result that carried no usable ID.
    bool request_failed = false;
    // The AnkiConnect error text on a request failure; empty otherwise.
    std::string error;

    bool operator==(const AddNoteOutcome&) const = default;
};

AddNoteOutcome parse_add_note_result(const AnkiConnectResult& result);

// A failure is nullopt, an empty array is an empty list.
std::optional<std::vector<std::int64_t>> parse_find_notes_result(const AnkiConnectResult& result);

enum class CanAddVerdict { can_add, cannot_add, indeterminate };

CanAddVerdict parse_can_add_result(const AnkiConnectResult& result);

std::optional<std::vector<std::string>> parse_string_array_result(const AnkiConnectResult& result);

std::optional<std::string> parse_media_dir_path_result(const AnkiConnectResult& result);

// The whole outcome of an action whose result is ignored.
bool request_succeeded(const AnkiConnectResult& result);

}  // namespace niratan::anki

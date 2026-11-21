// SPDX-License-Identifier: GPL-3.0-or-later

#include "duplicate_lookup.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSaveFile>
#include <QString>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "models/anki/note_type.h"
#include "services/anki/anki_requests.h"
#include "services/anki/duplicate_query.h"
#include "services/storage/json_file.h"

namespace niratan::anki {

namespace {

QString utf8(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

DuplicateLookupResult saved_words_answer(bool is_duplicate) {
    return {.is_duplicate = is_duplicate, .note_ids = {}};
}

}  // namespace

std::filesystem::path saved_words_path(const std::filesystem::path& data_root) {
    return data_root / "anki_words.json";
}

std::set<std::string> load_saved_words(const std::filesystem::path& data_root) {
    const storage::JsonReadResult read = storage::read_json_file(saved_words_path(data_root));
    if (read.status != storage::JsonReadStatus::success || !read.document.isArray()) {
        return {};
    }

    std::set<std::string> words;
    for (const QJsonValue& value : read.document.array()) {
        if (!value.isString()) {
            // Swift decodes the whole set at once, so one bad member empties it.
            return {};
        }
        words.insert(value.toString().toStdString());
    }
    return words;
}

DuplicateLookupService::DuplicateLookupService(std::filesystem::path data_root)
    : data_root_(std::move(data_root)), saved_words_(load_saved_words(data_root_)) {}

void DuplicateLookupService::lookup(std::string_view word, const AnkiSettings& settings,
                                    AnkiRequestFunction request, DuplicateLookupCallback callback) {
    const std::string word_text(word);

    const NoteType* note_type = nullptr;
    if (settings.selected_note_type.has_value()) {
        for (const NoteType& candidate : settings.available_note_types) {
            if (candidate.name == *settings.selected_note_type) {
                note_type = &candidate;
                break;
            }
        }
    }
    if (note_type == nullptr || note_type->fields.empty() || !settings.selected_deck.has_value()) {
        callback(saved_words_answer(saved_words_.contains(word_text)));
        return;
    }

    // The settings' scope applies to both requests; without a transport Swift's
    // nil configuration makes the options use the deck scope while the query
    // falls back to the collection scope.
    DuplicateScope can_add_scope = DuplicateScope::deck;
    DuplicateScope query_scope = DuplicateScope::collection;
    bool check_all_models = false;
    if (settings.transport.has_value()) {
        can_add_scope = settings.transport->duplicate_scope;
        query_scope = settings.transport->duplicate_scope;
        check_all_models = settings.transport->check_all_models;
    }

    const std::string deck = *settings.selected_deck;
    const std::string note_type_name = note_type->name;
    const std::string first_field = note_type->fields.front();
    const Request can_add = can_add_notes_request(deck, note_type_name, first_field, word_text,
                                                  can_add_scope, check_all_models);

    request(can_add.action, can_add.params,
            [this, word_text, deck, note_type_name, first_field, query_scope, check_all_models,
             note_types = settings.available_note_types, request,
             callback = std::move(callback)](AnkiConnectResult result) mutable {
                const CanAddVerdict verdict = parse_can_add_result(result);
                if (verdict == CanAddVerdict::indeterminate) {
                    callback(saved_words_answer(saved_words_.contains(word_text)));
                    return;
                }
                if (verdict == CanAddVerdict::can_add) {
                    callback(saved_words_answer(false));
                    return;
                }

                // The word is recorded for this session before the search; only
                // add_saved_word writes the file.
                saved_words_.insert(word_text);
                const std::string query =
                    duplicate_search_query(word_text, deck, note_type_name, first_field,
                                           query_scope, check_all_models, note_types);
                const Request find = find_notes_request(query);
                request(find.action, find.params,
                        [callback = std::move(callback)](AnkiConnectResult find_result) mutable {
                            const std::optional<std::vector<std::int64_t>> note_ids =
                                parse_find_notes_result(find_result);
                            if (!note_ids.has_value()) {
                                callback({.is_duplicate = true, .note_ids = {}});
                                return;
                            }
                            callback({.is_duplicate = true, .note_ids = *note_ids});
                        });
            });
}

bool DuplicateLookupService::add_saved_word(std::string_view word) {
    saved_words_.insert(std::string(word));

    const std::filesystem::path path = saved_words_path(data_root_);
    std::error_code directory_error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), directory_error);
        if (directory_error) {
            return false;
        }
    }

    QSaveFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonArray array;
    for (const std::string& saved : saved_words_) {
        array.append(utf8(saved));
    }
    const QByteArray bytes = QJsonDocument(array).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

}  // namespace niratan::anki

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The duplicate decision before an Anki card is added, following
// Core/AnkiManager.swift (duplicateLookup(word:), duplicateNoteIDs(matching:),
// duplicateSearchQuery, loadWords, addWord, saveWords, the ankiWords constant
// and AnkiDuplicateLookupResult) and Models/Anki.swift (DuplicateScope).
//
// DuplicateLookupService is the module object. It loads the saved words once from
// <data root>/anki_words.json and keeps them in memory across lookups, as
// AnkiManager's savedWords does. A lookup answers with mining_gate's
// DuplicateLookupResult: whether the word is a duplicate and the AnkiConnect
// note IDs that matched. Decision order:
//   1. The saved-word answer is `is_duplicate` = the word is in the in-memory
//      saved words, with no IDs.
//   2. The lookup needs a selected note type that names a note type in
//      available_note_types and has at least one field, and a selected deck.
//      Without both, it answers with the saved-word answer and sends no
//      request. A present empty string counts as selected, as Swift's
//      `String?` does.
//   3. Otherwise it sends canAddNotesWithErrorDetail for the selected deck,
//      note type, first field and the word, built by anki_requests, with the
//      settings' duplicate scope and check-all-models. Only the verdict is
//      read.
//   4. A can_add verdict is not a duplicate, no matter what the saved words
//      hold, and sends nothing more.
//   5. A cannot_add verdict inserts the word into the in-memory saved words,
//      then sends findNotes with duplicate_query's duplicate_search_query for
//      the same deck, note type, first field, scope and note types, and
//      answers a duplicate with the IDs that request returned. The insertion
//      is not persisted here; add_saved_word does that after a successful add.
//   6. An indeterminate verdict, or a failed canAddNotesWithErrorDetail
//      request, answers with the saved-word answer and sends no findNotes.
//   7. A failed findNotes, or a successful one whose result is not an array,
//      answers a duplicate with no IDs because the word is now in memory.
//
// Without a transport the canAddNotesWithErrorDetail options carry the deck
// scope, while the findNotes query uses the collection scope; that is what a
// nil ankiConnectConfig produces in Swift. A loaded configuration always has a
// transport, so this only matters to a caller that builds settings by hand.
//
// The word is the note's expression; the mining caller derives it as
// `content["expression"] ?? ""`. This module reads no content.
//
// Saved words: anki_words.json holds a JSON array of strings. A file that is
// missing, unreadable, not valid JSON or holds a non-string member loads as an
// empty set; the whole decode fails in Swift, so one bad member empties the
// set. add_saved_word inserts into the in-memory set and writes the whole set,
// so the insertion survives a failed write for later lookups, as Swift's
// in-memory `savedWords` does. An empty word is stored as an empty string.
// Swift's Set order is not defined, so the port writes the distinct words
// sorted by byte value; reading them back gives the same set.
//
// Recorded differences:
// - A failed request does not touch connectivity (isAnkiConnectReachable and
//   scheduleAnkiConnectReconnect); the lookup reports and nothing else.
// - add_saved_word reports whether the write succeeded. Swift's `try?
//   saveWords` discards that result, and callers here ignore it as well; only
//   the saved-word file is affected.
// - The canAdd verdict is read from the first array element only, through
//   anki_requests' parse_can_add_result, so `[{"canAdd": false}, 5]` is
//   cannot_add here while Swift's `result as? [[String: Any]]` makes the whole
//   verdict indeterminate. The request carries one note, so its reply has one
//   element. anki_requests records this difference in full.
// - The saved-word set compares bytes. Swift's Set<String> compares Strings by
//   canonical equivalence, so a decomposed and a precomposed spelling are two
//   entries here, while Swift treats the two canonically equivalent
//   spellings as one; the note-type match compares bytes the same way. Not
//   reproduced, as normal input does not reach it. The file is compact JSON,
//   as JSONEncoder writes it.
// - The module writes anki_words.json itself through QSaveFile: the bytes go
//   to a temporary file in the data root and replace the target atomically on
//   commit, and an existing file is replaced even when it is damaged or not
//   valid JSON, as Swift's `try? write` does. Parent directories are created
//   first. A write that fails leaves the previous file in place.

#pragma once

#include <QJsonObject>

#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "services/anki/anki_config.h"
#include "services/anki/anki_connect.h"
#include "services/anki/mining_gate.h"

namespace niratan::anki {

// An injected request seam with the shape of AnkiConnectClient::request, so a
// test answers without a network and the app passes the client's method.
// Taking it by value lets a lookup that waits for AnkiConnect keep its own
// copy after returning.
using AnkiRequestFunction = std::function<void(
    std::string action, std::optional<QJsonObject> params, AnkiConnectClient::Callback callback)>;

// A lookup calls the callback exactly once. The callback may run before
// lookup returns or later, on the same thread the request seam used.
using DuplicateLookupCallback = std::function<void(DuplicateLookupResult)>;

// The saved-word file at the data root.
std::filesystem::path saved_words_path(const std::filesystem::path& data_root);

// The file's set, empty when it cannot be decoded.
std::set<std::string> load_saved_words(const std::filesystem::path& data_root);

class DuplicateLookupService {
   public:
    // Loads the saved words once; nothing is written.
    explicit DuplicateLookupService(std::filesystem::path data_root);

    void lookup(std::string_view word, const AnkiSettings& settings, AnkiRequestFunction request,
                DuplicateLookupCallback callback);

    // Inserts word into the in-memory set and writes it back. False when the
    // write failed; the word stays in memory either way.
    bool add_saved_word(std::string_view word);

   private:
    std::filesystem::path data_root_;
    std::set<std::string> saved_words_;
};

}  // namespace niratan::anki

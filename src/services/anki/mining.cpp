// SPDX-License-Identifier: GPL-3.0-or-later

#include "mining.h"

#include <QCoreApplication>
#include <QString>

#include <utility>

#include "services/anki/anki_requests.h"
#include "services/anki/mining_gate.h"

namespace niratan::anki {

namespace mining_detail {

using MediaDirectory = std::optional<std::filesystem::path>;
using MediaDirectories = std::map<std::pair<std::string, std::string>, MediaDirectory>;
using NoteReady = std::function<void(QJsonObject)>;

// Card assembly and media handling live in mining_card.cpp.
void build_card(const MiningDependencies& deps, MediaDirectories& cache, MiningAttempt attempt,
                const AnkiSettings& settings, NoteReady ready);

// Defined in mining_card.cpp so video capture and card building share one
// transport cache.
void resolve_media_directory(const MiningDependencies& deps, MediaDirectories& cache,
                             std::function<void(MediaDirectory)> callback);

}  // namespace mining_detail

namespace {

void send(const MiningDependencies& deps, Request request, AnkiConnectClient::Callback callback) {
    deps.request(std::move(request.action), std::move(request.params), std::move(callback));
}

AnkiMiningResult failed(std::string message) {
    return {AnkiMiningStatus::failed, std::move(message), std::nullopt};
}

AnkiMiningResult profile_changed() {
    return failed(QCoreApplication::translate(
                      "AnkiMining", "The active Profile changed. Try adding the card again.")
                      .toStdString());
}

bool same_profile(const MiningDependencies& deps, const std::string& id) {
    return deps.repository_profile_id() == id && deps.anki_profile_id() == id;
}

AnkiMiningResult from_gate(MiningResult result) {
    const auto status = result.status == MiningStatus::duplicate ? AnkiMiningStatus::duplicate
                                                                 : AnkiMiningStatus::failed;
    return {status, std::move(result.message), result.note_id};
}

void submit_note(const MiningDependencies& deps, DuplicateLookupService& lookup, QJsonObject note,
                 std::string expression, MiningService::Callback callback) {
    send(deps, add_note_request(note),
         [&deps, &lookup, expression = std::move(expression),
          callback = std::move(callback)](AnkiConnectResult reply) mutable {
             auto outcome = parse_add_note_result(reply);
             if (!outcome.note_id) {
                 callback(
                     failed(outcome.request_failed
                                ? std::move(outcome.error)
                                : QCoreApplication::translate(
                                      "AnkiMining", "AnkiConnect did not return the added note ID.")
                                      .toStdString()));
                 return;
             }
             lookup.add_saved_word(expression);
             deps.clear_media();
             AnkiMiningResult added{
                 AnkiMiningStatus::added,
                 QCoreApplication::translate("AnkiMining", "Added to Anki.").toStdString(),
                 outcome.note_id};
             if (const auto& transport = deps.settings().transport;
                 transport && transport->force_sync) {
                 send(deps, sync_request(),
                      [callback = std::move(callback), added = std::move(added)](
                          AnkiConnectResult) mutable { callback(std::move(added)); });
             } else {
                 callback(std::move(added));
             }
         });
}

}  // namespace

QJsonObject AnkiMiningResult::web_payload() const {
    QString name;
    switch (status) {
        case AnkiMiningStatus::added:
            name = "added";
            break;
        case AnkiMiningStatus::duplicate:
            name = "duplicate";
            break;
        case AnkiMiningStatus::failed:
            name = "failed";
            break;
    }
    QJsonObject payload{{"status", name}, {"message", QString::fromStdString(message)}};
    if (note_id) {
        payload.insert("noteID", QString::number(*note_id));
    }
    return payload;
}

MiningService::MiningService(DuplicateLookupService& duplicate_lookup,
                             MiningDependencies dependencies)
    : duplicate_lookup_(duplicate_lookup), dependencies_(std::move(dependencies)) {}

void MiningService::preflight(Strings content, PreflightCallback callback) {
    const auto profile_id = dependencies_.repository_profile_id();
    dependencies_.activate_profile(profile_id);
    const auto& settings = dependencies_.settings();
    const MiningSettings gate{.selected_deck = settings.selected_deck,
                              .selected_note_type = settings.selected_note_type,
                              .allow_dupes = settings.allow_dupes};
    // Probe the gate for a lookup without evaluating video capture, which
    // may not have happened yet. Reapply it with the real verdict only
    // after checking the profile that was active before activation.
    std::optional<std::string> expression;
    auto verdict = gate_anki_mining(gate, content, {}, [&expression](std::string_view word) {
        expression = word;
        return DuplicateLookupResult{};
    });
    if (verdict) {
        callback(from_gate(std::move(*verdict)));
        return;
    }
    if (!expression) {
        callback(std::nullopt);
        return;
    }
    duplicate_lookup_.lookup(
        *expression, settings, dependencies_.request,
        [this, profile_id, gate, content = std::move(content),
         callback = std::move(callback)](DuplicateLookupResult duplicate) {
            if (!same_profile(dependencies_, profile_id)) {
                callback(profile_changed());
                return;
            }
            auto result = gate_anki_mining(gate, content, {},
                                           [&duplicate](std::string_view) { return duplicate; });
            callback(result ? std::optional(from_gate(std::move(*result))) : std::nullopt);
        });
}

void MiningService::mine(MiningAttempt attempt, Callback callback) {
    const auto profile_id =
        attempt.profile_id ? *attempt.profile_id : dependencies_.repository_profile_id();
    if (dependencies_.repository_profile_id() != profile_id) {
        callback(profile_changed());
        return;
    }
    dependencies_.activate_profile(profile_id);
    const bool preflight_passed = attempt.preflight_already_passed;
    auto preflight_content = preflight_passed ? Strings{} : attempt.content;
    auto begin_add = [this, profile_id, attempt = std::move(attempt),
                      callback =
                          std::move(callback)](std::optional<AnkiMiningResult> verdict) mutable {
        if (verdict) {
            callback(std::move(*verdict));
            return;
        }
        if (!attempt.preflight_already_passed && !same_profile(dependencies_, profile_id)) {
            callback(profile_changed());
            return;
        }
        const auto& settings = dependencies_.settings();
        MiningSettings media_gate;
        media_gate.needs_video_screenshot = needs_video_screenshot(settings.field_mappings);
        media_gate.needs_video_audio_clip = needs_video_audio_clip(settings.field_mappings);
        if (auto result =
                gate_anki_mining(media_gate, attempt.content, attempt.context, {}, true)) {
            callback(from_gate(std::move(*result)));
            return;
        }
        if (!settings.selected_deck || !settings.selected_note_type) {
            callback(failed(
                QCoreApplication::translate("AnkiMining", "Failed to add card.").toStdString()));
            return;
        }
        const auto found = attempt.content.find("expression");
        auto expression = found == attempt.content.end() ? std::string{} : found->second;
        mining_detail::build_card(dependencies_, media_directories_, std::move(attempt), settings,
                                  [this, expression = std::move(expression),
                                   callback = std::move(callback)](QJsonObject note) mutable {
                                      submit_note(dependencies_, duplicate_lookup_, std::move(note),
                                                  std::move(expression), std::move(callback));
                                  });
    };
    if (preflight_passed) {
        begin_add(std::nullopt);
    } else {
        preflight(std::move(preflight_content), std::move(begin_add));
    }
}

void MiningService::media_directory(
    std::function<void(std::optional<std::filesystem::path>)> callback) {
    mining_detail::resolve_media_directory(dependencies_, media_directories_, std::move(callback));
}

}  // namespace niratan::anki

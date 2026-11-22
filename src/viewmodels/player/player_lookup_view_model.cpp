// SPDX-License-Identifier: GPL-3.0-or-later

#include "player_lookup_view_model.h"

#include "viewmodels/dictionary/dictionary_popup_model.h"

#include "player_lookup_policy.h"
#include "services/dictionary/text_selection.h"
#include "services/player/subtitle_document.h"
#include "services/player/video_mining.h"
#include "services/settings/dictionary_settings_watch.h"

#include <algorithm>
#include <optional>

namespace niratan::viewmodels {

using dictionary::highlight_range;
using dictionary::lookup_candidate;
using player::SubtitleCueStore;
using player::SubtitleDocument;

struct PlayerLookupViewModel::State {
    std::optional<SubtitleDocument> document;
    std::optional<SubtitleCueStore> cue_store;
    double position = 0.0;
    double subtitle_delay = 0.0;
    QVariantList current_cues;
    QString active_cue_id;
    DictionaryPopupModel* popup = nullptr;
    bool has_popup = false;
    bool playing = false;
    bool auto_pause = true;
    bool should_resume = false;
    int hover_delay_ms = 45;
    ShiftHoverLookupState hover;
    settings::DictionarySettingsWatch settings_watch;

    void refresh_current_cues(PlayerLookupViewModel& view);
    void resume_after_popup(PlayerLookupViewModel& view);
};

PlayerLookupViewModel::PlayerLookupViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    // One popup model serves every lookup; a new lookup replaces its content,
    // which is the video overlay's only path (replacingExisting: true).
    state_->popup = new DictionaryPopupModel(this);
    state_->settings_watch.reload();
}

PlayerLookupViewModel::~PlayerLookupViewModel() = default;

QVariantList PlayerLookupViewModel::current_cues() const {
    return state_->current_cues;
}

QString PlayerLookupViewModel::active_cue_id() const {
    return state_->active_cue_id;
}

bool PlayerLookupViewModel::has_popup() const {
    return state_->has_popup;
}

QObject* PlayerLookupViewModel::popup_model() {
    return state_->popup;
}

int PlayerLookupViewModel::hover_delay_ms() const {
    return state_->hover_delay_ms;
}

void PlayerLookupViewModel::setSubtitleDocument(const QUrl& file, const QByteArray& bytes,
                                                bool readable) {
    state_->document.reset();
    state_->cue_store.reset();
    if (!file.isEmpty() && readable) {
        // No format hint: the source path's extension decides, as
        // SubtitleParser.format(for:) does, so a dotless name is rejected.
        auto parsed = player::parse_subtitle_document(bytes, file);
        if (parsed.document) {
            state_->cue_store.emplace(*parsed.document);
            state_->document = std::move(parsed.document);
        }
    }
    state_->refresh_current_cues(*this);
}

void PlayerLookupViewModel::updatePlayback(double position, double subtitleDelay) {
    state_->position = position;
    state_->subtitle_delay = subtitleDelay;
    state_->refresh_current_cues(*this);
}

void PlayerLookupViewModel::setPlaying(bool playing) {
    state_->playing = playing;
}

void PlayerLookupViewModel::setLookupSettings(bool autoPauseOnLookup, int hoverDelayMs) {
    state_->auto_pause = autoPauseOnLookup;
    const int delay = normalized_delay_milliseconds(hoverDelayMs);
    if (state_->hover_delay_ms != delay) {
        state_->hover_delay_ms = delay;
        emit hoverDelayChanged();
    }
}

QVariantMap PlayerLookupViewModel::lookupAt(const QString& cueId, int utf16Offset) {
    if (!state_->document) {
        return {};
    }
    const auto& cues = state_->document->cues;
    const auto cue = std::ranges::find_if(
        cues, [&cueId](const auto& candidate_cue) { return candidate_cue.id == cueId; });
    if (cue == cues.end()) {
        return {};
    }

    // The same active profile settings DictionaryPopupModel looks up with.
    state_->settings_watch.reload();
    const auto& settings = state_->settings_watch.settings();
    const auto candidate =
        lookup_candidate(cue->text.toStdString(), utf16Offset, settings.scan_length,
                         state_->settings_watch.language_id());
    // Swift's performLookup returns before presenting, so a click without a
    // candidate leaves the popup, the active cue and the resume flag alone.
    if (!candidate) {
        return {};
    }

    if (state_->active_cue_id != cueId) {
        state_->active_cue_id = cueId;
        emit activeCueChanged();
    }

    const bool was_popup_open = state_->has_popup;
    state_->popup->lookup(QString::fromStdString(candidate->text));
    if (!state_->popup->has_results()) {
        // PopupPresentationCoordinator.present returns nil; the coordinator
        // then drops the popup, the active cue and any remembered resume.
        if (state_->has_popup) {
            state_->has_popup = false;
            emit popupChanged();
        }
        if (!state_->active_cue_id.isEmpty()) {
            state_->active_cue_id.clear();
            emit activeCueChanged();
        }
        state_->resume_after_popup(*this);
        return {};
    }

    if (!state_->has_popup) {
        state_->has_popup = true;
        emit popupChanged();
    }
    if (!was_popup_open) {
        if (state_->auto_pause) {
            state_->should_resume = state_->playing;
            if (state_->should_resume) {
                emit pauseRequested();
            }
        } else {
            state_->should_resume = false;
        }
    }

    const QString matched = state_->popup->matched_text();
    QVariantMap result;
    result.insert(QStringLiteral("text"), QString::fromStdString(candidate->text));
    result.insert(QStringLiteral("utf16Start"), candidate->utf16_start);
    result.insert(QStringLiteral("matchedText"), matched);
    const auto highlight = highlight_range(*candidate, matched.toStdString());
    result.insert(QStringLiteral("highlightStart"),
                  highlight ? highlight->utf16_start : candidate->utf16_start);
    result.insert(QStringLiteral("highlightLength"), highlight ? highlight->utf16_length : 0);
    return result;
}

QVariantMap PlayerLookupViewModel::miningSelection() const {
    if (!state_->document || state_->active_cue_id.isEmpty()) {
        return {};
    }
    const auto& cues = state_->document->cues;
    const auto cue = std::ranges::find_if(
        cues, [this](const auto& candidate) { return candidate.id == state_->active_cue_id; });
    if (cue == cues.end()) {
        return {};
    }
    const auto selection = player::resolve_mining_selection(*cue, cues);
    QVariantMap result;
    result.insert(QStringLiteral("cueText"), selection.cue_text);
    result.insert(QStringLiteral("cueStart"), selection.cue_start);
    result.insert(QStringLiteral("cueEnd"), selection.cue_end);
    result.insert(QStringLiteral("sentence"), selection.sentence);
    if (selection.previous_cue_text) {
        result.insert(QStringLiteral("previousCueText"), *selection.previous_cue_text);
    }
    if (selection.next_cue_text) {
        result.insert(QStringLiteral("nextCueText"), *selection.next_cue_text);
    }
    return result;
}

void PlayerLookupViewModel::closeLookup() {
    state_->popup->clear();
    if (state_->has_popup) {
        state_->has_popup = false;
        emit popupChanged();
    }
    if (!state_->active_cue_id.isEmpty()) {
        state_->active_cue_id.clear();
        emit activeCueChanged();
    }
    state_->resume_after_popup(*this);
}

bool PlayerLookupViewModel::setShiftPressed(bool pressed) {
    return state_->hover.set_shift_pressed(pressed);
}

bool PlayerLookupViewModel::setHoverPointAvailable(bool available) {
    return state_->hover.set_hover_point_available(available);
}

bool PlayerLookupViewModel::pointerMoved() const {
    return state_->hover.pointer_moved();
}

void PlayerLookupViewModel::cancelShiftHover() {
    state_->hover.cancel();
}

void PlayerLookupViewModel::State::refresh_current_cues(PlayerLookupViewModel& view) {
    QVariantList cues;
    if (cue_store) {
        for (const auto& cue : cue_store->cues_at_playback(position, subtitle_delay)) {
            QVariantMap entry;
            entry.insert(QStringLiteral("id"), cue.id);
            entry.insert(QStringLiteral("start"), cue.start_time);
            entry.insert(QStringLiteral("end"), cue.end_time);
            entry.insert(QStringLiteral("text"), cue.text);
            cues.append(entry);
        }
    }
    if (cues != current_cues) {
        current_cues = std::move(cues);
        emit view.currentCuesChanged();
    }
}

void PlayerLookupViewModel::State::resume_after_popup(PlayerLookupViewModel& view) {
    // Swift plays whenever the pause was remembered, whatever the page has
    // reported since, and clears the flag so a later close stays silent.
    if (should_resume) {
        emit view.playRequested();
    }
    should_resume = false;
}

}  // namespace niratan::viewmodels

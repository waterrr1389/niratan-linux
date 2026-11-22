// SPDX-License-Identifier: GPL-3.0-or-later
//
// The mining side of the popup's cue audio, following
// Features/Popup/PopupView.swift (mineEntry's needsSasayakiAudio gate and
// sasayakiCue) and Core/AnkiManager.swift (needsSasayakiAudio,
// addNoteAnkiConnect's sasayakiAudioFields).
//
// setSasayakiCue is called before every mineEntry. The cue and the pending
// answers live here so the add keeps the attempt, profile and reply captured
// when the mine started. A mine asks for one clip only after its preflight
// passed, only with an exact {sasayaki-audio} mapping and a cue, and only for
// a reader context; video keeps its video-audio bucket. sasayakiAudioReady
// answers the first pending mine in request order, with empty bytes meaning
// no attachment. The answer's extension comes from the same format that was
// sent to the page, not from a later settings read.

#include "mining_view_model.h"

#include <QByteArray>
#include <QString>

#include <memory>
#include <string>
#include <utility>

namespace niratan::viewmodels {

void MiningViewModel::setSasayakiCue(const QString& cueId) {
    if (!sasayaki_state_) {
        sasayaki_state_ = std::make_unique<SasayakiState>();
    }
    sasayaki_state_->cue = cueId;
}

void MiningViewModel::sasayakiAudioReady(const QByteArray& data) {
    if (!sasayaki_state_ || sasayaki_state_->pending.empty()) {
        return;
    }
    auto add = std::move(sasayaki_state_->pending.front());
    sasayaki_state_->pending.pop_front();
    add(data, {});
}

bool MiningViewModel::request_cue_audio(
    const QString& sentence, int format, int bitrate,
    std::function<void(QByteArray data, std::string extension)> add) {
    if (!sasayaki_state_ || sasayaki_state_->cue.isEmpty()) {
        return false;
    }
    const std::string extension = format == 1 ? "mp3" : "m4a";
    sasayaki_state_->pending.push_back(
        [add = std::move(add), extension](QByteArray data, std::string) mutable {
            add(std::move(data), extension);
        });
    emit sasayakiAudioRequested(sasayaki_state_->cue, sentence, format, bitrate);
    return true;
}

}  // namespace niratan::viewmodels

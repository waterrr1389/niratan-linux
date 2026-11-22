// SPDX-License-Identifier: GPL-3.0-or-later
//
// The popup's cue audio, following Features/Sasayaki/SasayakiPlayer.swift
// (cueSentenceAudio and expandCue) and Features/Popup/PopupView.swift
// (mineEntry's sasayakiCue). requestCueAudio expands the cue with the
// selection's sentence, applies the playback delay to the cut range, exports
// the clip at the stored rate through services/sasayaki/cue_audio and answers
// cueAudioReady once, in request order. Missing audio, an unknown cue and a
// failed export answer empty bytes; the card is still added without audio.
// The exporter's cache lives until the audio source changes or the book
// closes, which sasayaki_view_model.cpp clears.

#include "sasayaki_view_model.h"

#include <QByteArray>
#include <QString>

#include <string>

#include "models/sasayaki/sasayaki.h"
#include "sasayaki_view_model_state.h"
#include "services/anki/anki_config.h"
#include "services/sasayaki/cue_audio.h"

namespace niratan::viewmodels {

void SasayakiViewModel::requestCueAudio(const QString& cueId, const QString& sentence, int format,
                                        int bitrate) {
    if (!state_->cue_audio_exporter) {
        state_->cue_audio_exporter = new sasayaki::CueAudioExporter(QString(), this);
        connect(state_->cue_audio_exporter, &sasayaki::CueAudioExporter::finished, this,
                [this](QByteArray data) { emit cueAudioReady(data); });
    }

    sasayaki::CueAudioRequest request;
    const sasayaki::SasayakiMatch* cue = state_->has_audio && !state_->audio_path.isEmpty()
        ? find_match_by_id(state_->match, cueId.toStdString())
        : nullptr;
    if (cue != nullptr) {
        const sasayaki::CueAudioRange expanded =
            sasayaki::expand_cue(state_->match->matches, *cue, sentence.toStdString());
        const sasayaki::CueAudioRange range = sasayaki::cue_clip_range(expanded, state_->delay);
        request.source_path = state_->audio_path;
        request.start = range.start;
        request.end = range.end;
        request.rate = state_->rate;
        request.format =
            format == 1 ? anki::AudioCompressionFormat::mp3 : anki::AudioCompressionFormat::aac;
        request.bitrate_kbps = bitrate;
    }
    // An unanswerable request keeps the empty start and end, which the
    // exporter answers without running ffmpeg, in the same request order.
    state_->cue_audio_exporter->start(request);
}

}  // namespace niratan::viewmodels

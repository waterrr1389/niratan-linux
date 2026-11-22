// SPDX-License-Identifier: GPL-3.0-or-later
//
// The lookup popup's Sasayaki cue, following Features/Sasayaki/SasayakiPlayer.swift
// (findCue(chapterIndex:offset:)) and NativeMac/NativeReaderView.swift
// (handleSelection's normalizedOffset lookup and syncBookmarkToSasayakiCue).
//
// findPopupCue is only answered while audio is loaded and the book has match
// data: without them a selection has no cue. The cue covers start ..<
// start + length in the reader's current chapter; an offset in a gap or at a
// range end finds nothing. The map also carries the cue's reader progress,
// start over the chapter's readable characters (clamped to 0...1, 0 for an
// empty chapter), which the page passes to
// ReaderViewModel.syncBookmarkToProgress so the bookmark of a jump matches
// SasayakiMatch.readerProgress.

#include "sasayaki_view_model.h"

#include <QVariantMap>

#include <cstddef>
#include <optional>

#include "models/sasayaki/sasayaki.h"
#include "sasayaki_player_policy.h"
#include "sasayaki_view_model_state.h"
#include "viewmodels/reader/reader_view_model.h"

namespace niratan::viewmodels {

QVariantMap SasayakiViewModel::findPopupCue(int chapterIndex, int offset) const {
    if (!state_->has_audio || !state_->match.has_value() || offset < 0) {
        return {};
    }
    const std::optional<std::size_t> index =
        cue_index_at_offset(state_->match->matches, chapterIndex, offset);
    if (!index.has_value()) {
        return {};
    }

    const sasayaki::SasayakiMatch& match = state_->match->matches[*index];
    QVariantMap cue;
    cue.insert(QStringLiteral("id"), QString::fromStdString(match.id));
    cue.insert(QStringLiteral("chapterIndex"), match.chapter_index);
    cue.insert(QStringLiteral("start"), match.start);
    cue.insert(QStringLiteral("length"), match.length);
    cue.insert(QStringLiteral("startTime"), match.start_time);
    cue.insert(QStringLiteral("endTime"), match.end_time);
    cue.insert(QStringLiteral("text"), QString::fromStdString(match.text));
    cue.insert(QStringLiteral("progress"),
               sasayaki::reader_progress(
                   match, ReaderViewModel::instance()->chapterCharacterCount(match.chapter_index)));
    return cue;
}

}  // namespace niratan::viewmodels

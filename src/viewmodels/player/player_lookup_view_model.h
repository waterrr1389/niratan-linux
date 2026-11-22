// SPDX-License-Identifier: GPL-3.0-or-later
//
// Click and Shift-hover lookup for the video subtitle overlay, following
// Features/Video/VideoLookupCoordinator.swift (present, closeAll, dismiss,
// resumePlaybackAfterPopupIfNeeded), Features/Popup/
// PopupPresentationCoordinator.swift (present, results.first,
// applyLookupMatch), PopupModels.swift (SelectionData.applyLookupMatch),
// VideoShiftHoverLookupState.swift (all members) and the click path of
// Features/Video/Subtitles/SubtitleOverlayView.swift.
//
// The port keeps at most one lookup popup and every subtitle click replaces
// it, which is the only path the video screen uses (replacingExisting: true).
// A lookup candidate comes from services/dictionary/text_selection with the
// active profile's language and scan length, and the popup is the
// DictionaryPopupModel the page shows.
//
// Pause and resume:
//   - setPlaying records the playback state the page reports.
//   - The first popup that a lookup produces pauses playback once when
//     auto-pause is on and the last reported state was playing; it also
//     remembers to resume. Replacement popups never pause.
//   - A no-result lookup closes the popup, clears the active cue and plays
//     again whenever the pause was remembered; a close does the same, and the
//     flag is then cleared, so a second close emits nothing.
//   - Resume does not depend on the later playback state: Swift plays whenever
//     shouldResumePlayback is set.
//
// lookupAt returns an empty map when there is no cue, no candidate or no
// dictionary result; otherwise text, utf16Start, matchedText, highlightStart
// and highlightLength. matchedText is the first dictionary result's matched
// expression, and the highlight is services/dictionary/text_selection
// highlight_range(candidate, matchedText).
//
// miningSelection is the active cue's mining selection over the whole parsed
// document, via services/player/video_mining resolve_mining_selection: the
// clicked cue's text and timing and its list neighbours. It is empty when no
// cue is active or no document is loaded. The mining-context selection sheet
// is out of scope (Decision D4).
//
// setEmbeddedSubtitle replaces the document with an extracted embedded SRT
// track's bytes, parsed as SRT regardless of a source name; empty bytes or a
// document with no cue leaves the cue store empty.

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::viewmodels {

class PlayerLookupViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList currentCues READ current_cues NOTIFY currentCuesChanged)
    Q_PROPERTY(QString activeCueId READ active_cue_id NOTIFY activeCueChanged)
    Q_PROPERTY(bool hasPopup READ has_popup NOTIFY popupChanged)
    Q_PROPERTY(QObject* popupModel READ popup_model NOTIFY popupChanged)
    Q_PROPERTY(int hoverDelayMs READ hover_delay_ms NOTIFY hoverDelayChanged)

   public:
    explicit PlayerLookupViewModel(QObject* parent = nullptr);
    ~PlayerLookupViewModel() override;

    QVariantList current_cues() const;
    QString active_cue_id() const;
    bool has_popup() const;
    QObject* popup_model();
    int hover_delay_ms() const;

    Q_INVOKABLE void setSubtitleDocument(const QUrl& file, const QByteArray& bytes, bool readable);
    Q_INVOKABLE void setEmbeddedSubtitle(const QByteArray& bytes);
    Q_INVOKABLE void updatePlayback(double position, double subtitleDelay);
    Q_INVOKABLE void setPlaying(bool playing);
    Q_INVOKABLE void setLookupSettings(bool autoPauseOnLookup, int hoverDelayMs);

    // Empty map when there is no candidate or no result. Otherwise: text,
    // utf16Start, matchedText, highlightStart, highlightLength.
    Q_INVOKABLE QVariantMap lookupAt(const QString& cueId, int utf16Offset);
    Q_INVOKABLE void closeLookup();

    // Empty when no cue is active or no document is loaded. Otherwise
    // cueText, cueStart, cueEnd, sentence and, when present, previousCueText
    // and nextCueText.
    Q_INVOKABLE QVariantMap miningSelection() const;

    Q_INVOKABLE bool setShiftPressed(bool pressed);
    Q_INVOKABLE bool setHoverPointAvailable(bool available);
    Q_INVOKABLE bool pointerMoved() const;
    Q_INVOKABLE void cancelShiftHover();

   signals:
    void pauseRequested();
    void playRequested();
    void currentCuesChanged();
    void activeCueChanged();
    void popupChanged();
    void hoverDelayChanged();

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::viewmodels

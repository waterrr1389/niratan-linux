// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows Features/Popup/PopupWebView.swift (Coordinator.userContentController
// and updateUIView), Features/Popup/PopupView.swift (buildContent, onMine,
// onMiningFeedback, mineEntry, showMiningToast and showAnkiNoteButton),
// Features/Popup/AnkiMining.swift (preflightAnkiMining, mineAnkiEntry and
// webPayload), Core/AnkiManager.swift (duplicateLookup, openNotesInAnki) and
// Features/Popup/popup.js (mineEntry, mineEntryAtIndex, openAnkiNoteAtIndex,
// createEntryHeader and applyAnkiDuplicateLookup).
//
// Mining accepts any string-to-string map, including absent or extra keys.
// Capture the active profile and the host context for the attempt. The
// services own profile activation, deck/model and duplicate preflight, and
// the add; the add must not repeat preflight. Publish the result toast before
// replying. Request IDs correlate replies with the page's promises; an
// invalid mine or duplicate body completes without a payload.
//
// Opening Anki is one-way on Linux: parse the string array as decimal int64,
// then browse once for the distinct positive IDs in order. The page releases
// text focus before dispatch; there is no AppKit activation here. Feedback is
// also one-way; ignore malformed bodies and unknown statuses. A missing message
// is empty and an invalid note ID is absent. Pending is only a toast status.
// A new toast replaces the current toast and restarts its 2.2-second lifetime.
// Linux omits the failed-feedback log and the native overlay buttons.
//
// The dictionary page has no document context. The reader supplies its
// available context: until the reader mining context is wired, the sentence
// and cover are empty and the title is the library display title rather than
// the EPUB title. The video screen supplies setVideoContext instead; it is
// cleared by setContext and clearVideoContext, so a reader or dictionary mine
// never carries video fields.
//
// A video mine follows PopupView.mineEntry: preflight first, then the capture,
// then the add with the same preflight already passed. The capture follows
// VideoMiningCoordinator.context: needs flags come from the field mappings,
// getMediaDirPath is requested only when a capture is needed, the frame and
// clip carry the deterministic direct-media names, and an existing
// destination is reused without a new capture. A requested screenshot emits
// screenshotRequested(path); the page writes the still there and answers
// screenshotFinished(ok). A clip is cut and encoded by services/player/
// audio_clip. Without a usable media directory the captured temporary paths
// reach the add as uploads. The card sees the same names, fields and failure
// messages as Swift.
//
// Screenshot and clip failures fail the add with the capture's message.
//
// A reader mine with an exact {sasayaki-audio} mapping and a cue from the
// popup requests the cue's clip after the preflight and adds only after the
// answer, with the profile captured at the start. The request carries the cue
// id, the selection's sentence and the settings' format and bitrate; an empty
// answer adds without audio. Without a cue, with no exact mapping, or on a
// video the add goes out as before, and a video keeps its video-audio bucket.
//
// Recorded differences: the frame and clip run one after the other rather
// than concurrently, with no video thumbnails to suspend, so the card waits
// for both the same way; imageCompressionQuality reaches the deterministic
// name but not the still, because mpv writes it directly (Decision D3). An
// AVIF request captures a JPEG still, and AVIF tokens remain name-only.
//
// The ordered term dictionaries come from the active profile's
// dictionary_config.json and the installed scan; they are reloaded before the
// popup options are computed and at the start of an add, then captured in the
// attempt so the card build does not read them again. The cover URL is for
// rendered text; an attachment needs a separate local cover path.
// The next mining, duplicate or browse command uses the shared transport
// saved by Anki settings. Changing the URL or key replaces the client without
// canceling requests that already went out on the previous transport.

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "models/anki/mining_context.h"
#include "services/anki/mining.h"

namespace niratan::viewmodels {

class MiningViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString toastStatus READ toast_status NOTIFY toastChanged)
    Q_PROPERTY(QString toastMessage READ toast_message NOTIFY toastChanged)
    Q_PROPERTY(QString toastNoteId READ toast_note_id NOTIFY toastChanged)

   public:
    explicit MiningViewModel(QObject* parent = nullptr);
    ~MiningViewModel() override;

    // Empty status means hidden. A note ID, if present, is a decimal string.
    QString toast_status() const;
    QString toast_message() const;
    QString toast_note_id() const;

    Q_INVOKABLE void setContext(const QString& sentence, const QString& documentTitle,
                                const QString& coverUrl, const QString& coverPath);
    void set_term_dictionaries(std::vector<anki::TermDictionary> dictionaries);

    // The lookup popup's cue for the next mine, set before every mineEntry.
    // Empty means the popup has no cue, so a {sasayaki-audio} mapping stays
    // out of the note.
    Q_INVOKABLE void setSasayakiCue(const QString& cueId);
    // The page's answer to sasayakiAudioRequested: the first pending mine adds
    // with these bytes, or without audio when they are empty. Ignored when no
    // mine waits.
    Q_INVOKABLE void sasayakiAudioReady(const QByteArray& data);

    // The video screen's mining context. Keys: path, title, audioStreamIndex,
    // cueText, cueStart, cueEnd, optional previousCueText/nextCueText,
    // subtitleDelay and duration. Anything that is not a map clears it; so
    // does setContext.
    Q_INVOKABLE void setVideoContext(const QVariant& video);
    Q_INVOKABLE void clearVideoContext();

    // The page's answer to screenshotRequested. Ignored when no capture is
    // waiting for it.
    Q_INVOKABLE void screenshotFinished(bool ok);
    // Empty means ffmpeg on PATH.
    void set_ffmpeg_path(QString ffmpeg);

    // Popup script globals from the active profile, before rendering entries.
    Q_INVOKABLE QVariantMap popupMiningOptions();

    Q_INVOKABLE void mineEntry(int requestId, const QVariant& content);
    Q_INVOKABLE void duplicateCheck(int requestId, const QVariant& word);
    Q_INVOKABLE void openAnkiNote(const QVariant& noteIds);
    Q_INVOKABLE void miningFeedback(const QVariant& result);

    // Context-mining confirmation can publish the note command to the host.
    void show_anki_note_button(int entry_index, qint64 note_id);

   signals:
    // Mine: status (added, duplicate or failed), message and optional decimal
    // string noteID. Duplicate: isDuplicate (bool) and noteIDs (decimal string
    // array). An invalid request body has no payload.
    void replyReady(int requestId, const QVariant& payload);
    void toastChanged();
    void ankiNoteButtonRequested(int entryIndex, const QString& noteId);
    // The page captures a still at path and then calls screenshotFinished.
    void screenshotRequested(const QString& path);
    // The mine waits for the popup cue's clip: the cue id, the selection's
    // sentence and the settings' format (0 aac, 1 mp3) and bitrate. The page
    // answers through sasayakiAudioReady.
    void sasayakiAudioRequested(const QString& cueId, const QString& sentence, int format,
                                int bitrate);

   private:
    struct State;
    // The popup cue and the mines waiting for its clip, in request order.
    struct SasayakiState {
        QString cue;
        std::deque<std::function<void(QByteArray data, std::string extension)>> pending;
    };
    std::unique_ptr<State> state_;
    // Null until setSasayakiCue or the first request.
    std::unique_ptr<SasayakiState> sasayaki_state_;

    // Enqueues a cue-audio answer when the popup has a cue and returns true;
    // otherwise the caller adds now and this returns false.
    bool request_cue_audio(const QString& sentence, int format, int bitrate,
                           std::function<void(QByteArray data, std::string extension)> add);
    // Builds the video capture request from State and starts it; completion
    // adds the note with the captured context.
    void start_video_capture(anki::MiningAttempt attempt,
                             std::function<void(anki::AnkiMiningResult)> finish);
};

}  // namespace niratan::viewmodels

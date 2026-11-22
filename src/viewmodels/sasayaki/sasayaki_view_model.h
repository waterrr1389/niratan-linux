// SPDX-License-Identifier: GPL-3.0-or-later
//
// State and commands for the Sasayaki audiobook player, following
// Features/Sasayaki/SasayakiPlayer.swift (SasayakiPlayer) and
// Features/Sasayaki/SasayakiSheet.swift (the playback header, the audio and
// subtitle pickers, the cue list and the rate and delay settings), and
// Features/Sasayaki/SasayakiMatchView.swift (matchFile and matchRate).
//
// The model owns one QMediaPlayer and plays a file the user picks from outside
// the book folder. It does not decode subtitles or match text: parse_cues()
// reads the SRT, CueTimeline answers cue lookups, and this class projects the
// result into QML. Playback follows the Swift player: the cue lookup time is
// position - delay, a seek target is a cue's start + delay, next and previous
// use the timeline rules, and seeks are clamped to 0..duration once the
// duration is known.
//
// The book root is the reader's open book folder: ReaderViewModel::book_folder()
// names the folder under Books/, and the sidecar files are
// sasayaki_playback.json and sasayaki_match.json inside it, read and written by
// books::BookStorage with the codecs in
// models/sasayaki/sasayaki.h. While a book is open, a folder that cannot be
// resolved is reported through error(); no title matching is involved, so two
// books with the same display title never share sidecars.
//
// Linux has no security-scoped bookmarks. audioBookmark stores the absolute
// path of the chosen audio file as UTF-8 bytes in Base64, the field's JSON
// representation, which is the port's analogue of the bookmark data: a
// bookmark for a file that moved is reported as missing instead of resolved.
//
// The matching worker aligns loaded SRT cues against filtered EPUB chapters.
// Only a nonempty, uncancelled result replaces sasayaki_match.json. Without a
// match file the SRT still drives playback and the cue list, but it has no
// chapter index or character offsets, so applyCuesJson() returns [].
//
// Persistence mirrors SasayakiPlayer.savePlayback: the position is written on
// each whole-second tick, on seek and on pause, with the delay and rate; the
// audio path is written when an audio file is opened. A damaged sidecar is
// left untouched and its defaults are used.
//
// Audiobook metadata and chapters come from services/audiobook when an audio
// file becomes the source, following SasayakiPlayer.loadAudiobookMetadata:
// the previous values are cleared, then the embedded title, artist and artwork
// and the chpl chapter list replace them, with isLoadingAudiobookChapters true
// while reading and chapters exposed as maps of id, title, startTime and
// endTime (null when unknown). currentAudiobookChapterId follows
// SasayakiPlayer.currentAudiobookChapterID. The sasayakiAutoScroll and
// sasayakiAutoPause preferences are read from the global settings document on
// every book load and by reloadSasayakiSettings(); a cue change reveals only
// when auto-scroll is on and playback has started. requestCueAudio exports the
// popup cue's sentence for the card through services/sasayaki/cue_audio;
// transcription is not part of this class.
//
// Playback keeps SasayakiPlayer's seek landing: a seek stores its target, the
// start-playback, update-cue and stop-time options and persists the target
// before moving the player; the next position tick within 0.75 s applies them,
// and the 1500 ms timer only drops a target that never landed. playCue(cueId,
// stop) arms the cue's end + delay as that stop time. flushPlayback writes a
// pending target as-is and never samples a paused player.
//
// The reader side is driven from ReaderPage, which owns this model: the page
// passes applyCuesJson() to window.hoshiReader.applySasayakiCues after each
// chapter loads, and forwards activeCueChanged, whose reveal flag it passes to
// highlightSasayakiCue. After matching, the panel asks the page to install the
// current chapter's new ranges, and the model re-emits activeCueChanged even
// if the current cue ID is unchanged. The model never touches the web view.
//
// The lookup popup asks findPopupCue() for the cue under the selection. It is
// SasayakiPlayer.findCue over the match data, only while audio is loaded, and
// carries the cue's reader progress so the popup can write the bookmark with
// ReaderViewModel.syncBookmarkToProgress before playCue(stop: false). An empty
// map means no cue or no audio.
//
// Cue following mirrors SasayakiPlayer.navigateToCue, prepareTransition and
// handleRestoreCompleted. A cue in the open chapter seeks to its start + delay.
// A cue in another chapter remembers it as the pending cue, with the reveal
// flag, and asks ReaderViewModel to load that chapter at the cue's reader
// progress; the model pauses before that load and resumes after the restore
// when playback was running or the cue asked to start it. The page calls
// prepareTransition() for every reader-initiated chapter change (the
// chapterTransitionStarted signal) and restoreCompleted() after it installs
// the restored chapter's cue ranges. Without match data the displayed cue does
// not change, as in Swift, while the SRT timeline still seeks and navigates.

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>

namespace niratan::sasayaki {
struct SasayakiCue;
struct SasayakiMatchData;
}

namespace niratan::viewmodels {

class SasayakiViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool hasAudio READ has_audio NOTIFY hasAudioChanged)
    Q_PROPERTY(bool hasMatch READ has_match NOTIFY hasMatchChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double rate READ rate NOTIFY rateChanged)
    Q_PROPERTY(double delay READ delay NOTIFY delayChanged)
    Q_PROPERTY(QString currentCueId READ current_cue_id NOTIFY currentCueChanged)
    Q_PROPERTY(QString currentCueText READ current_cue_text NOTIFY currentCueChanged)
    Q_PROPERTY(QVariantList cues READ cues NOTIFY cuesChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString audioName READ audio_name NOTIFY audioNameChanged)
    Q_PROPERTY(QString subtitleName READ subtitle_name NOTIFY subtitleNameChanged)
    Q_PROPERTY(bool matching READ matching NOTIFY matchingChanged)
    Q_PROPERTY(double matchProgress READ match_progress NOTIFY matchProgressChanged)
    Q_PROPERTY(int matchedCount READ matched_count NOTIFY matchDataChanged)
    Q_PROPERTY(int unmatchedCount READ unmatched_count NOTIFY matchDataChanged)
    Q_PROPERTY(int inferredCount READ inferred_count NOTIFY matchDataChanged)
    Q_PROPERTY(QString matchRateLabel READ match_rate_label NOTIFY matchDataChanged)
    Q_PROPERTY(QString matchError READ match_error NOTIFY matchErrorChanged)
    Q_PROPERTY(int matchSearchWindow READ match_search_window WRITE set_match_search_window
                   RESET reset_match_search_window NOTIFY matchSearchWindowChanged)
    Q_PROPERTY(QString audiobookTitle READ audiobook_title NOTIFY audiobookMetadataChanged)
    Q_PROPERTY(QString audiobookArtist READ audiobook_artist NOTIFY audiobookMetadataChanged)
    Q_PROPERTY(QByteArray audiobookArtwork READ audiobook_artwork NOTIFY audiobookMetadataChanged)
    Q_PROPERTY(QString audiobookArtworkSource READ audiobook_artwork_source NOTIFY
                   audiobookMetadataChanged)
    Q_PROPERTY(int currentAudiobookChapterId READ current_audiobook_chapter_id NOTIFY
                   audiobookChaptersChanged)
    Q_PROPERTY(bool isLoadingAudiobookChapters READ is_loading_audiobook_chapters NOTIFY
                   audiobookChaptersChanged)

   public:
    explicit SasayakiViewModel(QObject* parent = nullptr);
    ~SasayakiViewModel() override;

    bool has_audio() const;
    bool has_match() const;
    bool playing() const;
    double position() const;
    double duration() const;
    double rate() const;
    double delay() const;
    QString current_cue_id() const;
    QString current_cue_text() const;
    QVariantList cues() const;
    QVariantList chapters() const;
    QString error() const;
    QString audio_name() const;
    QString subtitle_name() const;
    bool matching() const;
    // Reports EPUB preparation and completion milestones, not per-cue progress.
    double match_progress() const;
    int matched_count() const;
    int unmatched_count() const;
    int inferred_count() const;
    QString match_rate_label() const;
    QString match_error() const;
    int match_search_window() const;
    void set_match_search_window(int value);
    void reset_match_search_window();

    // Embedded metadata of the current audio file; empty values when absent.
    QString audiobook_title() const;
    QString audiobook_artist() const;
    QByteArray audiobook_artwork() const;
    // The embedded artwork as a data: URL (PNG or JPEG by signature), else
    // empty.
    QString audiobook_artwork_source() const;
    // The id of the chapter containing the position, or -1 when none does.
    int current_audiobook_chapter_id() const;
    bool is_loading_audiobook_chapters() const;

    // Sets the audio source and stores its absolute path as the bookmark.
    // Keeps the current cue state when the file cannot be opened.
    Q_INVOKABLE bool openAudio(const QUrl& file);
    // Parses the SRT for playback and cue navigation. Ignored for display
    // while sasayaki_match.json supplied cues, which keep the chapter and
    // character offsets the reader needs.
    Q_INVOKABLE bool openSubtitles(const QUrl& file);
    Q_INVOKABLE void matchSubtitles(int searchWindow);
    Q_INVOKABLE void cancelSubtitleMatch();

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void seek(double seconds);
    // SasayakiPlayer.skip(forward:): 15 s, clamped to the duration.
    Q_INVOKABLE void skipForward(bool forward);
    // Seeks to the chapter's start (no delay) and clears a pending stop.
    Q_INVOKABLE void seekToAudiobookChapter(int chapterId);
    Q_INVOKABLE void previousCue();
    Q_INVOKABLE void nextCue();
    // Seeks to the current cue's start + delay, or to 0 when no cue is active.
    Q_INVOKABLE void replayCue();
    Q_INVOKABLE void jumpCue(const QString& cueId);
    Q_INVOKABLE void setRate(double rate);
    Q_INVOKABLE void setDelay(double delay);
    // Plays the cue with this id and starts playback on landing; `stop` arms
    // the cue's end + delay as the stop time. Unknown ids do nothing.
    Q_INVOKABLE void playCue(const QString& cueId, bool stop);
    // B13: pauses for a lookup and reports whether this call paused a playing
    // player; only such a pause is resumed by resumeAfterLookup().
    Q_INVOKABLE bool pauseForLookup();
    Q_INVOKABLE void resumeAfterLookup(bool was_paused_by_lookup);
    // Persists a pending seek target as-is, or the playing position. A paused
    // player writes nothing.
    Q_INVOKABLE void flushPlayback();
    // SasayakiPlayer.prepareTransition: remember whether playback was running,
    // mark the chapter transition, clear the stop boundary and the displayed
    // cue, and pause a playing player. Called by the page before a
    // reader-initiated chapter change.
    Q_INVOKABLE void prepareTransition();
    // SasayakiPlayer.handleRestoreCompleted(currentIndex:): choose the cue of
    // the restored chapter, display it, and resume playback when the
    // transition was playing. A book without match data leaves every flag
    // untouched.
    Q_INVOKABLE void restoreCompleted(int currentIndex);
    // Re-reads auto-scroll and auto-pause from the global settings document.
    Q_INVOKABLE void reloadSasayakiSettings();

    // The current chapter's cue ranges as reader.js applySasayakiCues expects:
    // a compact JSON array of {id, start, length}. Empty without match data.
    Q_INVOKABLE QString applyCuesJson() const;

    // The popup cue at the reader chapter and selection character offset, or
    // an empty map when audio, match data or a covering cue is missing. Keys:
    // id, chapterIndex, start, length, startTime, endTime, text and progress,
    // the cue's reader progress for the bookmark sync.
    Q_INVOKABLE QVariantMap findPopupCue(int chapterIndex, int offset) const;

    // Answers the popup's cue audio request once with the clip bytes, or with
    // empty bytes when there is no audio, the cue is unknown or the export
    // failed. The format is AudioCompressionFormat's order (0 aac, 1 mp3).
    Q_INVOKABLE void requestCueAudio(const QString& cueId, const QString& sentence, int format,
                                     int bitrate);

   signals:
    void hasAudioChanged();
    void hasMatchChanged();
    void playingChanged();
    void positionChanged();
    void durationChanged();
    void rateChanged();
    void delayChanged();
    void currentCueChanged();
    void cuesChanged();
    void chaptersChanged();
    void errorChanged();
    void audioNameChanged();
    void subtitleNameChanged();
    void matchingChanged();
    void matchProgressChanged();
    void matchDataChanged();
    void matchErrorChanged();
    void matchSearchWindowChanged();
    void audiobookMetadataChanged();
    void audiobookChaptersChanged();
    void matchApplied();
    // The cue to highlight, or an empty id to clear the highlight; `reveal`
    // scrolls to it only when auto-scroll is on and playback has started.
    void activeCueChanged(const QString& id, bool reveal);
    // One answer per requestCueAudio, in request order.
    void cueAudioReady(const QByteArray& data);

   private:
    struct State;
    std::unique_ptr<State> state_;

    void reload_book();
    void load_playback();
    bool load_match();
    void apply_match_result(const sasayaki::SasayakiMatchData& result);
    void restore_audio();
    void open_audio_source();
    void rebuild_timeline(bool refresh_highlight = false);
    void rebuild_cue_list();
    void update_cue(bool refresh_highlight = false);
    // SasayakiPlayer.tick: landing options first, then stop, persistence and
    // the cue.
    void position_tick(double seconds);
    void apply_position(double seconds);
    void persist_position(double seconds);
    void save_playback(double position);
    void update_audiobook_chapter();
    void seek(double seconds, bool start_playback, bool update_cue,
              std::optional<double> stop_playback_time);
    void navigate_to_cue(const sasayaki::SasayakiCue& cue);
    void navigate_to_cue(const sasayaki::SasayakiCue& cue, bool start_playback);
    // SasayakiPlayer.displayCue and clearDisplayedCue.
    void display_cue(const sasayaki::SasayakiCue& cue, bool reveal);
    void clear_displayed_cue();
    void set_error(const QString& message);
    void set_match_error(const QString& message);
    void set_match_progress(double progress);
    bool take_local_file(const QUrl& file, QString* path);
};

}  // namespace niratan::viewmodels

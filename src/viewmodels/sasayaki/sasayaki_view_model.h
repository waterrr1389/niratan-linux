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
// SasayakiPlayer.currentAudiobookChapterID. Auto-scroll, auto-pause on lookup,
// sentence audio mining and transcription are not part of this class.
//
// The reader side is driven from ReaderPage, which owns this model: the page
// passes applyCuesJson() to window.hoshiReader.applySasayakiCues after each
// chapter loads, and forwards activeCueChanged to highlightSasayakiCue or
// clearSasayakiCue. After matching, the panel asks the page to install the
// current chapter's new ranges, and the model re-emits activeCueChanged even
// if the current cue ID is unchanged. The model never touches the web view.

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

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
    // Index into chapters(), or -1 when no chapter contains the position.
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
    // Relative seek in seconds, positive forward.
    Q_INVOKABLE void skip(double seconds);
    Q_INVOKABLE void previousCue();
    Q_INVOKABLE void nextCue();
    // Seeks to the current cue's start + delay, or to 0 when no cue is active.
    Q_INVOKABLE void replayCue();
    Q_INVOKABLE void jumpCue(const QString& cueId);
    Q_INVOKABLE void setRate(double rate);
    Q_INVOKABLE void setDelay(double delay);

    // The current chapter's cue ranges as reader.js applySasayakiCues expects:
    // a compact JSON array of {id, start, length}. Empty without match data.
    Q_INVOKABLE QString applyCuesJson() const;

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
    // The cue to highlight, or an empty id to clear the highlight.
    void activeCueChanged(const QString& id);

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
    void apply_position(double seconds, bool persist);
    void persist_position(double seconds);
    void save_playback();
    void navigate_to_cue(const sasayaki::SasayakiCue& cue);
    void set_error(const QString& message);
    void set_match_error(const QString& message);
    void set_match_progress(double progress);
    bool take_local_file(const QUrl& file, QString* path);
};

}  // namespace niratan::viewmodels

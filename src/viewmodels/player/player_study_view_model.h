// SPDX-License-Identifier: GPL-3.0-or-later
//
// Study state follows VideoMiningHistorySidebar.swift (chapter and history
// lists), Models/Subtitle.swift (SubtitleTranscript and its window),
// VideoMiningHistoryStore.swift (record and pruning), and
// VideoMiningHistoryNavigation.swift (local history destinations).
// SubtitleTranscriptView.swift (transcriptRow) marks both cue endpoints
// inclusive at playback position minus subtitle delay. Follow playback uses
// SubtitleTranscript.nearestRowIndex: prefer the active cue, then the next
// cue in gaps, or the last cue after the transcript. Manual scrolling keeps
// the current window until following resumes. Chapter time is independent.
// Previous/next subtitle seeks and adjacent-cue alignment follow
// Models/Subtitle.swift (rowIndex(containing:), nearestRowIndex,
// relativeRowIndex), VideoPlayerScreen+Mining.swift (seekRelativeSubtitleCue)
// and SubtitleCueStore.delayAligningAdjacentCue. They use the full primary cue
// list, never the filtered window, and alignment reuses SubtitleCueStore
// rather than repeating the formula. Alignment emits the absolute delay for
// the page to close lookups, persist and show its OSD; it never seeks.
// The playback surface supplies subtitle bytes and native cue state and
// performs playback commands. Extracted cues retain their embedded track
// identity; their cache file is not an external subtitle source.

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::player {
enum class SubtitleOffsetDirection;
}

namespace niratan::viewmodels {

class PlayerStudyViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList transcript READ transcript NOTIFY transcriptChanged)
    Q_PROPERTY(QString transcriptError READ transcript_error NOTIFY transcriptChanged)
    Q_PROPERTY(double adjustedTime READ adjusted_time NOTIFY adjustedTimeChanged)
    Q_PROPERTY(int currentRow READ current_row NOTIFY currentRowChanged)
    Q_PROPERTY(int currentVisibleRow READ current_visible_row NOTIFY currentVisibleRowChanged)
    Q_PROPERTY(QString search READ search WRITE set_search NOTIFY searchChanged)
    Q_PROPERTY(bool canShowEarlier READ can_show_earlier NOTIFY transcriptChanged)
    Q_PROPERTY(bool canShowLater READ can_show_later NOTIFY transcriptChanged)
    Q_PROPERTY(bool hasTranscriptRows READ has_transcript_rows NOTIFY transcriptChanged)
    Q_PROPERTY(bool canAlignPreviousSubtitle READ can_align_previous_subtitle NOTIFY
                   alignmentAvailabilityChanged)
    Q_PROPERTY(
        bool canAlignNextSubtitle READ can_align_next_subtitle NOTIFY alignmentAvailabilityChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList chapters READ chapters WRITE set_chapters NOTIFY chaptersChanged)
    Q_PROPERTY(int currentChapter READ current_chapter NOTIFY currentChapterChanged)
    Q_PROPERTY(QString lastMiningNotice READ last_mining_notice NOTIFY lastMiningNoticeChanged)
    Q_PROPERTY(int historyLimit READ history_limit NOTIFY historyChanged)
    Q_PROPERTY(bool canMine READ can_mine NOTIFY canMineChanged)

public:
    explicit PlayerStudyViewModel(QObject* parent = nullptr);
    ~PlayerStudyViewModel() override;

    QVariantList transcript() const;
    QString transcript_error() const;
    double adjusted_time() const;
    int current_row() const;
    int current_visible_row() const;
    QString search() const;
    void set_search(const QString& query);
    bool can_show_earlier() const;
    bool can_show_later() const;
    bool has_transcript_rows() const;
    bool can_align_previous_subtitle() const;
    bool can_align_next_subtitle() const;
    QVariantList history() const;
    QVariantList chapters() const;
    void set_chapters(const QVariantList& chapters);
    int current_chapter() const;
    QString last_mining_notice() const;
    int history_limit() const;
    bool can_mine() const;
    Q_INVOKABLE void setTracks(const QVariantList& tracks);
    Q_INVOKABLE void setSubtitlesVisible(bool visible);

    Q_INVOKABLE void setVideo(const QUrl& video);
    Q_INVOKABLE void setPlaybackReady(bool ready);
    Q_INVOKABLE void updatePlayback(double position, double subtitleDelay);
    Q_INVOKABLE void updateNativeCue(const QString& text, double start, double end);
    Q_INVOKABLE void setSubtitleDocument(const QUrl& file, const QByteArray& bytes, bool readable);
    Q_INVOKABLE void setEmbeddedTranscript(const QByteArray& bytes, const QString& error);
    Q_INVOKABLE void seekToRow(int index);
    Q_INVOKABLE bool seekPreviousSubtitleCue();
    Q_INVOKABLE bool seekNextSubtitleCue();
    Q_INVOKABLE bool alignPreviousSubtitleToCurrentTime();
    Q_INVOKABLE bool alignNextSubtitleToCurrentTime();
    Q_INVOKABLE void setTranscriptFollowing(bool following);
    Q_INVOKABLE void showEarlier();
    Q_INVOKABLE void showLater();
    Q_INVOKABLE void playChapter(int index);
    Q_INVOKABLE void mineCurrentSubtitle();
    Q_INVOKABLE void jumpToHistory(const QString& id);
    Q_INVOKABLE void copyHistory(const QString& id) const;
    Q_INVOKABLE void removeHistory(const QString& id);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void refreshHistory();
    Q_INVOKABLE void reportSubtitleLoadFailure();

signals:
    void transcriptChanged();
    void adjustedTimeChanged();
    void currentRowChanged();
    void currentVisibleRowChanged();
    void searchChanged();
    void historyChanged();
    void chaptersChanged();
    void currentChapterChanged();
    void lastMiningNoticeChanged();
    void canMineChanged();
    void alignmentAvailabilityChanged();
    void subtitleAlignmentRequested(double delay);
    void seekRequested(double seconds);
    void chapterRequested(int id);
    void videoRequested(const QUrl& file);
    void historySubtitleRequested(const QUrl& file, int trackId, double seconds);

private:
    struct State;
    std::unique_ptr<State> state_;

    void update_current();
    void update_can_mine();
    void update_alignment_availability();
    int relative_row(int offset) const;
    bool align_subtitle(player::SubtitleOffsetDirection direction);
    void publish_transcript();
    void rebuild_filter();
    void reset_window(int focus);
    void publish_window();
    void set_notice(const QString& notice);
    void complete_jump();
};

}  // namespace niratan::viewmodels

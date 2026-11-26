// SPDX-License-Identifier: GPL-3.0-or-later
//
// B12 of notes/specs/viewmodels-player-lookup.md: PlayerStudyViewModel
// .setSubtitleDocument decides the format from the source path, through
// parse_subtitle_document without a hint (SubtitleParser.format(for:) in
// VideoPlayerScreen+Subtitles.swift), rejects a dotless name such as /tmp/ass,
// and records the parsed document's raw format in the mining history.

#include "viewmodels/player/player_study_view_model.h"

#include "data_root.h"
#include "services/player/mining_history_store.h"

#include <QUrl>
#include <QtTest>

#include <memory>
#include <optional>
#include <string>

using niratan::player::MiningHistoryStore;
using niratan::test::TempDataRoot;
using niratan::viewmodels::PlayerStudyViewModel;

namespace {

const QByteArray kAss = QByteArray(
    "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
    "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,星\n");
const QByteArray kSrt = QByteArray("1\n00:00:01,000 --> 00:00:02,000\n星\n");
const QByteArray kVtt = QByteArray("WEBVTT\n\n00:00.500 --> 00:02.000\n星\n");
const QByteArray kNavigation = QByteArray("1\n00:00:00,000 --> 00:00:01,500\nfirst\n\n"
                                          "2\n00:00:02,000 --> 00:00:04,000\nsecond\n\n"
                                          "3\n00:00:05,000 --> 00:00:07,000\nthird\n");
const QByteArray kTiedCues = QByteArray("1\n00:00:00,000 --> 00:00:01,000\nA\n\n"
                                        "2\n00:00:00,500 --> 00:00:01,000\nB\n\n"
                                        "3\n00:00:02,000 --> 00:00:03,000\nC\n\n"
                                        "4\n00:00:02,000 --> 00:00:02,500\nD\n");
const QByteArray kEndedRowHidesEarlierOverlap =
    QByteArray("1\n00:00:00,000 --> 00:00:10,000\nlong\n\n"
               "2\n00:00:02,000 --> 00:00:03,000\nshort\n");

QUrl file(const char* path) {
    return QUrl::fromLocalFile(QString::fromLatin1(path));
}

}  // namespace

class PlayerStudyViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void dotlessAndUnknownSourcesAreUnsupported();
    void validSrtGivesCues();
    void garbageAndUnreadableReportTheirErrors();
    void minedFormatComesFromTheParsedDocument();
    void relativeSeek_usesFullTranscriptAtDelayedPlayback();
    void relativeSeek_stopsAtEndedRowAndDoesNotWrap();
    void alignment_usesAdjacentGroupsAndAvailability();
    void transcriptFlags_ignoreSearchResults();

   private:
    std::unique_ptr<TempDataRoot> root_;
};

void PlayerStudyViewModelTest::initTestCase() {
    root_ = std::make_unique<TempDataRoot>();
    QVERIFY(root_->isValid());
}

void PlayerStudyViewModelTest::dotlessAndUnknownSourcesAreUnsupported() {
    PlayerStudyViewModel dotless;
    dotless.setSubtitleDocument(file("/tmp/ass"), kAss, true);
    QVERIFY(dotless.transcript().isEmpty());
    QCOMPARE(dotless.transcript_error(),
             QStringLiteral("This subtitle format is not supported for transcripts."));

    PlayerStudyViewModel unknown;
    unknown.setSubtitleDocument(file("/tmp/clip.txt"), kSrt, true);
    QVERIFY(unknown.transcript().isEmpty());
    QCOMPARE(unknown.transcript_error(),
             QStringLiteral("This subtitle format is not supported for transcripts."));
}

void PlayerStudyViewModelTest::validSrtGivesCues() {
    PlayerStudyViewModel model;
    model.setSubtitleDocument(file("/tmp/clip.srt"), kSrt, true);
    QVERIFY(!model.transcript().isEmpty());
    QVERIFY(model.transcript_error().isEmpty());
}

void PlayerStudyViewModelTest::garbageAndUnreadableReportTheirErrors() {
    PlayerStudyViewModel garbage;
    garbage.setSubtitleDocument(file("/tmp/clip.srt"), QByteArray("not a subtitle\n"), true);
    QVERIFY(garbage.transcript().isEmpty());
    QCOMPARE(garbage.transcript_error(),
             QStringLiteral("No readable cues were found in this subtitle file."));

    PlayerStudyViewModel unreadable;
    unreadable.setSubtitleDocument(file("/tmp/clip.srt"), kSrt, false);
    QVERIFY(unreadable.transcript().isEmpty());
    QCOMPARE(unreadable.transcript_error(), QStringLiteral("Could not read the subtitle file."));
}

void PlayerStudyViewModelTest::minedFormatComesFromTheParsedDocument() {
    QVERIFY(MiningHistoryStore::at_data_root().clear());
    PlayerStudyViewModel ass_model;
    ass_model.setVideo(file("/tmp/video.mkv"));
    ass_model.setPlaybackReady(true);
    ass_model.setSubtitlesVisible(true);
    ass_model.setSubtitleDocument(file("/tmp/clip.ass"), kAss, true);
    ass_model.updatePlayback(1.5, 0.0);
    ass_model.mineCurrentSubtitle();
    const auto ass_items = MiningHistoryStore::at_data_root().load_snapshot().items;
    QCOMPARE(ass_items.size(), 1u);
    QCOMPARE(ass_items.front().subtitle_format, std::optional<std::string>("ass"));

    QVERIFY(MiningHistoryStore::at_data_root().clear());
    PlayerStudyViewModel vtt_model;
    vtt_model.setVideo(file("/tmp/video.mkv"));
    vtt_model.setPlaybackReady(true);
    vtt_model.setSubtitlesVisible(true);
    vtt_model.setSubtitleDocument(file("/tmp/clip.vtt"), kVtt, true);
    vtt_model.updatePlayback(1.0, 0.0);
    vtt_model.mineCurrentSubtitle();
    const auto vtt_items = MiningHistoryStore::at_data_root().load_snapshot().items;
    QCOMPARE(vtt_items.size(), 1u);
    QCOMPARE(vtt_items.front().subtitle_format, std::optional<std::string>("webVTT"));

    QVERIFY(MiningHistoryStore::at_data_root().clear());
    PlayerStudyViewModel embedded;
    embedded.setVideo(file("/tmp/video.mkv"));
    embedded.setPlaybackReady(true);
    embedded.setEmbeddedTranscript(kSrt, {});
    embedded.updatePlayback(1.5, 0.0);
    embedded.mineCurrentSubtitle();
    const auto embedded_items = MiningHistoryStore::at_data_root().load_snapshot().items;
    QCOMPARE(embedded_items.size(), 1u);
    QCOMPARE(embedded_items.front().subtitle_format, std::optional<std::string>("embedded"));
}

void PlayerStudyViewModelTest::relativeSeek_usesFullTranscriptAtDelayedPlayback() {
    PlayerStudyViewModel model;
    QSignalSpy seeks(&model, &PlayerStudyViewModel::seekRequested);
    model.setVideo(file("/tmp/navigation.mkv"));
    model.setPlaybackReady(true);
    model.setSubtitleDocument(file("/tmp/navigation.srt"), kNavigation, true);

    model.updatePlayback(3.25, 0.0);
    QCOMPARE(model.current_row(), 1);
    QVERIFY(model.seekPreviousSubtitleCue());
    QCOMPARE(seeks.size(), 1);
    QCOMPARE(seeks.takeFirst().front().toDouble(), 0.0);
    QVERIFY(model.seekNextSubtitleCue());
    QCOMPARE(seeks.size(), 1);
    QCOMPARE(seeks.takeFirst().front().toDouble(), 5.0);

    // In a gap the anchor is the next cue, so +1 moves past it.
    model.updatePlayback(1.75, 0.0);
    QVERIFY(model.seekNextSubtitleCue());
    QCOMPARE(seeks.size(), 1);
    QCOMPARE(seeks.takeFirst().front().toDouble(), 5.0);

    // Filtering and the visible window never change which cue is adjacent.
    model.set_search(QStringLiteral("third"));
    QVERIFY(model.transcript().size() == 1);
    model.updatePlayback(3.75, 0.5);
    QVERIFY(model.seekPreviousSubtitleCue());
    QCOMPARE(seeks.size(), 1);
    QCOMPARE(seeks.takeFirst().front().toDouble(), 0.5);

    // Both cue endpoints are inclusive.
    model.set_search({});
    model.updatePlayback(4.0, 0.0);
    QCOMPARE(model.current_row(), 1);
    QVERIFY(model.seekPreviousSubtitleCue());
    QCOMPARE(seeks.size(), 1);
    QCOMPARE(seeks.takeFirst().front().toDouble(), 0.0);

    PlayerStudyViewModel empty;
    QSignalSpy empty_seeks(&empty, &PlayerStudyViewModel::seekRequested);
    QVERIFY(!empty.seekPreviousSubtitleCue());
    QVERIFY(!empty.seekNextSubtitleCue());
    QCOMPARE(empty_seeks.size(), 0);
}

void PlayerStudyViewModelTest::relativeSeek_stopsAtEndedRowAndDoesNotWrap() {
    PlayerStudyViewModel model;
    QSignalSpy seeks(&model, &PlayerStudyViewModel::seekRequested);
    model.setVideo(file("/tmp/overlap.mkv"));
    model.setPlaybackReady(true);
    model.setSubtitleDocument(file("/tmp/overlap.srt"), kEndedRowHidesEarlierOverlap, true);
    model.updatePlayback(4.0, 0.0);

    // The examined short cue ended before 4.0, so the earlier long overlap is
    // not the containing row (Subtitle.rowIndex(containing:)).
    QVERIFY(model.seekPreviousSubtitleCue());
    QCOMPARE(seeks.size(), 1);
    QCOMPARE(seeks.takeFirst().front().toDouble(), 0.0);
    QVERIFY(!model.seekNextSubtitleCue());
    QCOMPARE(seeks.size(), 0);

    model.setSubtitleDocument(file("/tmp/navigation.srt"), kNavigation, true);
    model.updatePlayback(0.5, 0.0);
    QVERIFY(!model.seekPreviousSubtitleCue());
    model.updatePlayback(7.5, 0.0);
    QVERIFY(!model.seekNextSubtitleCue());
    QVERIFY(model.seekPreviousSubtitleCue());
    QCOMPARE(seeks.size(), 1);
    QCOMPARE(seeks.takeFirst().front().toDouble(), 2.0);
}

void PlayerStudyViewModelTest::alignment_usesAdjacentGroupsAndAvailability() {
    PlayerStudyViewModel model;
    QSignalSpy alignments(&model, &PlayerStudyViewModel::subtitleAlignmentRequested);
    QSignalSpy seeks(&model, &PlayerStudyViewModel::seekRequested);
    model.setVideo(file("/tmp/ties.mkv"));
    model.setPlaybackReady(true);
    model.setSubtitleDocument(file("/tmp/ties.srt"), kTiedCues, true);
    model.updatePlayback(1.5, 0.0);

    QVERIFY(model.can_align_previous_subtitle());
    QVERIFY(model.can_align_next_subtitle());
    QVERIFY(model.alignPreviousSubtitleToCurrentTime());
    QCOMPARE(alignments.size(), 1);
    QCOMPARE(alignments.takeFirst().front().toDouble(), 1.0);
    QVERIFY(model.alignNextSubtitleToCurrentTime());
    QCOMPARE(alignments.size(), 1);
    QCOMPARE(alignments.takeFirst().front().toDouble(), -0.5);
    QCOMPARE(seeks.size(), 0);

    model.updatePlayback(0.0, 0.0);
    QVERIFY(!model.can_align_previous_subtitle());
    QVERIFY(model.can_align_next_subtitle());
    QVERIFY(!model.alignPreviousSubtitleToCurrentTime());
    QCOMPARE(alignments.size(), 0);
    QVERIFY(model.alignNextSubtitleToCurrentTime());
    QCOMPARE(alignments.size(), 1);
    QCOMPARE(alignments.takeFirst().front().toDouble(), -0.5);

    model.updatePlayback(8.0, 0.0);
    QVERIFY(model.can_align_previous_subtitle());
    QVERIFY(!model.can_align_next_subtitle());
    QVERIFY(model.alignPreviousSubtitleToCurrentTime());
    QCOMPARE(alignments.size(), 1);
    QCOMPARE(alignments.takeFirst().front().toDouble(), 6.0);
    QVERIFY(!model.alignNextSubtitleToCurrentTime());
    QCOMPARE(alignments.size(), 0);

    // The adjacent cue is found at playback minus subtitle delay, but the
    // emitted delay is playback time minus that cue's start.
    model.updatePlayback(3.75, 0.5);
    QVERIFY(model.alignPreviousSubtitleToCurrentTime());
    QCOMPARE(alignments.size(), 1);
    QCOMPARE(alignments.takeFirst().front().toDouble(), 1.75);
}

void PlayerStudyViewModelTest::transcriptFlags_ignoreSearchResults() {
    PlayerStudyViewModel model;
    model.setSubtitleDocument(file("/tmp/navigation.srt"), kNavigation, true);
    QVERIFY(model.has_transcript_rows());

    model.set_search(QStringLiteral("no match"));
    QVERIFY(model.transcript().isEmpty());
    QVERIFY(model.has_transcript_rows());

    model.set_search({});
    model.updatePlayback(1.0, 0.0);
    QVERIFY(model.has_transcript_rows());
}

QTEST_GUILESS_MAIN(PlayerStudyViewModelTest)
#include "tst_player_study_view_model.moc"

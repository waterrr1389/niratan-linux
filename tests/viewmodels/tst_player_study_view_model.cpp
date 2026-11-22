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

QTEST_GUILESS_MAIN(PlayerStudyViewModelTest)
#include "tst_player_study_view_model.moc"

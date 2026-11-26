// SPDX-License-Identifier: GPL-3.0-or-later
//
// B1-B3, B5, B6 of notes/specs/video-v5-transcript-search.md, on top of the
// V1-V4 study behaviours: VideoStudySearch matching, SubtitleTranscriptView's
// search results and following resets, and VideoMiningHistorySidebar's current
// chapter progress over raw playback time.

#include "viewmodels/player/player_study_view_model.h"

#include "data_root.h"
#include "services/player/mining_history.h"
#include "services/player/mining_history_store.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

using niratan::player::MiningHistoryItem;
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

// One cue per two seconds, numbered from one, with the given text.
QByteArray numbered_srt(int count, const std::function<QString(int)>& text) {
    const auto stamp = [](int seconds) {
        return QStringLiteral("00:%1:%2,000")
            .arg(seconds / 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'))
            .toUtf8();
    };
    QByteArray out;
    for (int i = 0; i < count; ++i) {
        out += QByteArray::number(i + 1) + '\n' + stamp(i * 2) + " --> " + stamp(i * 2 + 1) + '\n' +
            text(i).toUtf8() + "\n\n";
    }
    return out;
}

// One mining history record; video_title and video_file_name share the title.
niratan::player::MiningHistoryItem history_item(const char* id, double created_at,
                                                const char* title, const char* text) {
    niratan::player::MiningHistoryItem item;
    item.id = id;
    item.created_at = created_at;
    item.video_file_name = title;
    item.video_title = title;
    item.subtitle_source_name = title;
    item.subtitle_text = text;
    item.cue_start = 1.0;
    item.cue_end = 2.0;
    return item;
}

QVariantMap chapter_row(int id, const char* title, double start, double duration) {
    QVariantMap row{{QStringLiteral("id"), id},
                    {QStringLiteral("title"), QString::fromLatin1(title)},
                    {QStringLiteral("start"), start}};
    if (duration >= 0.0) {
        row.insert(QStringLiteral("duration"), duration);
    }
    return row;
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
    void searchListsEveryMatchInOrder();
    void searchRowsCarryHighlightRanges();
    void clearedSearchResumesFollowing();
    void newTranscriptResumesFollowing();
    void chapterProgressUsesRawPlayback();
    void historySearchFiltersAndGroupsSections();
    void historyJumpReportsResolvedDestinations();
    void historyJumpReportsMissingSources();

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

// B1, B2: a trimmed query lists every match at once, in transcript order, with
// no window paging; an empty or unmatchable query lists no rows.
void PlayerStudyViewModelTest::searchListsEveryMatchInOrder() {
    PlayerStudyViewModel model;
    model.setSubtitleDocument(file("/tmp/search.srt"),
                              numbered_srt(200,
                                           [](int i) {
                                               return i % 2 == 0
                                                   ? QStringLiteral("needle %1").arg(i)
                                                   : QStringLiteral("hay %1").arg(i);
                                           }),
                              true);
    QCOMPARE(model.transcript().size(), qsizetype(80));

    model.set_search(QStringLiteral("  NEEDLE\n"));
    QVERIFY(model.search_active());
    QCOMPARE(model.search_match_count(), 100);
    const QVariantList matches = model.transcript();
    QCOMPARE(matches.size(), qsizetype(100));
    QVERIFY(!model.can_show_earlier());
    QVERIFY(!model.can_show_later());
    for (qsizetype i = 0; i < matches.size(); ++i) {
        const QVariantMap row = matches.at(i).toMap();
        QCOMPARE(row.value(QStringLiteral("index")).toInt(), int(i * 2));
        QVERIFY(row.value(QStringLiteral("text")).toString().contains(QStringLiteral("needle")));
    }

    model.set_search(QStringLiteral("absent"));
    QVERIFY(model.search_active());
    QCOMPARE(model.search_match_count(), 0);
    QVERIFY(model.transcript().isEmpty());

    model.set_search(QString());
    QVERIFY(!model.search_active());
    QCOMPARE(model.search_match_count(), 0);
    QCOMPARE(model.transcript().size(), qsizetype(80));
}

// B4: listed rows carry the highlight ranges in the original cue text.
void PlayerStudyViewModelTest::searchRowsCarryHighlightRanges() {
    PlayerStudyViewModel model;
    model.setSubtitleDocument(file("/tmp/highlight.srt"),
                              QByteArray("1\n00:00:01,000 --> 00:00:02,000\none two one\n"), true);
    model.set_search(QStringLiteral("ONE"));
    const QVariantList rows = model.transcript();
    QCOMPARE(rows.size(), qsizetype(1));
    const QVariantList highlights =
        rows.first().toMap().value(QStringLiteral("highlights")).toList();
    QCOMPARE(highlights.size(), qsizetype(2));
    QCOMPARE(highlights.at(0).toMap().value(QStringLiteral("start")).toInt(), 0);
    QCOMPARE(highlights.at(0).toMap().value(QStringLiteral("length")).toInt(), 3);
    QCOMPARE(highlights.at(1).toMap().value(QStringLiteral("start")).toInt(), 8);
    QCOMPARE(highlights.at(1).toMap().value(QStringLiteral("length")).toInt(), 3);
    QCOMPARE(rows.first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("one two one"));

    model.set_search({});
    const QVariantList unfiltered = model.transcript();
    QVERIFY(unfiltered.first().toMap().value(QStringLiteral("highlights")).toList().isEmpty());
}

// B5: clearing a query turns following back on and recenters the window, so
// playback moves the list again.
void PlayerStudyViewModelTest::clearedSearchResumesFollowing() {
    PlayerStudyViewModel model;
    model.setSubtitleDocument(
        file("/tmp/many.srt"),
        numbered_srt(200, [](int i) { return QStringLiteral("line %1").arg(i); }), true);
    model.updatePlayback(241.0, 0.0);
    QVERIFY(model.current_visible_row() >= 0);
    model.setTranscriptFollowing(false);

    model.set_search(QStringLiteral("line 1"));
    QVERIFY(model.search_active());
    model.set_search({});
    QVERIFY(!model.search_active());
    QVERIFY(model.current_visible_row() >= 0);
    QVERIFY(model.can_show_earlier());
    QVERIFY(model.can_show_later());

    model.updatePlayback(361.0, 0.0);
    QVERIFY(!model.can_show_later());
}

// B5: a new transcript also resumes following.
void PlayerStudyViewModelTest::newTranscriptResumesFollowing() {
    PlayerStudyViewModel model;
    model.setSubtitleDocument(
        file("/tmp/a.srt"),
        numbered_srt(200, [](int i) { return QStringLiteral("line %1").arg(i); }), true);
    model.updatePlayback(21.0, 0.0);
    model.setTranscriptFollowing(false);
    model.setSubtitleDocument(
        file("/tmp/b.srt"),
        numbered_srt(200, [](int i) { return QStringLiteral("line %1").arg(i); }), true);
    model.updatePlayback(361.0, 0.0);
    QVERIFY(!model.can_show_later());
}

// B6: current-chapter progress uses raw playback time, the next later start or
// the chapter duration as its end, clamps to 0..1 and is unavailable otherwise.
void PlayerStudyViewModelTest::chapterProgressUsesRawPlayback() {
    PlayerStudyViewModel model;
    model.set_chapters({chapter_row(0, "A", 0, 10), chapter_row(1, "B", 10, 10)});
    model.updatePlayback(15.0, 3.0);
    QCOMPARE(model.current_chapter(), 1);
    QCOMPARE(model.current_chapter_progress(), 0.5);

    QSignalSpy progress(&model, &PlayerStudyViewModel::currentChapterProgressChanged);
    model.updatePlayback(17.0, 3.0);
    QCOMPARE(progress.count(), 1);
    QCOMPARE(model.current_chapter_progress(), 0.7);

    // The next later start wins over a longer chapter duration.
    model.set_chapters({chapter_row(0, "A", 0, 100), chapter_row(1, "B", 10, 5)});
    model.updatePlayback(5.0, 0.0);
    QCOMPARE(model.current_chapter(), 0);
    QCOMPARE(model.current_chapter_progress(), 0.5);

    model.set_chapters({chapter_row(0, "A", 20, 10)});
    model.updatePlayback(35.0, 0.0);
    QCOMPARE(model.current_chapter(), 0);
    QCOMPARE(model.current_chapter_progress(), 1.0);

    model.set_chapters({chapter_row(0, "A", 0, -1)});
    model.updatePlayback(5.0, 0.0);
    QCOMPARE(model.current_chapter(), 0);
    QCOMPARE(model.current_chapter_progress(), -1.0);

    model.set_chapters({});
    QCOMPARE(model.current_chapter(), -1);
    QCOMPARE(model.current_chapter_progress(), -1.0);
}

// B1, B2, B4: the history query is separate from the transcript search,
// matches subtitle text or video title, keeps newest-first order and groups
// only consecutive runs of the same video title; rows carry their ranges.
void PlayerStudyViewModelTest::historySearchFiltersAndGroupsSections() {
    auto store = MiningHistoryStore::at_data_root();
    QVERIFY(store.clear());
    QVERIFY(store.add(history_item("a-old", 100.0, "A.mkv", "alpha needle"), 25));
    QVERIFY(store.add(history_item("b", 200.0, "B.mkv", "beta"), 25));
    QVERIFY(store.add(history_item("a-new", 300.0, "A.mkv", "alpha again"), 25));

    PlayerStudyViewModel model;
    QCOMPARE(model.history_search(), QString());
    QVERIFY(!model.history_search_active());
    QVariantList sections = model.history_sections();
    QCOMPARE(sections.size(), qsizetype(3));
    QCOMPARE(sections.at(0).toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("A.mkv"));
    QCOMPARE(sections.at(1).toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("B.mkv"));
    QCOMPARE(sections.at(2).toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("A.mkv"));
    for (const QVariant& section : sections) {
        QCOMPARE(section.toMap().value(QStringLiteral("count")).toInt(), 1);
    }
    QCOMPARE(sections.at(0)
                 .toMap()
                 .value(QStringLiteral("items"))
                 .toList()
                 .at(0)
                 .toMap()
                 .value(QStringLiteral("id"))
                 .toString(),
             QStringLiteral("a-new"));

    // A text match highlights the original characters.
    QSignalSpy sections_changed(&model, &PlayerStudyViewModel::historySectionsChanged);
    model.set_history_search(QStringLiteral("  needle "));
    QCOMPARE(sections_changed.count(), 1);
    QCOMPARE(model.history_search(), QStringLiteral("  needle "));
    QVERIFY(model.history_search_active());
    sections = model.history_sections();
    QCOMPARE(sections.size(), qsizetype(1));
    QCOMPARE(sections.at(0).toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("A.mkv"));
    const QVariantMap row =
        sections.at(0).toMap().value(QStringLiteral("items")).toList().first().toMap();
    QCOMPARE(row.value(QStringLiteral("id")).toString(), QStringLiteral("a-old"));
    QCOMPARE(row.value(QStringLiteral("text")).toString(), QStringLiteral("alpha needle"));
    const QVariantList highlights = row.value(QStringLiteral("highlights")).toList();
    QCOMPARE(highlights.size(), qsizetype(1));
    QCOMPARE(highlights.at(0).toMap().value(QStringLiteral("start")).toInt(), 6);
    QCOMPARE(highlights.at(0).toMap().value(QStringLiteral("length")).toInt(), 6);

    // A title match removes the other video between the two A records, so the
    // filtered records form one section.
    model.set_history_search(QStringLiteral(" a.mkv "));
    QVERIFY(model.history_search_active());
    sections = model.history_sections();
    QCOMPARE(sections.size(), qsizetype(1));
    QCOMPARE(sections.at(0).toMap().value(QStringLiteral("count")).toInt(), 2);
    const QVariantList title_items = sections.at(0).toMap().value(QStringLiteral("items")).toList();
    QCOMPARE(title_items.at(0).toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("a-new"));
    QCOMPARE(title_items.at(1).toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("a-old"));
    // The query matched the video title, so no subtitle text is highlighted.
    for (const QVariant& item : title_items) {
        QVERIFY(item.toMap().value(QStringLiteral("highlights")).toList().isEmpty());
    }

    // No match leaves the unfiltered history alone but shows no sections.
    model.set_history_search(QStringLiteral("missing"));
    QVERIFY(model.history_search_active());
    QVERIFY(model.history_sections().isEmpty());
    QCOMPARE(model.history().size(), qsizetype(3));

    // Whitespace clears the filter.
    model.set_history_search(QStringLiteral("   "));
    QVERIFY(!model.history_search_active());
    QCOMPARE(model.history_sections().size(), qsizetype(3));

    // The transcript query is independent.
    model.set_search(QStringLiteral("needle"));
    QCOMPARE(model.history_sections().size(), qsizetype(3));
    QVERIFY(!model.history_search_active());
}

// B5, B6: a ready record requests the stored video when it is not open, then
// the resolved subtitle or embedded track and the delayed seek time.
void PlayerStudyViewModelTest::historyJumpReportsResolvedDestinations() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString video = directory.filePath(QStringLiteral("Episode 1.mkv"));
    const QString subtitle = directory.filePath(QStringLiteral("Episode 1.ja.srt"));
    for (const QString& path : {video, subtitle}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArrayLiteral("x"));
    }
    const QString canonical_video = QFileInfo(video).canonicalFilePath();
    const QString canonical_subtitle = QFileInfo(subtitle).canonicalFilePath();

    auto store = MiningHistoryStore::at_data_root();
    QVERIFY(store.clear());
    MiningHistoryItem ready = history_item("ready", 100.0, "Episode 1.mkv", "ready text");
    ready.video_path = canonical_video.toStdString();
    ready.subtitle_source_path = canonical_subtitle.toStdString();
    ready.subtitle_source_name = "Episode 1.ja.srt";
    ready.subtitle_format = "srt";
    ready.cue_start = 12.5;
    QVERIFY(store.add(ready, 25));
    MiningHistoryItem embedded = history_item("embedded", 200.0, "Episode 1.mkv", "embedded text");
    embedded.video_path = canonical_video.toStdString();
    // An embedded format carries no subtitle file even when a path is stored.
    embedded.subtitle_source_path = canonical_subtitle.toStdString();
    embedded.subtitle_format = "embedded";
    embedded.embedded_subtitle_track_id = 4;
    embedded.cue_start = 3.0;
    QVERIFY(store.add(embedded, 25));
    MiningHistoryItem legacy = history_item("legacy", 300.0, "Episode 1.mkv", "legacy text");
    legacy.cue_start = 12.5;
    QVERIFY(store.add(legacy, 25));

    PlayerStudyViewModel model;
    QSignalSpy video_requests(&model, &PlayerStudyViewModel::videoRequested);
    QSignalSpy subtitle_requests(&model, &PlayerStudyViewModel::historySubtitleRequested);

    model.updatePlayback(0.0, 0.5);
    model.jumpToHistory(QStringLiteral("ready"));
    QCOMPARE(video_requests.size(), 1);
    QCOMPARE(video_requests.takeFirst().front().toUrl(), QUrl::fromLocalFile(canonical_video));
    QCOMPARE(subtitle_requests.size(), 0);
    model.setVideo(QUrl::fromLocalFile(canonical_video));
    model.setPlaybackReady(true);
    QCOMPARE(subtitle_requests.size(), 1);
    QList<QVariant> request = subtitle_requests.takeFirst();
    QCOMPARE(request.at(0).toUrl(), QUrl::fromLocalFile(canonical_subtitle));
    QCOMPARE(request.at(1).toInt(), -1);
    QCOMPARE(request.at(2).toDouble(), 13.0);
    QVERIFY(model.last_mining_notice().isEmpty());

    // The same video jumps in place.
    model.updatePlayback(0.0, 0.5);
    model.jumpToHistory(QStringLiteral("ready"));
    QCOMPARE(video_requests.size(), 0);
    QCOMPARE(subtitle_requests.size(), 1);
    request = subtitle_requests.takeFirst();
    QCOMPARE(request.at(0).toUrl(), QUrl::fromLocalFile(canonical_subtitle));
    QCOMPARE(request.at(1).toInt(), -1);
    QCOMPARE(request.at(2).toDouble(), 13.0);

    // A current video spelled with a redundant segment or opened through a
    // symlink is the same media, so the jump does not request a reopen.
    const QString other_video = directory.filePath(QStringLiteral("Other.mkv"));
    {
        QFile file(other_video);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArrayLiteral("x"));
    }
    const QString link = directory.filePath(QStringLiteral("Episode 1 link.mkv"));
    std::error_code link_error;
    std::filesystem::create_symlink(std::filesystem::path(canonical_video.toStdString()),
                                    std::filesystem::path(link.toStdString()), link_error);
    QVERIFY(!link_error);

    model.setVideo(QUrl::fromLocalFile(directory.filePath(QStringLiteral("./Episode 1.mkv"))));
    model.setPlaybackReady(true);
    model.updatePlayback(0.0, 0.5);
    model.jumpToHistory(QStringLiteral("ready"));
    QCOMPARE(video_requests.size(), 0);
    QCOMPARE(subtitle_requests.size(), 1);
    subtitle_requests.clear();

    model.setVideo(QUrl::fromLocalFile(link));
    model.setPlaybackReady(true);
    model.updatePlayback(0.0, 0.5);
    model.jumpToHistory(QStringLiteral("ready"));
    QCOMPARE(video_requests.size(), 0);
    QCOMPARE(subtitle_requests.size(), 1);
    subtitle_requests.clear();

    // With another video open, the reopen keeps the pending jump when the same
    // file arrives under a symlink path.
    model.setVideo(QUrl::fromLocalFile(other_video));
    model.setPlaybackReady(true);
    model.updatePlayback(0.0, 0.5);
    model.jumpToHistory(QStringLiteral("ready"));
    QCOMPARE(video_requests.size(), 1);
    video_requests.clear();
    QCOMPARE(subtitle_requests.size(), 0);
    model.setVideo(QUrl::fromLocalFile(link));
    model.setPlaybackReady(true);
    QCOMPARE(subtitle_requests.size(), 1);
    QCOMPARE(subtitle_requests.takeFirst().at(2).toDouble(), 13.0);

    // An embedded record carries its track id and no subtitle file.
    model.jumpToHistory(QStringLiteral("embedded"));
    QCOMPARE(video_requests.size(), 0);
    QCOMPARE(subtitle_requests.size(), 1);
    request = subtitle_requests.takeFirst();
    QCOMPARE(request.at(0).toUrl(), QUrl());
    QCOMPARE(request.at(1).toInt(), 4);
    QCOMPARE(request.at(2).toDouble(), 3.5);

    // A pathless record with the current file name jumps in this video.
    model.setVideo(QUrl::fromLocalFile(canonical_video));
    model.setPlaybackReady(true);
    model.updatePlayback(0.0, 0.5);
    model.jumpToHistory(QStringLiteral("legacy"));
    QCOMPARE(video_requests.size(), 0);
    QCOMPARE(subtitle_requests.size(), 1);
    request = subtitle_requests.takeFirst();
    QCOMPARE(request.at(0).toUrl(), QUrl());
    QCOMPARE(request.at(1).toInt(), -1);
    QCOMPARE(request.at(2).toDouble(), 13.0);
}

// B5, B6: missing and legacy sources set the exact notice and request no
// playback; a remote record keeps its existing notice.
void PlayerStudyViewModelTest::historyJumpReportsMissingSources() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString video = directory.filePath(QStringLiteral("Episode 1.mkv"));
    const QString subtitle = directory.filePath(QStringLiteral("Episode 1.ja.srt"));
    for (const QString& path : {video, subtitle}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArrayLiteral("x"));
    }
    const QString canonical_video = QFileInfo(video).canonicalFilePath();

    auto store = MiningHistoryStore::at_data_root();
    QVERIFY(store.clear());
    MiningHistoryItem missing_video = history_item("missing-video", 100.0, "Episode 1.mkv", "text");
    missing_video.video_path = directory.filePath(QStringLiteral("Missing.mkv")).toStdString();
    missing_video.subtitle_source_path = QFileInfo(subtitle).canonicalFilePath().toStdString();
    missing_video.subtitle_format = "srt";
    QVERIFY(store.add(missing_video, 25));
    MiningHistoryItem missing_subtitle =
        history_item("missing-subtitle", 200.0, "Episode 1.mkv", "text");
    missing_subtitle.video_path = canonical_video.toStdString();
    missing_subtitle.subtitle_source_path =
        directory.filePath(QStringLiteral("Missing.srt")).toStdString();
    missing_subtitle.subtitle_format = "srt";
    QVERIFY(store.add(missing_subtitle, 25));
    MiningHistoryItem legacy = history_item("legacy", 300.0, "Other.mkv", "text");
    QVERIFY(store.add(legacy, 25));
    MiningHistoryItem remote = history_item("remote", 400.0, "Remote.mkv", "text");
    MiningHistoryItem::RemoteVideoIdentity identity;
    identity.provider_id = "youtube";
    identity.remote_id = "dQw4w9WgXcQ";
    identity.original_url = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";
    identity.title = "Remote";
    remote.remote_video_identity = identity;
    QVERIFY(store.add(remote, 25));

    PlayerStudyViewModel model;
    model.setVideo(QUrl::fromLocalFile(canonical_video));
    model.setPlaybackReady(true);
    QSignalSpy video_requests(&model, &PlayerStudyViewModel::videoRequested);
    QSignalSpy subtitle_requests(&model, &PlayerStudyViewModel::historySubtitleRequested);

    model.jumpToHistory(QStringLiteral("missing-video"));
    QCOMPARE(model.last_mining_notice(),
             QStringLiteral("The saved video file is no longer available. Open it again to "
                            "continue."));
    model.jumpToHistory(QStringLiteral("missing-subtitle"));
    QCOMPARE(model.last_mining_notice(),
             QStringLiteral("The saved subtitle file is no longer available. Open it again to "
                            "continue."));
    model.jumpToHistory(QStringLiteral("legacy"));
    QCOMPARE(model.last_mining_notice(),
             QStringLiteral("Open the matching video before using this older Mining History "
                            "item."));
    model.jumpToHistory(QStringLiteral("remote"));
    QCOMPARE(model.last_mining_notice(),
             QStringLiteral("This remote video cannot be reopened from history."));
    QCOMPARE(video_requests.size(), 0);
    QCOMPARE(subtitle_requests.size(), 0);
}

QTEST_GUILESS_MAIN(PlayerStudyViewModelTest)
#include "tst_player_study_view_model.moc"

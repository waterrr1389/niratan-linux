// SPDX-License-Identifier: GPL-3.0-or-later
//
// Flow 2: open a local video from the library, resume it, mine a subtitle,
// look a word up and add the Anki card, then close and refresh the library.
//
// The test drives the same view models and services the player page binds,
// without loading mpv or QML, with a loopback AnkiConnect stub and a headless
// QWebEnginePage for the popup bridge:
//
// - Features/Video/VideoLibraryView.swift (open, the history notification) and
//   Features/Video/VideoLibraryViewModel.swift (openURL,
//   openFromBeginningURL, subtitleURLForOpening, refreshPlaybackHistory): B1's
//   row resolution and B5's library refresh after the player closes.
// - Features/Video/VideoPlaybackHistoryStore.swift (VideoPlaybackState,
//   VideoPlaybackResumeOptions, prepareOpen, clearProgress): B2's restored
//   position, resume options and resumability bounds.
// - Features/Video/VideoPlayerScreen+Mining.swift (mineCurrentSubtitle),
//   Features/Video/VideoMiningHistoryStore.swift (record) and
//   Features/Video/Playback/PlaybackEngine.swift (VideoPlaybackSource.title):
//   B3's study-sidebar record, its filename stem (Decision D8 of video-gap)
//   and the current cue.
//
// - Features/Video/VideoPlayerScreen+Opening.swift (openVideo),
//   Features/Video/VideoPlayerScreen+Subtitles.swift
//   (autoloadSubtitleIfAvailable, applySubtitlesOff) and
//   Features/Video/Playback/PlaybackEngine.swift
//   (VideoPlaybackSource.title, audioExportSource(selectedAudioTrackID:)):
//   the sidecar document and the video title the page supplies.
// - Features/Video/Subtitles/SubtitleParser.swift and SubtitleCueStore.swift:
//   the cues at a playback time; InteractiveSubtitleTextView.swift
//   (performLookup): the UTF-16 offset of a click;
//   Core/SelectionLookup/TextSelectionResolver.swift (lookupCandidate,
//   lookupRange): the candidate and the highlight.
// - Features/Popup/PopupView.swift (mineEntry, buildContent),
//   Features/Popup/popup.js (mineEntry, miningContent),
//   Features/Video/VideoPlayerScreen.swift (popupView and its
//   miningContextProvider) and Features/Video/VideoMiningCoordinator.swift
//   (context, the direct-media branch): the video context set before the mine
//   and the fields and media the card carries.
// - Core/AnkiManager.swift (duplicateLookup, getMediaDirPath,
//   addNoteAnkiConnect, needsVideoScreenshot, needsVideoAudioClip) and
//   Models/Anki.swift (VideoMiningContext.value(for:)): the request sequence,
//   the duplicate preflight and the stored values.
//
// The player page's wiring is mirrored where the views are not loaded:
// VideoLookupLayer.updateVideoMiningContext builds the context map from
// miningSelection and the loaded video, sets it on MiningViewModel, and
// VideoLookupSurface.prepareMining repeats it before mineEntry and
// duplicateCheck. The overlay's lookup click maps to lookupAt(cueId,
// utf16Offset); like Swift's lookup.activeCue, the view model only knows the
// cue after that lookup, so the F1 selection is read after the F2 click.
//
// The media fixture is generated into the temporary data root: a short PCM
// WAV named Show 01.mkv (never decoded; the frame comes from the test and the
// clip from a fake ffmpeg) and its Show 01.srt sidecar. The dictionary is the
// yomitan fixture with one 現在 entry. No media is committed.

#include "viewmodels/anki/mining_view_model.h"
#include "viewmodels/dictionary/dictionary_popup_model.h"
#include "viewmodels/player/player_lookup_view_model.h"
#include "viewmodels/player/player_study_view_model.h"
#include "viewmodels/video/video_library_view_model.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "anki_stub.h"
#include "data_root.h"
#include "yomitan_fixture.h"

#include "services/anki/anki_config.h"
#include "services/dictionary/dictionary_store.h"
#include "services/player/mining_history_store.h"
#include "services/player/playback_history.h"
#include "services/player/resume_options.h"
#include "services/player/transcript_extractor.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

using niratan::anki::AnkiConfigStore;
using niratan::dictionary::DictionaryStore;
using niratan::player::MiningHistoryStore;
using niratan::player::PlaybackHistory;
using niratan::player::ResumeOptions;
using niratan::profiles::ProfileRepository;
using niratan::test::added_id;
using niratan::test::AnkiStub;
using niratan::test::minimal_index;
using niratan::test::write_stored_zip;
using niratan::viewmodels::DictionaryPopupModel;
using niratan::viewmodels::MiningViewModel;
using niratan::viewmodels::PlayerLookupViewModel;
using niratan::viewmodels::PlayerStudyViewModel;
using niratan::viewmodels::VideoLibraryViewModel;

namespace {

const QString kVideoName = QStringLiteral("Show 01");
const QByteArray kCuePrevious = QString::fromUtf8("前。").toUtf8();
const QByteArray kCueCurrent = QString::fromUtf8("現在。").toUtf8();
const QByteArray kCueNext = QString::fromUtf8("次。").toUtf8();
const QByteArray kCueAfterNext = QString::fromUtf8("次の次。").toUtf8();
const QByteArray kFrame = QByteArray("frame-bytes");
const QString kDictionaryTitle = QStringLiteral("Probe");
const QString added_id_text = QStringLiteral("9007199254740993");
const QString pendingMessage = QString::fromUtf8("Preparing card…");
// The page reports the mpv duration and the selected audio stream's ffIndex.
constexpr double kDuration = 30;
constexpr int kAudioStreamIndex = 3;

bool write_file(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(bytes) == bytes.size();
}

// A short PCM WAV (8 kHz, mono, 16 bit). The bytes are never decoded: the
// frame comes from the test and the clip from the fake ffmpeg.
QByteArray wav_bytes() {
    QByteArray format("fmt ", 4);
    format.append(QByteArray::fromHex("10000000"));
    format.append(QByteArray::fromHex("0100"));
    format.append(QByteArray::fromHex("0100"));
    format.append(QByteArray::fromHex("401f0000"));
    format.append(QByteArray::fromHex("803e0000"));
    format.append(QByteArray::fromHex("0200"));
    format.append(QByteArray::fromHex("1000"));
    QByteArray samples(16, '\0');
    QByteArray body("data", 4);
    body.append(QByteArray::fromHex("10000000"));
    body.append(samples);

    QByteArray wav("RIFF", 4);
    const quint32 riff_size = static_cast<quint32>(4 + format.size() + body.size());
    for (int shift = 0; shift < 32; shift += 8) {
        wav.append(static_cast<char>((riff_size >> shift) & 0xff));
    }
    wav.append("WAVE", 4);
    wav.append(format);
    wav.append(body);
    return wav;
}

QByteArray srt_bytes() {
    return QStringLiteral("1\n"
                          "00:00:01,000 --> 00:00:02,000\n"
                          "前。\n"
                          "\n"
                          "2\n"
                          "00:00:03,000 --> 00:00:04,000\n"
                          "現在。\n"
                          "\n"
                          "3\n"
                          "00:00:05,000 --> 00:00:06,000\n"
                          "次。\n"
                          "\n"
                          "4\n"
                          "00:00:07,000 --> 00:00:08,000\n"
                          "次の次。\n")
        .toUtf8();
}

// A library source holding one clip and its bound sidecar, as adding a folder
// to the Video section does.
struct LibraryFixture {
    QString source;
    QString video;
    QString subtitle;
};

LibraryFixture make_library(const QString& folder) {
    LibraryFixture fixture;
    fixture.source =
        QString::fromStdString((niratan::storage::data_root() / folder.toStdString()).string());
    QDir().mkpath(fixture.source);
    fixture.video = QDir(fixture.source).filePath(QStringLiteral("Show 01.mkv"));
    fixture.subtitle = QDir(fixture.source).filePath(QStringLiteral("Show 01.srt"));
    write_file(fixture.video, wav_bytes());
    write_file(fixture.subtitle, srt_bytes());
    return fixture;
}

// The row map for id across the sections, as the page reads it.
QVariantMap library_row(VideoLibraryViewModel& library, const QString& id) {
    for (const QVariant& section : library.sections()) {
        for (const QVariant& row : section.toMap().value(QStringLiteral("rows")).toList()) {
            if (row.toMap().value(QStringLiteral("id")).toString() == id) {
                return row.toMap();
            }
        }
    }
    return {};
}

QString fake_ffmpeg(const QString& directory) {
    const QString path = QDir(directory).filePath(QStringLiteral("ffmpeg"));
    write_file(path,
               "#!/bin/sh\n"
               "for last; do :; done\n"
               "printf '%s\\n' \"$@\" > \"$ARGS_FILE\"\n"
               "printf '%s' \"$FAKE_OUTPUT\" > \"$last\"\n"
               "exit ${FAKE_EXIT:-0}\n");
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

QString value_after(const QStringList& arguments, const QString& flag) {
    const auto index = arguments.indexOf(flag);
    if (index < 0 || index + 1 >= arguments.size()) {
        return {};
    }
    return arguments.at(index + 1);
}

QStringList recorded_arguments(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    if (!lines.isEmpty() && lines.last().isEmpty()) {
        lines.removeLast();
    }
    return lines;
}

QVariantMap mining_payload(const QString& status, const QString& message,
                           const QString& note_id = {}) {
    QVariantMap payload{{"status", status}, {"message", message}};
    if (!note_id.isEmpty()) {
        payload.insert("noteID", note_id);
    }
    return payload;
}

QString evaluate(QWebEnginePage& page, const QString& script) {
    QString result;
    bool finished = false;
    QEventLoop loop;
    page.runJavaScript(script, [&](const QVariant& value) {
        result = value.toString();
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    return finished ? result : QString();
}

QString sha1_hex(const QString& text) {
    return QString::fromLatin1(
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha1).toHex());
}

}  // namespace

// The bridge exposes the same methods VideoLookupSurface.qml's dictionaryBridge
// does, so popup.js reaches the view model unchanged.
class MiningBridgeProbe : public QObject {
    Q_OBJECT

   public slots:
    void mineEntry(int id, const QVariant& data) { emit mineReceived(id, data); }
    void duplicateCheck(int id, const QVariant& word) { emit duplicateReceived(id, word); }
    void openAnkiNote(const QVariant& ids) { emit openReceived(ids); }
    void miningFeedback(const QVariant& data) { emit feedbackReceived(data); }

   signals:
    void mineReceived(int id, const QVariant& data);
    void duplicateReceived(int id, const QVariant& word);
    void openReceived(const QVariant& ids);
    void feedbackReceived(const QVariant& data);
    void replyReady(int id, const QVariant& payload);
};

class FlowVideoMiningTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void library_resolves_the_row_clip_and_subtitle();
    void resume_restores_the_saved_playback_state();
    void study_sidebar_records_the_mined_subtitle();
    void closing_saves_and_the_library_refreshes();
    void open_lookup_and_add_the_card();

   private:
    void prepareVideoMiningContext();

    AnkiStub anki_;
    MiningViewModel mining_;
    MiningBridgeProbe probe_;
    std::unique_ptr<PlayerLookupViewModel> lookup_;
    QString video_path_;
    QString media_directory_;
    int screenshot_requests_ = 0;
};

// B1: VideoLibraryView.open / VideoLibraryViewModel.openURL,
// openFromBeginningURL and subtitleURLForOpening.
void FlowVideoMiningTest::library_resolves_the_row_clip_and_subtitle() {
    const LibraryFixture fixture = make_library(QStringLiteral("Library One"));
    QVERIFY(!fixture.video.isEmpty());

    VideoLibraryViewModel library;
    library.setDisplayMode(QStringLiteral("all"));
    library.addFolders({fixture.source});
    QTRY_VERIFY(!library.sections().isEmpty());
    QCOMPARE(library.openUrl(fixture.video), fixture.video);

    // A bound sidecar opens only while the file exists.
    library.bindSubtitle(fixture.video, fixture.subtitle);
    QCOMPARE(library.subtitleUrlForOpening(fixture.video), fixture.subtitle);
    QVERIFY(QFile::remove(fixture.subtitle));
    QCOMPARE(library.subtitleUrlForOpening(fixture.video), QString());

    // From the beginning opens the same clip and drops the stored position.
    PlaybackHistory history(niratan::storage::data_root() / "video_playback_history.json");
    QVERIFY(history.save(fixture.video, 20, 100));
    QCOMPARE(library.openFromBeginningUrl(fixture.video), fixture.video);
    QVERIFY(!PlaybackHistory(niratan::storage::data_root() / "video_playback_history.json")
                 .state(fixture.video)
                 .has_value());

    // A vanished clip rescans its source and stays unopened.
    QVERIFY(QFile::remove(fixture.video));
    QCOMPARE(library.openUrl(fixture.video), QString());
    QTRY_VERIFY(library_row(library, fixture.video).isEmpty());
    QCOMPARE(library.errorMessage(),
             QStringLiteral("The saved video file is no longer available."));
}

// B2: VideoPlaybackState.isResumable and VideoPlaybackHistoryStore.prepareOpen.
void FlowVideoMiningTest::resume_restores_the_saved_playback_state() {
    PlaybackHistory history(niratan::storage::data_root() / "resume_flow.json");
    const QString key = QStringLiteral("/videos/Show 01.mkv");

    ResumeOptions options;
    options.speed = 1.5;
    options.subtitle_delay = -0.25;
    QVERIFY(history.save(key, 20, 100, options));
    const auto state = history.state(key);
    QVERIFY(state.has_value());
    QVERIFY(state->resumable());
    QCOMPARE(state->progress(), std::optional(0.2));

    const auto restored = history.prepare_open(key, true, false);
    QCOMPARE(restored.position, std::optional(20.0));
    QCOMPARE(restored.options.speed, std::optional(1.5));
    QCOMPARE(restored.options.subtitle_delay, std::optional(-0.25));

    // An explicit start restores at 0 and removes the entry.
    const auto fresh = history.prepare_open(key, true, true);
    QCOMPARE(fresh.position, std::optional(0.0));
    QVERIFY(!history.state(key).has_value());

    // Under two seconds in is not stored at all (updatePlaybackState removes
    // the entry).
    QVERIFY(!history.save(QStringLiteral("/videos/a.mkv"), 1.5, 100));
    QVERIFY(!history.state(QStringLiteral("/videos/a.mkv")).has_value());

    // At 98 % of a long clip the state is stored but not resumable.
    QVERIFY(history.save(QStringLiteral("/videos/b.mkv"), 980, 1000));
    const auto near_end = history.state(QStringLiteral("/videos/b.mkv"));
    QVERIFY(near_end.has_value());
    QCOMPARE(near_end->progress(), std::optional(0.98));
    QVERIFY(!near_end->resumable());
}

// B3: VideoPlayerScreen+Mining.mineCurrentSubtitle and
// VideoMiningHistoryStore.record (videoFileName is the filename stem, D8).
void FlowVideoMiningTest::study_sidebar_records_the_mined_subtitle() {
    const std::filesystem::path data_root = niratan::storage::data_root();
    QVERIFY(!data_root.empty());
    const QString video = QString::fromStdString((data_root / "Show 01.mkv").string());
    const QString subtitle = QString::fromStdString((data_root / "Show 01.srt").string());
    QVERIFY(QFileInfo::exists(video));
    QVERIFY(QFileInfo::exists(subtitle));

    MiningHistoryStore::at_data_root().clear();

    PlayerStudyViewModel study;
    study.setVideo(QUrl::fromLocalFile(video));
    const std::optional<QByteArray> document = niratan::player::read_subtitle_document(subtitle);
    QVERIFY(document.has_value());
    study.setSubtitleDocument(QUrl::fromLocalFile(subtitle), *document, true);
    study.setPlaybackReady(true);
    study.updatePlayback(3.0, 0.0);
    QVERIFY(study.can_mine());
    study.mineCurrentSubtitle();

    const auto snapshot = MiningHistoryStore::at_data_root().load_snapshot();
    QCOMPARE(snapshot.items.size(), std::size_t(1));
    const auto& item = snapshot.items.front();
    QCOMPARE(QString::fromStdString(item.video_file_name), QStringLiteral("Show 01"));
    QCOMPARE(QString::fromStdString(item.video_title), QStringLiteral("Show 01"));
    QCOMPARE(QString::fromStdString(item.subtitle_text), QString::fromUtf8(kCueCurrent));
    QCOMPARE(item.cue_start, 3.0);
    QCOMPARE(item.cue_end, 4.0);
    QCOMPARE(QString::fromStdString(item.video_path.value_or("")), video);
}

// B5: VideoPlayerViewModel.saveCurrentPosition, the history change and the
// library refresh (VideoLibraryView's didChange notification).
void FlowVideoMiningTest::closing_saves_and_the_library_refreshes() {
    const LibraryFixture fixture = make_library(QStringLiteral("Library Two"));
    VideoLibraryViewModel library;
    library.setDisplayMode(QStringLiteral("all"));
    library.addFolders({fixture.source});
    QTRY_VERIFY(!library.sections().isEmpty());

    PlaybackHistory history(niratan::storage::data_root() / "video_playback_history.json");
    QVERIFY(history.save(fixture.video, 20, 100));

    QSignalSpy revisions(&library, &VideoLibraryViewModel::playbackHistoryRevisionChanged);
    library.refreshPlaybackHistory();
    QCOMPARE(revisions.size(), 1);

    const QVariantMap row = library_row(library, fixture.video);
    QVERIFY(!row.isEmpty());
    const QVariantMap playback = row.value(QStringLiteral("playback")).toMap();
    QCOMPARE(playback.value(QStringLiteral("position")).toDouble(), 20.0);
    QCOMPARE(playback.value(QStringLiteral("resumable")).toBool(), true);
    QCOMPARE(playback.value(QStringLiteral("progress")).toDouble(), 0.2);
    QCOMPARE(library.openUrl(fixture.video), fixture.video);

    library.markWatched(fixture.video);
    const QVariantMap watched =
        library_row(library, fixture.video).value(QStringLiteral("playback")).toMap();
    QCOMPARE(watched.value(QStringLiteral("finished")).toBool(), true);
    QCOMPARE(watched.value(QStringLiteral("resumable")).toBool(), false);

    library.clearProgress(fixture.video);
    QVERIFY(
        library_row(library, fixture.video).value(QStringLiteral("playback")).toMap().isEmpty());
}

void FlowVideoMiningTest::initTestCase() {
    const std::filesystem::path data_root = niratan::storage::data_root();
    QVERIFY(!data_root.empty());

    const auto zip = data_root / "video_dictionary.zip";
    try {
        write_stored_zip(zip,
                         {{"index.json", minimal_index(kDictionaryTitle)},
                          {"term_bank_1.json",
                           QByteArrayLiteral(R"([["現在","げんざい","","",0,["now"],1,""]])")}});
    } catch (const std::exception&) {
        QFAIL("cannot write the dictionary fixture");
    }
    QVERIFY(DictionaryStore(niratan::storage::dictionaries_root()).import_zip(zip).ok);

    const QString video = QString::fromStdString((data_root / "Show 01.mkv").string());
    const QString subtitle = QString::fromStdString((data_root / "Show 01.srt").string());
    QVERIFY(write_file(video, wav_bytes()));
    QVERIFY(write_file(subtitle, srt_bytes()));

    qputenv("FAKE_OUTPUT", "clip-bytes");
    qputenv("FAKE_EXIT", "0");
}

// The same map VideoLookupLayer.qml's updateVideoMiningContext builds, with
// the title of VideoPlaybackSource.title and the selected audio stream.
void FlowVideoMiningTest::prepareVideoMiningContext() {
    const QVariantMap selection = lookup_->miningSelection();
    QVariantMap context{
        {"path", video_path_},
        {"title", kVideoName},
        {"audioStreamIndex", kAudioStreamIndex},
        {"cueText", selection.value("cueText")},
        {"cueStart", selection.value("cueStart")},
        {"cueEnd", selection.value("cueEnd")},
        {"subtitleDelay", 0.0},
        {"duration", kDuration},
    };
    if (selection.contains("previousCueText")) {
        context.insert("previousCueText", selection.value("previousCueText"));
    }
    if (selection.contains("nextCueText")) {
        context.insert("nextCueText", selection.value("nextCueText"));
    }
    mining_.setVideoContext(context);
}

// F1-F7: the sidecar cues, the lookup and highlight, the context and card
// fields, the captured frame and clip, the reply and the duplicate preflight.
void FlowVideoMiningTest::open_lookup_and_add_the_card() {
    const std::filesystem::path data_root = niratan::storage::data_root();
    QVERIFY(!data_root.empty());

    // Anki settings: point the stored transport at the loopback stub and map
    // every field the video context and the entry content produce.
    ProfileRepository profiles{data_root};
    {
        AnkiConfigStore store{data_root, profiles};
        auto& settings = store.settings();
        settings.transport->url = anki_.url();
        settings.selected_deck = std::string("Mining");
        settings.selected_note_type = std::string("Lapis");
        settings.available_note_types = {{"Lapis",
                                          {"Word", "Sentence", "File", "Title", "Sub", "Start",
                                           "End", "Prev", "Next", "Frame", "Clip"}}};
        settings.field_mappings = {
            {"Word", "{expression}"},          {"Sentence", "{sentence}"},
            {"File", "{video-file-name}"},     {"Title", "{document-title}"},
            {"Sub", "{video-subtitle}"},       {"Start", "{video-cue-start}"},
            {"End", "{video-cue-end}"},        {"Prev", "{video-previous-subtitle}"},
            {"Next", "{video-next-subtitle}"}, {"Frame", "{video-screenshot}"},
            {"Clip", "{video-audio-clip}"}};
        QVERIFY(store.save().ok);
    }

    const LibraryFixture fixture = make_library(QStringLiteral("Library Lookup"));
    VideoLibraryViewModel library;
    library.setDisplayMode(QStringLiteral("all"));
    library.addFolders({fixture.source});
    QTRY_VERIFY(!library.sections().isEmpty());
    library.bindSubtitle(fixture.video, fixture.subtitle);
    video_path_ = library.openUrl(fixture.video);
    const QString subtitle_path = library.subtitleUrlForOpening(fixture.video);
    QVERIFY(!video_path_.isEmpty());
    QVERIFY(!subtitle_path.isEmpty());

    // VideoPlayerScreen+Opening openVideo / autoloadSubtitleIfAvailable: the
    // sidecar bytes become the lookup document, and playback at 3.5 s shows
    // cue b.
    const std::optional<QByteArray> subtitle =
        niratan::player::read_subtitle_document(subtitle_path);
    QVERIFY(subtitle.has_value());
    lookup_ = std::make_unique<PlayerLookupViewModel>();
    lookup_->setLookupSettings(false, 45);
    lookup_->setPlaying(true);
    lookup_->setSubtitleDocument(QUrl::fromLocalFile(subtitle_path), *subtitle, true);
    lookup_->updatePlayback(3.5, 0.0);

    // F1: the current cue is b (3-4 s, "現在。").
    QCOMPARE(lookup_->current_cues().size(), 1);
    const QVariantMap current = lookup_->current_cues().first().toMap();
    QCOMPARE(current.value("id").toString(), QStringLiteral("2"));
    QCOMPARE(current.value("start").toDouble(), 3.0);
    QCOMPARE(current.value("end").toDouble(), 4.0);
    QCOMPARE(current.value("text").toString(), QString::fromUtf8(kCueCurrent));

    // F2: a click at UTF-16 offset 0 of cue b looks up 現在.
    const QVariantMap result = lookup_->lookupAt(QStringLiteral("2"), 0);
    QVERIFY(!result.isEmpty());
    QCOMPARE(result.value("text").toString(), QString::fromUtf8(kCueCurrent));
    QCOMPARE(result.value("utf16Start").toInt(), 0);
    QCOMPARE(result.value("matchedText").toString(), QStringLiteral("現在"));
    QCOMPARE(result.value("highlightStart").toInt(), 0);
    QCOMPARE(result.value("highlightLength").toInt(), 2);
    QVERIFY(lookup_->has_popup());

    DictionaryPopupModel* popup = qobject_cast<DictionaryPopupModel*>(lookup_->popup_model());
    QVERIFY(popup != nullptr);
    QVERIFY(popup->error().isEmpty());
    QCOMPARE(popup->matched_text(), QStringLiteral("現在"));

    // F1: the mining selection the mine will carry. The view model, like
    // Swift's lookup.activeCue, learns the cue from the lookup above.
    const QVariantMap selection = lookup_->miningSelection();
    QCOMPARE(selection.value("cueText").toString(), QString::fromUtf8(kCueCurrent));
    QCOMPARE(selection.value("sentence").toString(), QString::fromUtf8(kCueCurrent));
    QCOMPARE(selection.value("cueStart").toDouble(), 3.0);
    QCOMPARE(selection.value("cueEnd").toDouble(), 4.0);
    QCOMPARE(selection.value("previousCueText").toString(), QString::fromUtf8(kCuePrevious));
    QCOMPARE(selection.value("nextCueText").toString(), QString::fromUtf8(kCueNext));
    QCOMPARE(selection.size(), 6);

    // The fake ffmpeg and the frame answer, as VideoLookupLayer wires them.
    const QString args_file = QString::fromStdString((data_root / "args.txt").string());
    qputenv("ARGS_FILE", args_file.toUtf8());
    mining_.set_ffmpeg_path(fake_ffmpeg(QString::fromStdString(data_root.string())));

    QTemporaryDir collection;
    QVERIFY(collection.isValid());
    media_directory_ = collection.filePath(QStringLiteral("collection.media"));
    QVERIFY(QDir().mkpath(media_directory_));
    anki_.responses["getMediaDirPath"] = {{"result", media_directory_}};

    connect(&mining_, &MiningViewModel::screenshotRequested, this, [this](const QString& path) {
        ++screenshot_requests_;
        const bool ok = write_file(path, kFrame);
        mining_.screenshotFinished(ok);
    });

    // The popup document and bridge, as VideoLookupSurface.qml loads them.
    QWebEnginePage page;
    QWebChannel channel(&page);
    channel.registerObject(QStringLiteral("dictionaryBridge"), &probe_);
    page.setWebChannel(&channel);
    // VideoLookupSurface.prepareMining runs before each mine and duplicate
    // check, so the video context is on the model when Mine is pressed.
    connect(&probe_, &MiningBridgeProbe::mineReceived, this, [this](int id, const QVariant& data) {
        prepareVideoMiningContext();
        mining_.mineEntry(id, data);
    });
    connect(&probe_, &MiningBridgeProbe::duplicateReceived, this,
            [this](int id, const QVariant& word) {
                prepareVideoMiningContext();
                mining_.duplicateCheck(id, word);
            });
    connect(&probe_, &MiningBridgeProbe::openReceived, this,
            [this](const QVariant& ids) { mining_.openAnkiNote(ids); });
    connect(&probe_, &MiningBridgeProbe::feedbackReceived, this,
            [this](const QVariant& data) { mining_.miningFeedback(data); });
    connect(&mining_, &MiningViewModel::replyReady, &probe_, &MiningBridgeProbe::replyReady);

    QList<QVariantMap> replies;
    connect(&probe_, &MiningBridgeProbe::replyReady, this,
            [&replies](int, const QVariant& payload) {
                const auto map = payload.toMap();
                if (map.contains(QStringLiteral("status"))) {
                    replies.append(map);
                }
            });
    QList<QStringList> toasts;
    connect(&mining_, &MiningViewModel::toastChanged, this,
            [this, &toasts] { toasts.append({mining_.toast_status(), mining_.toast_message()}); });
    QSignalSpy duplicates(&probe_, &MiningBridgeProbe::duplicateReceived);

    QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
    page.setHtml(popup->html());
    QVERIFY(loaded.wait(15000));
    QVERIFY(loaded.last().at(0).toBool());
    const auto options = QString::fromUtf8(
        QJsonDocument::fromVariant(mining_.popupMiningOptions()).toJson(QJsonDocument::Compact));
    evaluate(page,
             QStringLiteral("window.hoshiMiningRequestBase = 0; window.hoshiMiningOptions = ") +
                 options + QStringLiteral(";\n") + popup->bridge_script());

    QString handler;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
        handler =
            evaluate(page, QStringLiteral("typeof window.webkit?.messageHandlers?.mineEntry"));
        if (handler == QStringLiteral("object")) {
            break;
        }
        QTest::qWait(50);
    }
    QCOMPARE(handler, QStringLiteral("object"));

    // F2: the generated page shows the entry that produced the highlight.
    QCOMPARE(evaluate(page, QStringLiteral("String(window.lookupEntries?.length)")),
             QStringLiteral("1"));
    QCOMPARE(evaluate(page, QStringLiteral("window.lookupEntries[0].expression")),
             QStringLiteral("現在"));
    QCOMPARE(evaluate(page, QStringLiteral("window.lookupEntries[0].matched")),
             QStringLiteral("現在"));
    QCOMPARE(
        evaluate(page,
                 QStringLiteral("document.querySelectorAll('#entries-container .entry').length")),
        QStringLiteral("1"));

    // The popup checks the rendered entry for duplicates after its handshake.
    QTRY_COMPARE_WITH_TIMEOUT(duplicates.size(), 1, 5000);
    QCOMPARE(duplicates.first().at(1).toString(), QStringLiteral("現在"));

    // F3-F6: mine through the page, as the Mine button does.
    anki_.requests.clear();
    QCOMPARE(evaluate(page, QStringLiteral("mineEntryAtIndex(0); 'sent';")),
             QStringLiteral("sent"));
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 15000);

    // F6: the reply reaches the page and the toast.
    QCOMPARE(
        replies.first(),
        mining_payload(QStringLiteral("added"), QStringLiteral("Added to Anki."), added_id_text));
    QVERIFY(toasts.size() >= 2);
    QCOMPARE(toasts.at(0), (QStringList{QStringLiteral("pending"), pendingMessage}));
    QCOMPARE(toasts.at(1),
             (QStringList{QStringLiteral("added"), QStringLiteral("Added to Anki.")}));
    QTRY_COMPARE_WITH_TIMEOUT(mining_.toast_status(), QStringLiteral("added"), 5000);
    QCOMPARE(mining_.toast_message(), QStringLiteral("Added to Anki."));
    QCOMPARE(mining_.toast_note_id(), added_id_text);

    // F3: the note and the video context fields.
    QCOMPARE(anki_.actions().count(QStringLiteral("addNote")), qsizetype(1));
    QCOMPARE(anki_.actions().count(QStringLiteral("getMediaDirPath")), qsizetype(1));
    QVERIFY(!anki_.actions().contains(QStringLiteral("storeMediaFile")));
    const auto note =
        anki_.params(QStringLiteral("addNote")).value(QStringLiteral("note")).toObject();
    QCOMPARE(note.value("deckName").toString(), QStringLiteral("Mining"));
    QCOMPARE(note.value("modelName").toString(), QStringLiteral("Lapis"));
    const auto fields = note.value("fields").toObject();
    QCOMPARE(fields.value("Word").toString(), QStringLiteral("現在"));
    // buildContent's {sentence} bolds every matched occurrence, as Swift's
    // replacingOccurrences does; {video-subtitle} keeps the plain cue text.
    QCOMPARE(fields.value("Sentence").toString(), QStringLiteral("<b>現在</b>。"));
    QCOMPARE(fields.value("File").toString(), QStringLiteral("Show 01"));
    QCOMPARE(fields.value("Title").toString(), QStringLiteral("Show 01"));
    QCOMPARE(fields.value("Sub").toString(), QString::fromUtf8(kCueCurrent));
    QCOMPARE(fields.value("Start").toString(), QStringLiteral("0:00:03.000"));
    QCOMPARE(fields.value("End").toString(), QStringLiteral("0:00:04.000"));
    QCOMPARE(fields.value("Prev").toString(), QString::fromUtf8(kCuePrevious));
    QCOMPARE(fields.value("Next").toString(), QString::fromUtf8(kCueNext));

    // F4: the frame is a compressed JPEG already in collection.media.
    const QString sha1 = sha1_hex(video_path_);
    const QString frame_name =
        QStringLiteral("hoshi_video_frame_") + sha1 + QStringLiteral("_3000-4000.jpg");
    QCOMPARE(fields.value("Frame").toString(), QStringLiteral("<img src=\"%1\">").arg(frame_name));
    const QString frame_path = QDir(media_directory_).filePath(frame_name);
    QVERIFY(QFileInfo::exists(frame_path));
    QFile frame(frame_path);
    QVERIFY(frame.open(QIODevice::ReadOnly));
    QCOMPARE(frame.readAll(), kFrame);
    QCOMPARE(screenshot_requests_, 1);

    // F5: the fake ffmpeg received the padded range and the selected stream.
    const QString audio_name =
        QStringLiteral("hoshi_video_audio_") + sha1 + QStringLiteral("_2880-4120.m4a");
    QCOMPARE(fields.value("Clip").toString(), QStringLiteral("[sound:%1]").arg(audio_name));
    QVERIFY(QFileInfo::exists(QDir(media_directory_).filePath(audio_name)));
    const QStringList arguments = recorded_arguments(args_file);
    QCOMPARE(value_after(arguments, QStringLiteral("-ss")), QStringLiteral("2.880000"));
    QCOMPARE(value_after(arguments, QStringLiteral("-t")), QStringLiteral("1.240000"));
    QCOMPARE(value_after(arguments, QStringLiteral("-map")), QStringLiteral("0:3"));
    QCOMPARE(value_after(arguments, QStringLiteral("-ac")), QStringLiteral("1"));
    QCOMPARE(value_after(arguments, QStringLiteral("-c:a")), QStringLiteral("aac"));
    QCOMPARE(value_after(arguments, QStringLiteral("-b:a")), QStringLiteral("64k"));
    QVERIFY(arguments.last().endsWith(QStringLiteral(".m4a")));

    // F7: Anki now holds the card; the next mine is the duplicate preflight,
    // before any capture.
    QTRY_COMPARE_WITH_TIMEOUT(duplicates.size(), 2, 5000);
    anki_.responses["canAddNotesWithErrorDetail"] = {
        {"result", QJsonArray{QJsonObject{{"canAdd", false}}}}};
    anki_.requests.clear();
    replies.clear();
    screenshot_requests_ = 0;
    QVERIFY(QFile::remove(args_file));

    QCOMPARE(evaluate(page, QStringLiteral("mineEntryAtIndex(0); 'sent';")),
             QStringLiteral("sent"));
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 10000);
    QCOMPARE(replies.first(),
             mining_payload(QStringLiteral("duplicate"), QStringLiteral("Already exists in Anki."),
                            added_id_text));
    QTRY_COMPARE_WITH_TIMEOUT(mining_.toast_status(), QStringLiteral("duplicate"), 5000);
    QCOMPARE(mining_.toast_note_id(), added_id_text);
    QCOMPARE(anki_.actions().count(QStringLiteral("getMediaDirPath")), qsizetype(0));
    QCOMPARE(anki_.actions().count(QStringLiteral("addNote")), qsizetype(0));
    QCOMPARE(screenshot_requests_, 0);
    QVERIFY(!QFileInfo::exists(args_file));
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    niratan::test::TempDataRoot data_root;
    if (!data_root.isValid()) {
        return 1;
    }
    QGuiApplication app(argc, argv);
    FlowVideoMiningTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_flow_video_mining.moc"

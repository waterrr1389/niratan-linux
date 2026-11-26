// SPDX-License-Identifier: GPL-3.0-or-later
//
// B1-B9 of notes/specs/viewmodels-player-inspector.md: the Inspector's episode
// list (VideoPlaylist.swift), track rows (VideoInspectorView.trackSection),
// subtitle timing (VideoSubtitleTiming), audio timing and playback speed
// (VideoPlaybackSpeed), through PlayerInspectorViewModel. Episode navigation
// follows VideoPlaylist.previousURL/nextURL and VideoPlayerViewModel's
// playPrevious/playNext/openPlaylistItem.

#include "services/video/video_playlist.h"
#include "viewmodels/player/player_inspector_view_model.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest>

#include <memory>

using niratan::video::VideoPlaylist;
using niratan::viewmodels::PlayerInspectorViewModel;

namespace {

bool write_file(const QString& directory, const QString& name) {
    QFile file(QDir(directory).filePath(name));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.close();
    return true;
}

QUrl local(const QString& directory, const QString& name) {
    return QUrl::fromLocalFile(QDir(directory).filePath(name));
}

QStringList file_names(const QList<QUrl>& urls) {
    QStringList names;
    for (const QUrl& url : urls) {
        names << QFileInfo(url.toLocalFile()).fileName();
    }
    return names;
}

QVariantMap row_at(const QVariantList& rows, int index) {
    return rows.value(index).toMap();
}

QVariantMap track(const QString& type, int id, const QString& title, const QString& language,
                  const QString& codec, bool selected, const QString& external = QString()) {
    return QVariantMap{
        {QStringLiteral("type"), type},
        {QStringLiteral("id"), id},
        {QStringLiteral("title"), title},
        {QStringLiteral("language"), language},
        {QStringLiteral("codec"), codec},
        {QStringLiteral("isSelected"), selected},
        {QStringLiteral("externalFilename"), external},
    };
}

QUrl url_from_signal(const QSignalSpy& spy, int index = 0) {
    return spy.at(index).at(0).toUrl();
}

double double_from_signal(const QSignalSpy& spy, int index = 0) {
    return spy.at(index).at(0).toDouble();
}

std::unique_ptr<QTemporaryDir> three_episode_folder() {
    auto directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) {
        return nullptr;
    }
    for (const char* name : {"Show 1.mkv", "Show 2.mkv", "Show 3.mkv"}) {
        if (!write_file(directory->path(), QString::fromLatin1(name))) {
            return nullptr;
        }
    }
    return directory;
}

}  // namespace

class PlayerInspectorViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void episodeListingFiltersSortsAndLocates();
    void headerCaptionAndDisplayTitle();
    void episodeRowsAndSelection();
    void adjacentEpisodeNavigation();
    void adjacentEpisodeNaturalOrder();
    void trackRows();
    void untitledTrackRows();
    void selectingTracksRequestsMpvIds();
    void subtitleTimingValueText();
    void subtitleTimingCommands();
    void audioTiming();
    void playbackSpeed();
};

void PlayerInspectorViewModelTest::episodeListingFiltersSortsAndLocates() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    for (const char* name : {"Show 10.mkv", "Show 2.mkv", "Show 1.mkv", "notes.txt", "Show 3.MKV",
                             ".Show 4.mkv", "apple.mkv", "Banana.mkv"}) {
        QVERIFY(write_file(directory.path(), QString::fromLatin1(name)));
    }

    const QList<QUrl> candidates = niratan::video::playlist_directory_urls(directory.path());
    QCOMPARE(candidates.size(), qsizetype(7));
    QVERIFY(!candidates.contains(local(directory.path(), QStringLiteral(".Show 4.mkv"))));

    const VideoPlaylist playlist = niratan::video::make_video_playlist(
        candidates, local(directory.path(), QStringLiteral("Show 2.mkv")));
    const QStringList expected_names{QStringLiteral("apple.mkv"),  QStringLiteral("Banana.mkv"),
                                     QStringLiteral("Show 1.mkv"), QStringLiteral("Show 2.mkv"),
                                     QStringLiteral("Show 3.MKV"), QStringLiteral("Show 10.mkv")};
    QCOMPARE(file_names(playlist.items), expected_names);
    QCOMPARE(playlist.currentIndex, 3);

    const VideoPlaylist absent = niratan::video::make_video_playlist(
        candidates, local(directory.path(), QStringLiteral("Elsewhere.mkv")));
    QCOMPARE(absent.items.size(), qsizetype(6));
    QCOMPARE(absent.currentIndex, -1);
}

void PlayerInspectorViewModelTest::headerCaptionAndDisplayTitle() {
    auto directory = three_episode_folder();
    QVERIFY(directory != nullptr);

    PlayerInspectorViewModel model;
    model.setVideo(local(directory->path(), QStringLiteral("Show 2.mkv")), QString());
    QCOMPARE(model.header_caption(), QStringLiteral("Now Playing · 2 / 3"));
    QCOMPARE(model.display_title(), QStringLiteral("Show 2"));

    model.setVideo(local(directory->path(), QStringLiteral("Show 2.mkv")),
                   QStringLiteral("Custom title"));
    QCOMPARE(model.display_title(), QStringLiteral("Custom title"));

    QTemporaryDir single;
    QVERIFY(single.isValid());
    QVERIFY(write_file(single.path(), QStringLiteral("Solo.mkv")));
    PlayerInspectorViewModel one;
    one.setVideo(local(single.path(), QStringLiteral("Solo.mkv")), QString());
    QCOMPARE(one.header_caption(), QStringLiteral("Now Playing"));

    PlayerInspectorViewModel empty;
    QCOMPARE(empty.display_title(), QStringLiteral("Inspector"));
    QCOMPARE(empty.header_caption(), QStringLiteral("Now Playing"));

    PlayerInspectorViewModel absent;
    absent.setVideo(local(directory->path(), QStringLiteral("Elsewhere.mkv")), QString());
    QCOMPARE(absent.header_caption(), QStringLiteral("Now Playing"));
}

void PlayerInspectorViewModelTest::episodeRowsAndSelection() {
    auto directory = three_episode_folder();
    QVERIFY(directory != nullptr);

    PlayerInspectorViewModel model;
    model.setVideo(local(directory->path(), QStringLiteral("Show 2.mkv")), QString());

    const QVariantList episodes = model.episodes();
    QCOMPARE(episodes.size(), qsizetype(3));
    QCOMPARE(row_at(episodes, 0).value(QStringLiteral("number")).toInt(), 1);
    QCOMPARE(row_at(episodes, 0).value(QStringLiteral("title")).toString(),
             QStringLiteral("Show 1"));
    QCOMPARE(row_at(episodes, 0).value(QStringLiteral("current")).toBool(), false);
    QCOMPARE(row_at(episodes, 1).value(QStringLiteral("current")).toBool(), true);
    QCOMPARE(row_at(episodes, 2).value(QStringLiteral("number")).toInt(), 3);
    QCOMPARE(row_at(episodes, 2).value(QStringLiteral("url")).toUrl(),
             local(directory->path(), QStringLiteral("Show 3.mkv")));

    QCOMPARE(model.current_episode().value(QStringLiteral("title")).toString(),
             QStringLiteral("Show 2"));

    QSignalSpy spy(&model, &PlayerInspectorViewModel::episodeRequested);
    model.selectEpisode(1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(url_from_signal(spy), local(directory->path(), QStringLiteral("Show 2.mkv")));

    QTemporaryDir empty;
    QVERIFY(empty.isValid());
    model.setVideo(local(empty.path(), QStringLiteral("Nothing.mkv")), QString());
    QVERIFY(model.episodes().isEmpty());
    QVERIFY(model.current_episode().isEmpty());
}

// B1, B2: playPreviousEpisode and playNextEpisode follow VideoPlaylist's
// previousURL and nextURL in the Inspector's order: one step, exact URL, no
// wrap, and nothing without a current episode or at either end.
void PlayerInspectorViewModelTest::adjacentEpisodeNavigation() {
    auto directory = three_episode_folder();
    QVERIFY(directory != nullptr);

    PlayerInspectorViewModel model;
    model.setVideo(local(directory->path(), QStringLiteral("Show 2.mkv")), QString());
    QVERIFY(model.can_play_previous_episode());
    QVERIFY(model.can_play_next_episode());

    QSignalSpy spy(&model, &PlayerInspectorViewModel::episodeRequested);
    QVERIFY(model.playPreviousEpisode());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(url_from_signal(spy), local(directory->path(), QStringLiteral("Show 1.mkv")));
    QVERIFY(model.playNextEpisode());
    QCOMPARE(spy.count(), 2);
    QCOMPARE(url_from_signal(spy, 1), local(directory->path(), QStringLiteral("Show 3.mkv")));

    model.setVideo(local(directory->path(), QStringLiteral("Show 1.mkv")), QString());
    QVERIFY(!model.can_play_previous_episode());
    QVERIFY(model.can_play_next_episode());
    spy.clear();
    QVERIFY(!model.playPreviousEpisode());
    QCOMPARE(spy.count(), 0);
    QVERIFY(model.playNextEpisode());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(url_from_signal(spy), local(directory->path(), QStringLiteral("Show 2.mkv")));

    model.setVideo(local(directory->path(), QStringLiteral("Show 3.mkv")), QString());
    QVERIFY(model.can_play_previous_episode());
    QVERIFY(!model.can_play_next_episode());
    spy.clear();
    QVERIFY(!model.playNextEpisode());
    QCOMPARE(spy.count(), 0);
    QVERIFY(model.playPreviousEpisode());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(url_from_signal(spy), local(directory->path(), QStringLiteral("Show 2.mkv")));

    QTemporaryDir single;
    QVERIFY(single.isValid());
    QVERIFY(write_file(single.path(), QStringLiteral("Solo.mkv")));
    model.setVideo(local(single.path(), QStringLiteral("Solo.mkv")), QString());
    QVERIFY(!model.can_play_previous_episode());
    QVERIFY(!model.can_play_next_episode());
    spy.clear();
    QVERIFY(!model.playPreviousEpisode());
    QVERIFY(!model.playNextEpisode());
    QCOMPARE(spy.count(), 0);

    model.setVideo(local(directory->path(), QStringLiteral("Elsewhere.mkv")), QString());
    QVERIFY(!model.can_play_previous_episode());
    QVERIFY(!model.can_play_next_episode());
    spy.clear();
    QVERIFY(!model.playPreviousEpisode());
    QVERIFY(!model.playNextEpisode());
    QCOMPARE(spy.count(), 0);

    PlayerInspectorViewModel idle;
    QVERIFY(!idle.can_play_previous_episode());
    QVERIFY(!idle.can_play_next_episode());
    QVERIFY(!idle.playPreviousEpisode());
    QVERIFY(!idle.playNextEpisode());
}

// B1: neighbors follow Foundation's natural order, not plain byte order.
void PlayerInspectorViewModelTest::adjacentEpisodeNaturalOrder() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    for (const char* name : {"Show 1.mkv", "Show 2.mkv", "Show 10.mkv"}) {
        QVERIFY(write_file(directory.path(), QString::fromLatin1(name)));
    }

    PlayerInspectorViewModel model;
    model.setVideo(local(directory.path(), QStringLiteral("Show 2.mkv")), QString());
    QSignalSpy spy(&model, &PlayerInspectorViewModel::episodeRequested);
    QVERIFY(model.playPreviousEpisode());
    QCOMPARE(url_from_signal(spy), local(directory.path(), QStringLiteral("Show 1.mkv")));
    QVERIFY(model.playNextEpisode());
    QCOMPARE(url_from_signal(spy, 1), local(directory.path(), QStringLiteral("Show 10.mkv")));
}

void PlayerInspectorViewModelTest::trackRows() {
    PlayerInspectorViewModel model;
    model.setTracks(QVariantList{
        track(QStringLiteral("video"), 1, QStringLiteral("Main"), QStringLiteral("ja"),
              QStringLiteral("h264"), true),
        track(QStringLiteral("audio"), 2, QStringLiteral("Japanese"), QStringLiteral("ja"),
              QStringLiteral("aac"), true),
        track(QStringLiteral("audio"), 3, QStringLiteral("Commentary"), QString(),
              QStringLiteral("aac"), false),
        track(QStringLiteral("sub"), 4, QStringLiteral("Full"), QStringLiteral("ja"),
              QStringLiteral("ass"), false),
        track(QStringLiteral("sub"), 5, QStringLiteral("External"), QStringLiteral("en"),
              QStringLiteral("ass"), true, QStringLiteral("/tmp/external.ass")),
    });

    const QVariantList video = model.video_tracks();
    QCOMPARE(video.size(), qsizetype(1));
    QCOMPARE(row_at(video, 0).value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(row_at(video, 0).value(QStringLiteral("title")).toString(),
             QStringLiteral("Main · ja"));
    QCOMPARE(row_at(video, 0).value(QStringLiteral("detail")).toString(), QStringLiteral("h264"));
    QCOMPARE(row_at(video, 0).value(QStringLiteral("selected")).toBool(), true);

    const QVariantList audio = model.audio_tracks();
    QCOMPARE(audio.size(), qsizetype(3));
    QCOMPARE(row_at(audio, 0).value(QStringLiteral("id")).toInt(), 0);
    QCOMPARE(row_at(audio, 0).value(QStringLiteral("title")).toString(), QStringLiteral("Off"));
    QCOMPARE(row_at(audio, 0).value(QStringLiteral("selected")).toBool(), false);
    QCOMPARE(row_at(audio, 1).value(QStringLiteral("title")).toString(),
             QStringLiteral("Japanese · ja"));
    QCOMPARE(row_at(audio, 2).value(QStringLiteral("title")).toString(),
             QStringLiteral("Commentary"));
    QCOMPARE(row_at(audio, 2).value(QStringLiteral("detail")).toString(), QStringLiteral("aac"));

    const QVariantList subtitles = model.subtitle_tracks();
    QCOMPARE(subtitles.size(), qsizetype(3));
    QCOMPARE(row_at(subtitles, 0).value(QStringLiteral("selected")).toBool(), false);
    QCOMPARE(row_at(subtitles, 1).value(QStringLiteral("detail")).toString(),
             QStringLiteral("ass"));
    QCOMPARE(row_at(subtitles, 2).value(QStringLiteral("detail")).toString(),
             QStringLiteral("External Subtitles · ass"));
    QCOMPARE(row_at(subtitles, 2).value(QStringLiteral("selected")).toBool(), true);

    model.setTracks(QVariantList{
        track(QStringLiteral("video"), 1, QStringLiteral("Main"), QString(), QStringLiteral("h264"),
              true),
    });
    QCOMPARE(model.audio_tracks().size(), qsizetype(1));
    QCOMPARE(row_at(model.audio_tracks(), 0).value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(model.subtitle_tracks().size(), qsizetype(1));
    QCOMPARE(row_at(model.subtitle_tracks(), 0).value(QStringLiteral("title")).toString(),
             QStringLiteral("Off"));

    model.setTracks(QVariantList{
        track(QStringLiteral("video"), 1, QStringLiteral("Main"), QString(), QStringLiteral("h264"),
              true),
        track(QStringLiteral("audio"), 8, QStringLiteral("Japanese"), QStringLiteral("ja"),
              QString(), false),
    });
    QCOMPARE(model.audio_tracks().size(), qsizetype(2));
    QCOMPARE(row_at(model.audio_tracks(), 0).value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(row_at(model.audio_tracks(), 1).value(QStringLiteral("detail")).toString(), QString());

    model.setTracks(QVariantList{
        track(QStringLiteral("sub"), 7, QStringLiteral("Sidecar"), QString(), QString(), false,
              QStringLiteral("/tmp/sidecar.srt")),
    });
    QCOMPARE(model.subtitle_tracks().size(), qsizetype(2));
    QCOMPARE(row_at(model.subtitle_tracks(), 1).value(QStringLiteral("detail")).toString(),
             QStringLiteral("External Subtitles"));
    QCOMPARE(row_at(model.subtitle_tracks(), 0).value(QStringLiteral("selected")).toBool(), true);
}

void PlayerInspectorViewModelTest::untitledTrackRows() {
    // HSMpvClient assigns a title from the raw type and ID before
    // VideoTrack.displayName appends a nonempty language.
    PlayerInspectorViewModel model;
    model.setTracks(QVariantList{
        track(QStringLiteral("video"), 1, QString(), QString(), QStringLiteral("h264"), true),
        track(QStringLiteral("audio"), 2, QStringLiteral(""), QStringLiteral("ja"),
              QStringLiteral("aac"), true),
        track(QStringLiteral("sub"), 5, QString(), QStringLiteral("en"), QStringLiteral("subrip"),
              false, QStringLiteral("/tmp/sidecar.srt")),
    });

    QCOMPARE(row_at(model.video_tracks(), 0).value(QStringLiteral("title")).toString(),
             QStringLiteral("Video 1"));
    QCOMPARE(row_at(model.audio_tracks(), 1).value(QStringLiteral("title")).toString(),
             QStringLiteral("Audio 2 · ja"));
    QCOMPARE(row_at(model.subtitle_tracks(), 1).value(QStringLiteral("title")).toString(),
             QStringLiteral("Sub 5 · en"));
}

void PlayerInspectorViewModelTest::selectingTracksRequestsMpvIds() {
    PlayerInspectorViewModel model;
    QSignalSpy spy(&model, &PlayerInspectorViewModel::selectTrackRequested);

    model.selectTrack(QStringLiteral("audio"), 2);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("audio"));
    QCOMPARE(spy.at(0).at(1).toInt(), 2);

    model.selectTrack(QStringLiteral("video"), 1);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("video"));
    QCOMPARE(spy.at(1).at(1).toInt(), 1);

    model.selectTrack(QStringLiteral("sub"), 0);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(2).at(0).toString(), QStringLiteral("sub"));
    QCOMPARE(spy.at(2).at(1).toInt(), 0);
}

void PlayerInspectorViewModelTest::subtitleTimingValueText() {
    struct Case {
        double delay;
        int milliseconds;
        const char* text;
    };
    const Case cases[] = {
        {0.0, 0, "+0 ms"},          {0.05, 50, "+50 ms"},
        {-1.5, -1500, "-1500 ms"},  {-61.0, -60000, "-60000 ms"},
        {61.0, 60000, "+60000 ms"}, {0.0005, 1, "+1 ms"},
        {-0.0005, -1, "-1 ms"},
    };
    for (const Case& entry : cases) {
        PlayerInspectorViewModel model;
        model.setPlaybackState(1.0, entry.delay, 0.0);
        QCOMPARE(model.subtitle_milliseconds(), entry.milliseconds);
        QCOMPARE(model.subtitle_value_text(), QString::fromLatin1(entry.text));
    }
}

void PlayerInspectorViewModelTest::subtitleTimingCommands() {
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::subtitleDelayRequested);
        model.stepSubtitleMilliseconds(-1000);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), -1.0);
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::subtitleDelayRequested);
        model.stepSubtitleMilliseconds(50);
        model.stepSubtitleMilliseconds(50);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(double_from_signal(spy, 1), 0.1);
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::subtitleDelayRequested);
        model.setSubtitleMilliseconds(60050);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 60.0);
        QCOMPARE(model.subtitle_milliseconds(), 60000);
        QCOMPARE(model.subtitle_input_text(), QStringLiteral("60000"));
    }
    {
        PlayerInspectorViewModel model;
        model.setPlaybackState(1.0, -12.0, 0.0);
        QCOMPARE(model.subtitle_slider_milliseconds(), -10000);
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::subtitleDelayRequested);
        model.setSubtitleInput(QStringLiteral(" 1,5 "));
        model.commitSubtitleInput();
        QCOMPARE(spy.count(), 0);
        QCOMPARE(model.subtitle_input_text(), QStringLiteral("0"));
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::subtitleDelayRequested);
        model.setSubtitleInput(QStringLiteral("+250"));
        model.commitSubtitleInput();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 0.25);
        QCOMPARE(model.subtitle_input_text(), QStringLiteral("250"));
    }
    {
        PlayerInspectorViewModel model;
        model.setPlaybackState(1.0, 0.25, 0.0);
        QCOMPARE(model.subtitle_input_text(), QStringLiteral("250"));
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::subtitleDelayRequested);
        model.setSubtitleSliderMilliseconds(9999.6);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 10.0);
        QCOMPARE(model.subtitle_input_text(), QStringLiteral("10000"));
    }
}

void PlayerInspectorViewModelTest::audioTiming() {
    {
        PlayerInspectorViewModel model;
        QCOMPARE(model.audio_value_text(), QStringLiteral("+0.0 s"));
        QVERIFY(model.audio_neutral());
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::audioDelayRequested);
        model.stepAudioDelay(0.5);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 0.5);
        QCOMPARE(model.audio_value_text(), QStringLiteral("+0.5 s"));
        QVERIFY(!model.audio_neutral());
    }
    {
        PlayerInspectorViewModel model;
        model.setPlaybackState(1.0, 0.0, 30.0);
        QSignalSpy spy(&model, &PlayerInspectorViewModel::audioDelayRequested);
        model.stepAudioDelay(0.5);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 30.0);
        QCOMPARE(model.audio_value_text(), QStringLiteral("+30.0 s"));
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::audioDelayRequested);
        model.setAudioDelay(-31.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), -30.0);
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::audioDelayRequested);
        model.setAudioDelay(0.04);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 0.04);
        QCOMPARE(model.audio_delay(), 0.04);
        QCOMPARE(model.audio_value_text(), QStringLiteral("+0.0 s"));
        QVERIFY(!model.audio_neutral());
    }
    {
        PlayerInspectorViewModel model;
        model.setPlaybackState(1.0, 0.0, 2.5);
        QSignalSpy spy(&model, &PlayerInspectorViewModel::audioDelayRequested);
        model.setAudioDelay(0.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 0.0);
    }
}

void PlayerInspectorViewModelTest::playbackSpeed() {
    {
        PlayerInspectorViewModel model;
        const QVariantList expected{
            QVariantList{0.25, 0.5, 1.0, 1.5},
            QVariantList{2.0, 3.0, 4.0, 5.0},
        };
        QCOMPARE(model.speed_rows(), expected);
        QVERIFY(model.isSpeedSelected(1.0));
        QVERIFY(!model.isSpeedSelected(0.5));
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::speedRequested);
        model.chooseSpeed(0.27);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 0.3);
        QCOMPARE(model.speed_label(), QStringLiteral("0.3x"));
        QCOMPARE(model.speed_input_text(), QStringLiteral("0.3"));
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::speedRequested);
        model.setSpeedInput(QStringLiteral("1,5"));
        model.commitSpeedInput();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 1.5);
        QCOMPARE(model.speed_label(), QStringLiteral("1.5x"));
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::speedRequested);
        model.setSpeedInput(QStringLiteral("fast"));
        model.commitSpeedInput();
        QCOMPARE(spy.count(), 0);
        QCOMPARE(model.speed_input_text(), QStringLiteral("1"));
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::speedRequested);
        model.chooseSpeed(0.2);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 0.25);
        QCOMPARE(model.speed_label(), QStringLiteral("0.25x"));
    }
    {
        PlayerInspectorViewModel model;
        QSignalSpy spy(&model, &PlayerInspectorViewModel::speedRequested);
        model.chooseSpeed(9.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(double_from_signal(spy), 5.0);
        QCOMPARE(model.speed_label(), QStringLiteral("5x"));
    }
    {
        PlayerInspectorViewModel model;
        model.setPlaybackState(2.0, 0.0, 0.0);
        QCOMPARE(model.speed_label(), QStringLiteral("2x"));
        QCOMPARE(model.speed_input_text(), QStringLiteral("2"));
        QVERIFY(model.isSpeedSelected(2.0));
    }
}

QTEST_APPLESS_MAIN(PlayerInspectorViewModelTest)

#include "tst_player_inspector_view_model.moc"

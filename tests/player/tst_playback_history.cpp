// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for PlaybackHistory, following script/test_video_playback_history.swift
// and VideoPlaybackHistoryStore.updatePlaybackState, markWatched,
// clearProgress and VideoPlaybackState.isResumable/progress in
// Features/Video/VideoPlaybackHistoryStore.swift.

#include "services/player/playback_history.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using niratan::player::PlaybackHistory;
using niratan::player::PlaybackState;

namespace {

const QString kShow = QStringLiteral("/tmp/Show 01.mkv");
const QDateTime kDate = QDateTime::fromSecsSinceEpoch(1'700'000'000, QTimeZone::UTC);
// kDate as Foundation's timeIntervalSinceReferenceDate.
constexpr double kDateReference = 1'700'000'000.0 - 978'307'200.0;

std::filesystem::path path_in(const QTemporaryDir& dir, const char* name) {
    return std::filesystem::path(dir.path().toStdString()) / name;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

QJsonObject read_object(const std::filesystem::path& path) {
    return QJsonDocument::fromJson(QByteArray::fromStdString(read_text(path))).object();
}

void write_object(const std::filesystem::path& path, const QJsonObject& object) {
    write_text(path, QJsonDocument(object).toJson().toStdString());
}

}  // namespace

class PlaybackHistoryTest : public QObject {
    Q_OBJECT

   private slots:
    void save_midway_resumesAtSavedPosition() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        QVERIFY(history.save(kShow, 42.5, 100, kDate));
        QCOMPARE(history.resume_position(kShow), std::optional<double>(42.5));
    }

    void save_nearEnd_recordsFinishedStateWithoutRestorePoint() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        history.save(kShow, 42.5, 100, kDate);
        QVERIFY(history.save(kShow, 98, 100, kDate));
        QCOMPARE(history.resume_position(kShow), std::nullopt);
        const auto state = history.state(kShow);
        QVERIFY(state.has_value());
        QVERIFY(state->finished);
        QCOMPARE(state->position, 100.0);
        QCOMPARE(state->duration, std::optional<double>(100.0));
        QVERIFY(!read_object(history.file()).value("positions").toObject().contains(kShow));
    }

    void save_ninetyNineOfHundred_isFinished() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        history.save(kShow, 99, 100, kDate);
        QCOMPARE(history.resume_position(kShow), std::nullopt);
        QVERIFY(history.state(kShow)->finished);
    }

    void save_unknownDuration_removesEntry() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        history.save(kShow, 42.5, 100, kDate);
        QVERIFY(history.save(kShow, 1, 0, kDate));
        QCOMPARE(history.resume_position(kShow), std::nullopt);
        QCOMPARE(history.state(kShow), std::nullopt);
    }

    void save_underTwoSeconds_removesEntry() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        history.save(kShow, 42.5, 100, kDate);
        QVERIFY(history.save(kShow, 1.9, 100, kDate));
        QCOMPARE(history.state(kShow), std::nullopt);
    }

    void save_nothingStoredAndTooEarly_reportsNoChange() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        QVERIFY(!history.save(kShow, 1, 100, kDate));
    }

    void save_preservesPositionDurationAndDate() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        history.save(kShow, 45, 100, kDate);
        const auto state = history.state(kShow);
        QVERIFY(state.has_value());
        QCOMPARE(state->position, 45.0);
        QCOMPARE(state->duration, std::optional<double>(100.0));
        QCOMPARE(state->updated_at, kDate);
        QVERIFY(!state->finished);
        QVERIFY(state->resume_options.isEmpty());
        QCOMPARE(history.resume_position(kShow), std::optional<double>(45.0));
    }

    void save_writesSwiftSnapshotMembers() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        history.save(kShow, 45, 100, kDate);
        const auto document = read_object(history.file());
        QCOMPARE(document.value("positions").toObject().value(kShow).toDouble(), 45.0);
        const auto state = document.value("playbackStates").toObject().value(kShow).toObject();
        QCOMPARE(state.value("position").toDouble(), 45.0);
        QCOMPARE(state.value("duration").toDouble(), 100.0);
        QCOMPARE(state.value("updatedAt").toDouble(), kDateReference);
        QCOMPARE(state.value("isFinished").toBool(true), false);
        QVERIFY(state.value("resumeOptions").isObject());
    }

    void save_keepsSubtitleMembers() {
        QTemporaryDir dir;
        const auto file = path_in(dir, "history.json");
        const QJsonObject selections{{kShow, QJsonObject{{"off", QJsonObject{}}}}};
        const QJsonObject paths{{kShow, QStringLiteral("/tmp/Show 01.srt")}};
        write_object(file, {{"subtitleSelections", selections}, {"externalSubtitlePaths", paths}});
        PlaybackHistory history(file);
        history.save(kShow, 45, 100, kDate);
        const auto document = read_object(file);
        QCOMPARE(document.value("subtitleSelections").toObject(), selections);
        QCOMPARE(document.value("externalSubtitlePaths").toObject(), paths);
    }

    void clear_removesStateAndPosition() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        history.save(kShow, 45, 100, kDate);
        QVERIFY(history.clear(kShow));
        QCOMPARE(history.state(kShow), std::nullopt);
        const auto document = read_object(history.file());
        QVERIFY(!document.value("positions").toObject().contains(kShow));
        QVERIFY(!document.value("playbackStates").toObject().contains(kShow));
        QVERIFY(!history.clear(kShow));
    }

    void markWatched_isFinishedWithFullProgressAndNoRestorePoint() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        QVERIFY(history.mark_watched(kShow, 100.0, kDate));
        const auto state = history.state(kShow);
        QVERIFY(state.has_value());
        QVERIFY(state->finished);
        QCOMPARE(state->progress(), std::optional<double>(1.0));
        QCOMPARE(history.resume_position(kShow), std::nullopt);
        QVERIFY(!history.mark_watched(kShow, 100.0, kDate));
    }

    void markWatched_unknownDuration_storesZeroPosition() {
        QTemporaryDir dir;
        PlaybackHistory history(path_in(dir, "history.json"));
        history.mark_watched(kShow, std::nullopt, kDate);
        const auto state = history.state(kShow);
        QVERIFY(state.has_value());
        QCOMPARE(state->position, 0.0);
        QCOMPARE(state->duration, std::nullopt);
        QVERIFY(!read_object(history.file()).value("playbackStates").toObject().value(kShow)
                     .toObject().contains("duration"));
    }

    void legacyPosition_readsAsStateWithoutDuration() {
        QTemporaryDir dir;
        const auto file = path_in(dir, "history.json");
        write_object(file, {{"positions", QJsonObject{{kShow, 37}}}});
        PlaybackHistory history(file);
        const auto state = history.state(kShow);
        QVERIFY(state.has_value());
        QCOMPARE(state->position, 37.0);
        QCOMPARE(state->duration, std::nullopt);
        QCOMPARE(state->updated_at, QDateTime::fromSecsSinceEpoch(-62'135'596'800, QTimeZone::UTC));
        QCOMPARE(history.resume_position(kShow), std::optional<double>(37.0));
    }

    void legacyEncodedState_decodesWithEmptyOptionsAndUnfinished() {
        QTemporaryDir dir;
        const auto file = path_in(dir, "history.json");
        const QJsonObject state{{"position", 12}, {"duration", 100}, {"updatedAt", kDateReference}};
        write_object(file, {{"playbackStates", QJsonObject{{kShow, state}}}});
        PlaybackHistory history(file);
        const auto read = history.state(kShow);
        QVERIFY(read.has_value());
        QVERIFY(!read->finished);
        QVERIFY(read->resume_options.isEmpty());
        QCOMPARE(read->updated_at, kDate);
        QCOMPARE(history.resume_position(kShow), std::optional<double>(12.0));
    }

    void finishedState_isNotResumableEvenWithLegacyPosition() {
        QTemporaryDir dir;
        const auto file = path_in(dir, "history.json");
        const QJsonObject state{{"position", 100}, {"duration", 100}, {"updatedAt", kDateReference},
                                {"isFinished", true}};
        write_object(file, {{"positions", QJsonObject{{kShow, 40}}},
                            {"playbackStates", QJsonObject{{kShow, state}}}});
        QCOMPARE(PlaybackHistory(file).resume_position(kShow), std::nullopt);
    }

    void resumable_followsSwiftThresholds() {
        const auto state = [](double position, std::optional<double> duration, bool finished = false) {
            PlaybackState result;
            result.position = position;
            result.duration = duration;
            result.finished = finished;
            return result;
        };
        QVERIFY(!state(1.9, 100).resumable());
        QVERIFY(state(2, 100).resumable());
        QVERIFY(state(97, 100).resumable());
        QVERIFY(!state(98, 100).resumable());
        QVERIFY(state(500, std::nullopt).resumable());
        QVERIFY(state(500, 0).resumable());
        QVERIFY(!state(50, 100, true).resumable());
    }

    void progress_clampsAndNeedsDuration() {
        PlaybackState state;
        state.position = 150;
        state.duration = 100;
        QCOMPARE(state.progress(), std::optional<double>(1.0));
        state.position = 25;
        QCOMPARE(state.progress(), std::optional<double>(0.25));
        state.duration = std::nullopt;
        QCOMPARE(state.progress(), std::nullopt);
        state.duration = 0;
        QCOMPARE(state.progress(), std::nullopt);
        state.finished = true;
        QCOMPARE(state.progress(), std::optional<double>(1.0));
    }

    void missingFile_migratesEarlierLinuxPositions() {
        QTemporaryDir dir;
        const auto file = path_in(dir, "history.json");
        const auto legacy = path_in(dir, "legacy.json");
        write_object(legacy, {{kShow, QJsonObject{{"position", 37}, {"duration", 100}}}});
        PlaybackHistory history(file, legacy);
        QCOMPARE(history.resume_position(kShow), std::optional<double>(37.0));
        const QString other = QStringLiteral("/tmp/Other.mkv");
        history.save(other, 20, 100, kDate);
        const auto positions = read_object(file).value("positions").toObject();
        QCOMPARE(positions.value(kShow).toDouble(), 37.0);
        QCOMPARE(positions.value(other).toDouble(), 20.0);
    }

    void existingFile_ignoresEarlierLinuxPositions() {
        QTemporaryDir dir;
        const auto file = path_in(dir, "history.json");
        const auto legacy = path_in(dir, "legacy.json");
        write_object(file, {{"positions", QJsonObject{}}});
        write_object(legacy, {{kShow, QJsonObject{{"position", 37}}}});
        QCOMPARE(PlaybackHistory(file, legacy).resume_position(kShow), std::nullopt);
    }

    void damagedFile_readsEmptyAndIsNotReplaced() {
        QTemporaryDir dir;
        const auto file = path_in(dir, "history.json");
        write_text(file, "{ damaged");
        PlaybackHistory history(file);
        QCOMPARE(history.state(kShow), std::nullopt);
        QVERIFY(!history.save(kShow, 45, 100, kDate));
        QCOMPARE(read_text(file), std::string("{ damaged"));
    }

    void undecodableSnapshot_readsEmptyAndStartsOver() {
        QTemporaryDir dir;
        const auto file = path_in(dir, "history.json");
        write_object(file, {{"positions", QJsonObject{{kShow, QStringLiteral("forty")}}}});
        PlaybackHistory history(file);
        QCOMPARE(history.state(kShow), std::nullopt);
        QVERIFY(history.save(kShow, 45, 100, kDate));
        QCOMPARE(history.resume_position(kShow), std::optional<double>(45.0));
    }

    void emptyPath_storesNothing() {
        PlaybackHistory history{std::filesystem::path{}};
        QVERIFY(!history.save(kShow, 45, 100, kDate));
        QCOMPARE(history.state(kShow), std::nullopt);
    }
};

QTEST_APPLESS_MAIN(PlaybackHistoryTest)
#include "tst_playback_history.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Flow 4: match an audiobook to the open book and carry its cues through the
// reader, the player and a book reopen.
//
// The test drives the same view models and services the reader page uses, with
// one headless QWebEnginePage for the reader shell:
//
// - Features/Sasayaki/SasayakiParser.swift (parseCues: block index as id, text
//   trimmed and joined with one space), SasayakiMatcher.match(rootURL:) and
//   SasayakiTextMatcher.match (placements and reader-character start/length),
//   following SasayakiPlayer.updateMatchData and
//   BookStorage.loadSasayakiMatch.
// - SasayakiPlayer.cues(for:), NativeReaderView.updateState(sasayakiCues:) and
//   reader.js applySasayakiCues/collectSasayakiCueRanges (the
//   span.hoshi-sasayaki-cue wrappers), highlightSasayakiCue/clearSasayakiCue
//   (hoshi-sasayaki-active).
// - SasayakiPlayer.handleRestoreCompleted, updateCue, displayCue and the
//   delay/rate observers, then savePlayback, reloadPlayback and restoreAudio
//   with BookStorage.loadSasayakiPlayback.
//
// The reader page's wiring is mirrored where the views are not loaded:
// ReaderPage.qml's onDocumentRestored applies the cue ranges and calls
// restoreCompleted(chapterIndex); ReaderPage.highlightSasayakiCue passes the
// cue id and reveal flag.
//
// The imported EPUB and the SRT next to it come from this test. The audio is a
// placeholder file of arbitrary bytes: no media backend is required, because
// the flow asserts model state only (Decision 1).

#include "viewmodels/reader/reader_view_model.h"
#include "viewmodels/sasayaki/sasayaki_view_model.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTimer>
#include <QUrl>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <filesystem>
#include <optional>
#include <string>

#include "data_root.h"
#include "epub_fixture.h"

#include "models/sasayaki/sasayaki.h"
#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"

using niratan::books::BookStorage;
using niratan::sasayaki::SasayakiMatchData;
using niratan::sasayaki::SasayakiPlaybackData;
using niratan::test::TempDataRoot;
using niratan::viewmodels::ReaderViewModel;
using niratan::viewmodels::SasayakiViewModel;

namespace {

const QString kFirstChapter = QString::fromUtf8("私は本を読む。今日は晴れです。");
const QString kSecondChapter = QString::fromUtf8("君は歌を歌う。");
const QString kFirstCue = QString::fromUtf8("私は本を読む。");
const QString kSecondCue = QString::fromUtf8("今日は晴れです。");
const QString kThirdCue = QString::fromUtf8("君は歌を歌う。");

QString chapter_path(int index) {
    return index == 0 ? QStringLiteral("first.xhtml") : QStringLiteral("second.xhtml");
}

bool write_bytes(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(bytes) == bytes.size();
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

bool saw_active_cue(const QSignalSpy& spy, const QString& id, bool reveal) {
    for (const auto& emission : spy) {
        if (emission.at(0).toString() == id && emission.at(1).toBool() == reveal) {
            return true;
        }
    }
    return false;
}

}  // namespace

// The bridge object the reader shell expects under the readerBridge name.
class ReaderBridgeProbe : public QObject {
    Q_OBJECT

   public:
    Q_INVOKABLE void message(const QString& name, const QString& payload) {
        Q_UNUSED(name);
        Q_UNUSED(payload);
    }
};

class SasayakiFlowTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void cleanupTestCase();
    void sasayaki_flow();

   private:
    ReaderViewModel* reader_ = nullptr;
};

void SasayakiFlowTest::initTestCase() {
    reader_ = ReaderViewModel::instance();
}

void SasayakiFlowTest::cleanupTestCase() {
    if (reader_ != nullptr) {
        reader_->closeBook();
    }
}

// B1: import the EPUB, match the SRT and read the saved match. B2: the
// chapter's cue ranges. B3: the reader shell applies and highlights them.
// B4: the restored position and the delay observer move the cue. B5: the
// playback sidecar and the reopen restore the position, delay, rate, audio and
// cue.
void SasayakiFlowTest::sasayaki_flow() {
    const std::filesystem::path data_root = niratan::storage::data_root();
    QVERIFY(!data_root.empty());

    // B1: the EPUB fixture, its SRT, and the model constructed after the
    // reader as the page does.
    const auto epub = data_root / "sasayaki.epub";
    QVERIFY(niratan::test::write_epub(epub, "ササヤキ", kFirstChapter.toStdString(),
                                      kSecondChapter.toStdString()));
    QVERIFY(reader_->importAndOpen(QUrl::fromLocalFile(QString::fromStdString(epub.string()))));
    const QString folder = reader_->book_folder();
    QVERIFY(!folder.isEmpty());
    QCOMPARE(reader_->chapter_index(), 0);

    const QString srt_text = QString::fromUtf8("1\n"
                                               "00:00:01,000 --> 00:00:03,000\n"
                                               "私は本を読む。\n"
                                               "\n"
                                               "2\n"
                                               "00:00:04,000 --> 00:00:06,000\n"
                                               "今日は晴れです。\n"
                                               "\n"
                                               "3\n"
                                               "00:00:08,000 --> 00:00:10,000\n"
                                               "君は歌を歌う。\n");
    const auto srt = data_root / "sasayaki.srt";
    QVERIFY(write_bytes(srt, srt_text.toUtf8()));
    QVERIFY(!reader_->document_for(chapter_path(0).toStdString()).empty());

    SasayakiViewModel sasayaki;
    QVERIFY(sasayaki.openSubtitles(QUrl::fromLocalFile(QString::fromStdString(srt.string()))));
    sasayaki.matchSubtitles(200);
    QTRY_VERIFY_WITH_TIMEOUT(!sasayaki.matching(), 15000);
    QVERIFY(sasayaki.match_error().isEmpty());
    QVERIFY(sasayaki.has_match());

    SasayakiMatchData expected_match;
    expected_match.matches = {
        {"0", 1.0, 3.0, kFirstCue.toStdString(), 0, 0, 6, false},
        {"1", 4.0, 6.0, kSecondCue.toStdString(), 0, 6, 7, false},
        {"2", 8.0, 10.0, kThirdCue.toStdString(), 1, 0, 6, false},
    };
    expected_match.unmatched = 0;
    const BookStorage storage(data_root);
    QCOMPARE(storage.load_sasayaki_match(folder.toStdString()),
             std::optional<SasayakiMatchData>{expected_match});
    QVERIFY(std::filesystem::exists(data_root / "Books" / folder.toStdString() /
                                    "sasayaki_match.json"));

    // B2: the open chapter's cue ranges, and the other chapter's.
    const QJsonArray first_ranges{
        QJsonObject{{"id", "0"}, {"start", 0}, {"length", 6}},
        QJsonObject{{"id", "1"}, {"start", 6}, {"length", 7}},
    };
    const QJsonArray second_ranges{
        QJsonObject{{"id", "2"}, {"start", 0}, {"length", 6}},
    };
    const auto cues_at = [&sasayaki]() {
        return QJsonDocument::fromJson(sasayaki.applyCuesJson().toUtf8()).array();
    };
    QCOMPARE(cues_at(), first_ranges);
    reader_->goToChapter(1);
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(cues_at(), second_ranges);
    reader_->goToChapter(0);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(cues_at(), first_ranges);

    // B3: the reader shell wraps the matched ranges and highlights one cue.
    const QString document = QString::fromStdString(reader_->document_for("first.xhtml"));
    QVERIFY(!document.isEmpty());
    QWebEnginePage page;
    QWebChannel channel(&page);
    ReaderBridgeProbe bridge;
    channel.registerObject(QStringLiteral("readerBridge"), &bridge);
    page.setWebChannel(&channel);
    QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
    page.setHtml(document, QUrl(reader_->chapter_url()));
    QVERIFY(loaded.wait(15000));
    QVERIFY(loaded.last().at(0).toBool());

    QString ready;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
        ready = evaluate(page, QStringLiteral("typeof window.hoshiReader"));
        if (ready == QStringLiteral("object")) {
            break;
        }
        QTest::qWait(50);
    }
    QCOMPARE(ready, QStringLiteral("object"));

    const QString first_cues = sasayaki.applyCuesJson();
    QVERIFY(!first_cues.isEmpty());
    evaluate(page,
             QStringLiteral("window.hoshiReader.applySasayakiCues(") + first_cues +
                 QStringLiteral("); 'ok';"));
    QCOMPARE(
        evaluate(page,
                 QStringLiteral("String(document.querySelectorAll('.hoshi-sasayaki-cue').length)")),
        QStringLiteral("2"));
    QCOMPARE(
        evaluate(page,
                 QStringLiteral("document.querySelectorAll('.hoshi-sasayaki-cue')[0].textContent")),
        QString::fromUtf8("私は本を読む"));
    QCOMPARE(
        evaluate(page,
                 QStringLiteral("document.querySelectorAll('.hoshi-sasayaki-cue')[1].textContent")),
        QString::fromUtf8("今日は晴れです"));

    evaluate(page, QStringLiteral("window.hoshiReader.highlightSasayakiCue('1', false); 'ok';"));
    QCOMPARE(evaluate(page,
                      QStringLiteral(
                          "String(document.querySelectorAll('.hoshi-sasayaki-active').length)")),
             QStringLiteral("1"));
    QCOMPARE(evaluate(page,
                      QStringLiteral(
                          "document.querySelectorAll('.hoshi-sasayaki-active')[0].textContent")),
             QString::fromUtf8("今日は晴れです"));
    evaluate(page, QStringLiteral("window.hoshiReader.clearSasayakiCue(); 'ok';"));
    QCOMPARE(evaluate(page,
                      QStringLiteral(
                          "String(document.querySelectorAll('.hoshi-sasayaki-active').length)")),
             QStringLiteral("0"));

    // B4: audio, the restored position's cue and a delay change that moves it.
    const auto audio = data_root / "sasayaki_audio.m4a";
    QVERIFY(write_bytes(audio, QByteArray("not audio")));
    const QUrl audio_url = QUrl::fromLocalFile(QString::fromStdString(audio.string()));
    const QString audio_path = audio_url.toLocalFile();
    QVERIFY(sasayaki.openAudio(audio_url));
    QVERIFY(sasayaki.has_audio());
    sasayaki.seek(4.5);
    QCOMPARE(sasayaki.position(), 4.5);

    QSignalSpy active(&sasayaki, &SasayakiViewModel::activeCueChanged);
    sasayaki.restoreCompleted(0);
    QCOMPARE(sasayaki.current_cue_id(), QStringLiteral("1"));
    QCOMPARE(sasayaki.current_cue_text(), kSecondCue);
    QTRY_VERIFY_WITH_TIMEOUT(saw_active_cue(active, QStringLiteral("1"), false), 1000);

    active.clear();
    sasayaki.setDelay(2.0);
    QCOMPARE(sasayaki.delay(), 2.0);
    QCOMPARE(sasayaki.current_cue_id(), QStringLiteral("0"));
    QCOMPARE(sasayaki.current_cue_text(), kFirstCue);
    QTRY_VERIFY_WITH_TIMEOUT(saw_active_cue(active, QStringLiteral("0"), false), 1000);

    // B5: the sidecar holds the seeked position, delay, rate and bookmark.
    sasayaki.setRate(1.25);
    SasayakiPlaybackData expected_playback;
    expected_playback.last_position = 4.5;
    expected_playback.delay = 2.0;
    expected_playback.rate = 1.25F;
    expected_playback.audio_bookmark = audio_path.toUtf8();
    QCOMPARE(storage.load_sasayaki_playback(folder.toStdString()),
             std::optional<SasayakiPlaybackData>{expected_playback});
    QVERIFY(std::filesystem::exists(data_root / "Books" / folder.toStdString() /
                                    "sasayaki_playback.json"));

    // Reopening restores the sidecar and the cue the position places.
    reader_->closeBook();
    QVERIFY(reader_->openBook(folder));
    QCOMPARE(reader_->chapter_index(), 0);
    sasayaki.restoreCompleted(reader_->chapter_index());
    QCOMPARE(sasayaki.position(), 4.5);
    QCOMPARE(sasayaki.delay(), 2.0);
    QCOMPARE(sasayaki.rate(), 1.25);
    QVERIFY(sasayaki.has_audio());
    QVERIFY(sasayaki.has_match());
    QCOMPARE(sasayaki.current_cue_id(), QStringLiteral("0"));
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    TempDataRoot data_root;
    if (!data_root.isValid()) {
        return 1;
    }
    QGuiApplication app(argc, argv);
    SasayakiFlowTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_flow_sasayaki.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The script bridge between the generated dictionary popup and the C++ side,
// loaded the way DictionaryPage.qml does it: the popup shell from
// services/dictionary/popup_html, then the WebChannel library and bridge.js
// injected into the loaded document. A message posted through
// window.webkit.messageHandlers.playWordAudio must reach the registered
// dictionaryBridge object.

#include "services/dictionary/popup_assets.h"
#include "services/dictionary/popup_html.h"

#include <QGuiApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <string>

namespace {

QString evaluate(QWebEnginePage& page, const QString& script) {
    QString result;
    bool finished = false;
    QEventLoop loop;
    page.runJavaScript(script, [&](const QVariant& value) {
        result = value.toString();
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
    return finished ? result : QString();
}

// The shell renders one entry; only the bridge is under test, so the entry
// carries the fields the popup script reads and no glossaries.
QJsonArray entries_fixture() {
    QJsonObject entry;
    entry.insert(QStringLiteral("expression"), QStringLiteral("decide"));
    entry.insert(QStringLiteral("reading"), QStringLiteral("decide"));
    entry.insert(QStringLiteral("matched"), QStringLiteral("decide"));
    entry.insert(QStringLiteral("deinflectionTraces"), QJsonArray{});
    entry.insert(QStringLiteral("deinflectionTrace"), QJsonArray{});
    entry.insert(QStringLiteral("glossaries"), QJsonArray{});
    entry.insert(QStringLiteral("frequencies"), QJsonArray{});
    entry.insert(QStringLiteral("pitches"), QJsonArray{});
    entry.insert(QStringLiteral("rules"), QJsonArray{});
    return QJsonArray{entry};
}

}  // namespace

class DictionaryBridgeProbe : public QObject {
    Q_OBJECT

   public slots:
    void playWordAudio(const QString& url) { emit audioReceived(url); }

   signals:
    void audioReceived(const QString& url);
};

class PopupBridgeTest : public QObject {
    Q_OBJECT

   private slots:
    void play_word_audio_reaches_the_bridge();
};

void PopupBridgeTest::play_word_audio_reaches_the_bridge() {
    const auto assets = niratan::dictionary::load_popup_assets();
    QVERIFY(!assets.js.empty());
    QVERIFY(!assets.web_channel.empty());
    QVERIFY(!assets.bridge.empty());

    const std::string shell = niratan::dictionary::popup_shell_html(
        entries_fixture(), {}, niratan::dictionary::PopupConfig{}, assets.css, assets.js);

    QWebEnginePage page;
    QWebChannel channel(&page);
    page.setWebChannel(&channel);
    DictionaryBridgeProbe probe;
    channel.registerObject(QStringLiteral("dictionaryBridge"), &probe);
    QSignalSpy audio_spy(&probe, &DictionaryBridgeProbe::audioReceived);

    QSignalSpy load_spy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(QString::fromStdString(shell));
    QVERIFY(load_spy.wait(15000));
    QVERIFY(load_spy.last().at(0).toBool());

    evaluate(page,
             QString::fromStdString(assets.web_channel) + QStringLiteral("\n") +
                 QString::fromStdString(assets.bridge));

    // The bridge installs the handlers once the WebChannel handshake resolves.
    QElapsedTimer timer;
    timer.start();
    QString handler_type;
    while (timer.elapsed() < 10000) {
        handler_type =
            evaluate(page, QStringLiteral("typeof window.webkit?.messageHandlers?.playWordAudio"));
        if (handler_type == QStringLiteral("object")) {
            break;
        }
        QTest::qWait(50);
    }
    QCOMPARE(handler_type, QStringLiteral("object"));

    evaluate(page,
             QStringLiteral("window.webkit.messageHandlers.playWordAudio.postMessage("
                            "{url: 'https://example.test/audio.mp3'});"));

    QTRY_COMPARE_WITH_TIMEOUT(audio_spy.count(), 1, 10000);
    QCOMPARE(audio_spy.first().first().toString(),
             QStringLiteral("https://example.test/audio.mp3"));
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();

    QTemporaryDir data_root;
    if (data_root.isValid()) {
        const QByteArray base = data_root.path().toUtf8();
        qputenv("HOME", base);
        qputenv("XDG_DATA_HOME", base + "/data");
        qputenv("XDG_CACHE_HOME", base + "/cache");
    }

    QGuiApplication app(argc, argv);
    PopupBridgeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_popup_bridge.moc"

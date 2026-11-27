// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Video/VideoPlayerScreen+Subtitles.swift: an embedded text track
// whose transcript was extracted becomes the subtitle document the overlay
// draws and looks up from, as a sidecar file does. MpvItem hands the
// transcript to QML as a byte array, which arrives as an ArrayBuffer.

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>

class VideoEmbeddedSubtitlesTest : public QObject {
    Q_OBJECT

   private slots:
    void an_extracted_transcript_shows_the_clickable_subtitles();
};

namespace {

QQuickItem* find_type(QQuickItem* item, QLatin1StringView type) {
    if (QLatin1StringView(item->metaObject()->className()).startsWith(type)) {
        return item;
    }
    for (QQuickItem* child : item->childItems()) {
        if (QQuickItem* found = find_type(child, type)) {
            return found;
        }
    }
    return nullptr;
}

}  // namespace

void VideoEmbeddedSubtitlesTest::an_extracted_transcript_shows_the_clickable_subtitles() {
    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/shell/VideoWindow.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QQuickItem* mpv = find_type(window->contentItem(), QLatin1StringView("MpvItem"));
    QQuickItem* layer = find_type(window->contentItem(), QLatin1StringView("VideoLookupLayer"));
    QVERIFY(mpv != nullptr);
    QVERIFY(layer != nullptr);
    QVERIFY(!layer->property("textDocumentLoaded").toBool());

    const QByteArray transcript("1\n00:00:00,000 --> 00:00:10,000\n星を見る\n");
    QVERIFY(QMetaObject::invokeMethod(mpv, "embeddedTranscriptChanged",
                                      Q_ARG(QByteArray, transcript), Q_ARG(QString, QString())));
    QTRY_VERIFY2(layer->property("textDocumentLoaded").toBool(),
                 "the extracted transcript did not load the subtitle document");
    QVERIFY(!mpv->property("nativeSubtitleRendering").toBool());

    // A failed or empty extraction leaves the document unloaded again.
    QVERIFY(QMetaObject::invokeMethod(mpv, "embeddedTranscriptChanged",
                                      Q_ARG(QByteArray, QByteArray()), Q_ARG(QString, QString())));
    QTRY_VERIFY(!layer->property("textDocumentLoaded").toBool());
}

int main(int argc, char* argv[]) {
    // Keep profile, cache and settings paths out of the developer's home.
    QTemporaryDir data_root;
    if (data_root.isValid()) {
        const QByteArray base = data_root.path().toUtf8();
        qputenv("HOME", base);
        qputenv("XDG_DATA_HOME", base + "/data");
        qputenv("XDG_CACHE_HOME", base + "/cache");
        qputenv("XDG_CONFIG_HOME", base + "/config");
    }

    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    VideoEmbeddedSubtitlesTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_video_embedded_subtitles.moc"

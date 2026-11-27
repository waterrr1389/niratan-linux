// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Video/Subtitles/InteractiveSubtitleTextView.swift
// (ClickableSubtitleTextView.mouseDown) in the player window: a click on a
// subtitle reaches the subtitle row and opens the lookup. The stage's hover
// and wheel handling sit above the video and must not take the click.

#include "services/dictionary/dictionary_store.h"
#include "services/storage/app_paths.h"

#include "../dictionary/yomitan_fixture.h"
#include "data_root.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTest>
#include <QUrl>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>
#include <filesystem>

class VideoSubtitleClickTest : public QObject {
    Q_OBJECT

   private slots:
    void a_click_on_a_subtitle_opens_the_lookup();
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

bool import_dictionary(const QString& folder) {
    const auto zip = std::filesystem::path(folder.toStdString()) / "dictionary.zip";
    niratan::test::write_stored_zip(
        zip,
        {{QStringLiteral("index.json"), niratan::test::minimal_index(QStringLiteral("Probe"))},
         {QStringLiteral("term_bank_1.json"),
          QByteArray(R"([["星","ほし","","n",0,["star"],1,""]])")}});
    return niratan::dictionary::DictionaryStore(niratan::storage::dictionaries_root())
        .import_zip(zip)
        .ok;
}

}  // namespace

void VideoSubtitleClickTest::a_click_on_a_subtitle_opens_the_lookup() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    QVERIFY(import_dictionary(root.path()));
    const QString video = QDir(root.path()).filePath(QStringLiteral("Alpha.mkv"));
    QFile file(video);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/shell/VideoWindow.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(QMetaObject::invokeMethod(
        window, "openVideo", Q_ARG(QVariant, QVariant(QUrl::fromLocalFile(video))),
        Q_ARG(QVariant, QVariant(QUrl())), Q_ARG(QVariant, QVariant(false))));
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickItem* mpv = find_type(window->contentItem(), QLatin1StringView("MpvItem"));
    QVERIFY(mpv != nullptr);
    QObject* lookup = nullptr;
    for (QObject* object : window->findChildren<QObject*>()) {
        if (object->inherits("niratan::viewmodels::PlayerLookupViewModel")) {
            lookup = object;
        }
    }
    QVERIFY(lookup != nullptr);

    // The engine reports a loaded subtitle document; the lookup model then
    // places the cue at the playback time.
    const QUrl subtitle = QUrl::fromLocalFile(QDir(root.path()).filePath(QStringLiteral("a.srt")));
    QVERIFY(QMetaObject::invokeMethod(
        mpv, "subtitleDocumentChanged", Q_ARG(QUrl, subtitle),
        Q_ARG(QByteArray, QByteArray("1\n00:00:00,000 --> 00:00:10,000\n星を見る\n")),
        Q_ARG(bool, true)));
    QVERIFY(QMetaObject::invokeMethod(lookup, "updatePlayback", Q_ARG(double, 1.0),
                                      Q_ARG(double, 0.0)));

    QQuickItem* row = nullptr;
    QTRY_VERIFY((row = find_type(window->contentItem(), QLatin1StringView("VideoSubtitleText")),
                 row != nullptr && row->isVisible() && row->height() > 0));
    QVariant rect;
    QVERIFY(QMetaObject::invokeMethod(row, "characterRect", Q_RETURN_ARG(QVariant, rect),
                                      Q_ARG(QVariant, 0)));
    QTest::mouseClick(window, Qt::LeftButton, {},
                      row->mapToScene(rect.toRectF().center()).toPoint());
    QTRY_VERIFY2(lookup->property("hasPopup").toBool(), "the click did not reach the subtitle");
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    VideoSubtitleClickTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_video_subtitle_click.moc"

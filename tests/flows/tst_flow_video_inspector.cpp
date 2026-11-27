// SPDX-License-Identifier: GPL-3.0-or-later
//
// VideoInspectorView.subtitleSourcesSection and subtitleTimingSection,
// VideoPlayerScreen+Opening.openVideo and VideoPlayerScreen+Subtitles
// (handleSubtitleImport, selectSubtitleTrack, applySubtitlesOff).

#include "viewmodels/player/player_inspector_view_model.h"
#include "views/player/mpv_item.h"

#include "data_root.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>

namespace {

QQuickItem* find_control(QQuickItem* item, const QString& name, int subtitle_id = -1) {
    if (item->objectName() == name &&
        (subtitle_id < 0 ||
         (item->property("rowType").toString() == QStringLiteral("sub") &&
          item->property("rowId").toInt() == subtitle_id))) {
        return item;
    }
    for (QQuickItem* child : item->childItems()) {
        if (QQuickItem* control = find_control(child, name, subtitle_id)) {
            return control;
        }
    }
    return nullptr;
}

bool click(QQuickItem* item) {
    return item != nullptr && QMetaObject::invokeMethod(item, "clicked");
}

QUrl write_fixture(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        return {};
    }
    return QUrl::fromLocalFile(path);
}

QVariantMap subtitle_track(int id, const QString& language, bool selected) {
    return {{QStringLiteral("type"), QStringLiteral("sub")},
            {QStringLiteral("id"), id},
            {QStringLiteral("language"), language},
            {QStringLiteral("isSelected"), selected}};
}

}  // namespace

class FlowVideoInspectorTest : public QObject {
    Q_OBJECT

   private slots:
    void change_subtitles_for_open_video();
};

void FlowVideoInspectorTest::change_subtitles_for_open_video() {
    niratan::test::TempDataRoot data_root;
    QVERIFY(data_root.isValid());
    // The headless player needs a local path to open, but cannot play video.
    const QUrl video = write_fixture(data_root.path() + QStringLiteral("/Episode.mkv"), {});
    const QUrl subtitle = write_fixture(data_root.path() + QStringLiteral("/External.srt"),
                                        "1\n00:00:00,000 --> 00:00:03,000\nExternal subtitle\n");
    QVERIFY(video.isLocalFile());
    QVERIFY(subtitle.isLocalFile());

    QQmlApplicationEngine engine;
    engine.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto* main_window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(main_window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(main_window));
    auto* window = main_window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    QVERIFY(window != nullptr);
    QVERIFY(QMetaObject::invokeMethod(window, "openVideo", Q_ARG(QVariant, QVariant(video)),
                                      Q_ARG(QVariant, QVariant(QUrl())),
                                      Q_ARG(QVariant, QVariant(false))));
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto* player = window->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"));
    QVERIFY(player != nullptr);
    QCOMPARE(player->property("videoFile").toUrl(), video);
    auto* mpv = player->findChild<MpvItem*>(QStringLiteral("mpvItem"));
    QVERIFY(mpv != nullptr);
    auto* model = player->findChild<niratan::viewmodels::PlayerInspectorViewModel*>(
        QStringLiteral("inspectorModel"));
    QVERIFY(model != nullptr);
    QQuickItem* panel = find_control(player, QStringLiteral("videoInspectorPanel"));
    QVERIFY(panel != nullptr);
    QVERIFY(!panel->isVisible());
    // Playback chrome is hidden until mpv loads media; exercise the same
    // button signal rather than hit-testing it in the offscreen window.
    QVERIFY(click(find_control(player, QStringLiteral("inspectorButton"))));
    QTRY_VERIFY(panel->isVisible());

    model->setTracks({subtitle_track(4, QStringLiteral("ja"), true),
                      subtitle_track(5, QStringLiteral("en"), false)});
    QSignalSpy selections(panel, SIGNAL(selectTrackRequested(QString, int)));
    QVERIFY(selections.isValid());
    for (const int id : {0, 5, 4}) {
        QVERIFY(click(find_control(panel, QStringLiteral("inspectorChoiceRow"), id)));
        QCOMPARE(selections.size(), 1);
        QCOMPARE(selections.takeFirst(), (QVariantList{QStringLiteral("sub"), id}));
        QTRY_COMPARE(mpv->subtitlesVisible(), id != 0);
    }

    QVERIFY(click(find_control(panel, QStringLiteral("inspectorSubtitleForward50"))));
    QTRY_COMPARE(mpv->subtitleDelay(), 0.05);
    QCOMPARE(model->subtitle_milliseconds(), 50);
    QQuickItem* offset = find_control(panel, QStringLiteral("inspectorSubtitleOffsetField"));
    QVERIFY(offset != nullptr);
    QVERIFY(offset->setProperty("text", QStringLiteral("250")));
    QVERIFY(QMetaObject::invokeMethod(offset, "accepted"));
    QTRY_COMPARE(mpv->subtitleDelay(), 0.25);
    QCOMPARE(model->subtitle_milliseconds(), 250);

    QSignalSpy file_requests(panel, SIGNAL(openSubtitleFileRequested(QVariant)));
    QVERIFY(file_requests.isValid());
    QVERIFY(click(find_control(panel, QStringLiteral("inspectorOpenSubtitles"))));
    auto* dialog = window->findChild<QObject*>(QStringLiteral("inspectorSubtitleFileDialog"));
    QVERIFY(dialog != nullptr);
    QTRY_VERIFY(dialog->property("visible").toBool());
    QVERIFY(dialog->setProperty("selectedFile", subtitle));
    QVERIFY(QMetaObject::invokeMethod(dialog, "accepted"));
    QCOMPARE(file_requests.size(), 1);
    QCOMPARE(file_requests.first().first().toUrl(), subtitle);

    // Without active media mpv cannot add a track, but the failure reaches
    // the player and appears in its error dialog.
    QObject* alert = nullptr;
    for (QObject* child : window->findChildren<QObject*>()) {
        if (child->property("title").toString() == QStringLiteral("Video Error")) {
            alert = child;
            break;
        }
    }
    QVERIFY(alert != nullptr);
    QTRY_COMPARE(alert->property("message").toString(),
                 QStringLiteral("Could not load the subtitle file."));
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));
    FlowVideoInspectorTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_flow_video_inspector.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dedicated Video window, following NativeMac/VideoWindowPresenter.swift
// (`VideoWindowGeometry`, `makeWindow`, `defaultVideoWindowFrame`, `open`,
// `windowWillClose`) and Features/Video/VideoWindowCoordinator.swift
// (`requestOpen`, `sessionID`, `windowDidAppear`, `windowDidDisappear`), with
// the empty and loading surfaces of Features/Video/VideoPlayerScreen.swift
// `videoSurface`.
//
// The tests cover the window contract from script/test_video_window_contract.swift
// (minimum and default size, no maximum, one window re-used for later opens),
// the library handoff, close/unload/refresh and the no-video copy. They assert
// loading, fit and wiring only, never a numeric layout value.
//
// The data root is a temporary directory with generated videos, a bound
// subtitle and saved playback positions, as in the library view tests.

#include "../viewmodels/data_root.h"

#include "services/player/playback_history.h"
#include "services/storage/app_paths.h"
#include "services/video/library_catalog.h"
#include "services/video/thumbnail_store.h"

#include <QBuffer>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMetaObject>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QWindow>
#include <QTemporaryDir>
#include <QUrl>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

#include <algorithm>
#include <clocale>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>

Q_IMPORT_QML_PLUGIN(NiratanPlugin)
Q_IMPORT_QML_PLUGIN(NiratanStylePlugin)

namespace {

QStringList g_warnings;
QtMessageHandler g_previous_handler = nullptr;

void collect_qml_messages(QtMsgType type, const QMessageLogContext& context,
                          const QString& message) {
    if (type == QtWarningMsg || type == QtCriticalMsg) {
        const QString category = QString::fromUtf8(context.category ? context.category : "");
        if (category.startsWith(QStringLiteral("qml")) ||
            message.startsWith(QStringLiteral("QML")) || message.contains(QStringLiteral(".qml"))) {
            g_warnings.append(message);
        }
    }
    if (g_previous_handler != nullptr) {
        g_previous_handler(type, context, message);
    }
}

QString warnings_after(const QString& label) {
    QTest::qWait(50);
    QString report;
    if (!g_warnings.isEmpty()) {
        report = label + QStringLiteral(":\n") + g_warnings.join('\n');
    }
    g_warnings.clear();
    return report;
}

void collect_visual_texts(QQuickItem* item, QStringList& texts) {
    const QVariant text = item->property("text");
    if (text.isValid() && !text.toString().isEmpty()) {
        texts.append(text.toString());
    }
    for (QQuickItem* child : item->childItems()) {
        collect_visual_texts(child, texts);
    }
}

QStringList visual_texts(QQuickItem* page) {
    QStringList texts;
    collect_visual_texts(page, texts);
    return texts;
}

void collect_named(QQuickItem* item, const QString& name, QList<QQuickItem*>& found) {
    if (item->objectName() == name) {
        found.append(item);
    }
    for (QQuickItem* child : item->childItems()) {
        collect_named(child, name, found);
    }
}

QQuickItem* item_named(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> found;
    if (root != nullptr) {
        collect_named(root, name, found);
    }
    return found.isEmpty() ? nullptr : found.first();
}

QQuickItem* card_with_id(QQuickItem* root, const QString& id) {
    QList<QQuickItem*> found;
    collect_named(root, QStringLiteral("videoCard"), found);
    for (QQuickItem* card : found) {
        if (card->property("rowID").toString() == id) {
            return card;
        }
    }
    return nullptr;
}

void click(QQuickItem* item) {
    QVERIFY(item != nullptr);
    QVERIFY(QMetaObject::invokeMethod(item, "clicked"));
}

QString touch_file(const QString& path, int size) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(QByteArray(size, 'x'));
    file.close();
    return QFileInfo(path).absoluteFilePath();
}

void seed_thumbnail_cache() {
    const auto directory = niratan::video::default_thumbnail_cache_directory();
    if (directory.empty()) {
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return;
    }
    QImage image(16, 9, QImage::Format_RGB32);
    image.fill(QColor(52, 96, 132));
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    const QByteArray bytes = buffer.data();

    niratan::video::LibraryStore store;
    for (const auto& item : store.catalog().items) {
        const auto request = niratan::video::ThumbnailRequest::for_item(item);
        const auto file =
            directory / (niratan::video::thumbnail_cache_key(request).toStdString() + ".png");
        QFile output(QString::fromStdString(file.string()));
        if (output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            output.write(bytes);
        }
    }
}

struct Fixture {
    QString source;
    QString alpha;
    QString beta;
    QString subtitle;
};

Fixture make_fixture(const QString& base) {
    Fixture fixture;
    fixture.source = base + QStringLiteral("/Anime");
    QDir().mkpath(fixture.source);
    fixture.alpha = touch_file(fixture.source + QStringLiteral("/Alpha.mp4"), 10);
    fixture.beta = touch_file(fixture.source + QStringLiteral("/Beta.mkv"), 30);
    fixture.subtitle = touch_file(fixture.source + QStringLiteral("/Alpha.ja.srt"), 4);

    const auto root = niratan::storage::data_root();
    niratan::video::LibraryStore store(root / "video_library.json");
    const auto source = store.add_source(std::filesystem::path(fixture.source.toStdString()));
    store.scan_source(source.id);
    store.bind_subtitle(fixture.alpha, fixture.subtitle);
    niratan::player::PlaybackHistory history(root / "video_playback_history.json");
    history.save(fixture.alpha, 20, 100);
    history.save(fixture.beta, 50, 100);
    seed_thumbnail_cache();
    return fixture;
}

QQuickWindow* load_main_window(QQmlApplicationEngine& engine) {
    engine.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    if (engine.rootObjects().isEmpty()) {
        return nullptr;
    }
    return qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
}

}  // namespace

class VideoWindowTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void main_window_video_section_has_no_player();
    void video_window_geometry_follows_swift();
    void video_window_is_its_own_full_screen();
    void opening_reuses_one_window_and_passes_the_request();
    void opening_from_beginning_passes_the_flag();
    void closing_unloads_and_refreshes_the_library();
    void closing_resets_the_player_and_reopening_shows_only_the_new_clip();
    void empty_window_shows_the_no_video_copy();
    void opening_a_video_suspends_library_thumbnails_and_closing_resumes();
};

void VideoWindowTest::initTestCase() {
    g_previous_handler = qInstallMessageHandler(collect_qml_messages);
}

// B1: the Video section is the library; no player lives in the main window.
void VideoWindowTest::main_window_video_section_has_no_player() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    make_fixture(root.path());

    QQmlApplicationEngine engine;
    auto* window = load_main_window(engine);
    QVERIFY2(window != nullptr, "MainWindow was not created");
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->setProperty("sectionIndex", 2);
    QTRY_VERIFY(window->findChild<QObject*>(QStringLiteral("videoLibraryModel")) != nullptr);

    QVERIFY2(item_named(window->contentItem(), QStringLiteral("videoPlayerPage")) == nullptr,
             "the main window still hosts the player");
    QVERIFY2(window->findChild<QQuickWindow*>(QStringLiteral("videoWindow")) != nullptr,
             "the Video window is not declared");

    const QString warnings = warnings_after(QStringLiteral("MainWindow"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B3: title, default size scaled to the screen, centring, minimum and no
// maximum (VideoWindowGeometry, defaultVideoWindowFrame, makeWindow).
void VideoWindowTest::video_window_geometry_follows_swift() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    make_fixture(root.path());

    QQmlApplicationEngine engine;
    auto* window = load_main_window(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto* video = window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    QVERIFY2(video != nullptr, "the Video window is not declared");

    QCOMPARE(video->title(), QStringLiteral("Video"));
    QCOMPARE(video->minimumWidth(), 285);
    QCOMPARE(video->minimumHeight(), 120);
    QCOMPARE(video->maximumWidth(), 16777215);
    QCOMPARE(video->maximumHeight(), 16777215);

    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    const double scale =
        std::min({1.0, double(available.width()) / 1132.0, double(available.height()) / 637.0});
    QVERIFY2(std::abs(video->width() - 1132.0 * scale) <= 1.0,
             qPrintable(
                 QStringLiteral("width %1, expected %2").arg(video->width()).arg(1132.0 * scale)));
    QVERIFY2(std::abs(video->height() - 637.0 * scale) <= 1.0,
             qPrintable(
                 QStringLiteral("height %1, expected %2").arg(video->height()).arg(637.0 * scale)));

    QVERIFY(QMetaObject::invokeMethod(video, "show"));
    QVERIFY(QTest::qWaitForWindowExposed(video));
    const QPointF screenCenter = QRectF(available).center();
    // Allow the offscreen frame origin offset and rounding to whole pixels.
    constexpr double centerTolerance = 2.5;
    QTRY_VERIFY2(
        std::abs(video->position().x() + video->width() / 2.0 - screenCenter.x()) <=
            centerTolerance,
        qPrintable(QStringLiteral("window is not centred: x %1").arg(video->position().x())));
    QTRY_VERIFY2(
        std::abs(video->position().y() + video->height() / 2.0 - screenCenter.y()) <=
            centerTolerance,
        qPrintable(QStringLiteral("window is not centred: y %1").arg(video->position().y())));

    // The closed window keeps Swift's minimum. Opening the Inspector grows a
    // shorter window to the Linux reachability minimum; closing restores only
    // the minimum, not the user's previous height.
    auto* player = video->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"));
    QVERIFY(player != nullptr);
    video->setHeight(120);
    QTRY_COMPARE(video->height(), 120);
    QVERIFY(QMetaObject::invokeMethod(player, "toggleInspector"));
    QTRY_COMPARE(video->minimumHeight(), 360);
    QTRY_COMPARE(video->height(), 360);
    QVERIFY(QMetaObject::invokeMethod(player, "toggleInspector"));
    QTRY_COMPARE(video->minimumHeight(), 120);
    QCOMPARE(video->height(), 360);
}

// B2: a from-beginning open carries the flag through the window request.
void VideoWindowTest::opening_from_beginning_passes_the_flag() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQmlApplicationEngine engine;
    auto* window = load_main_window(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->setProperty("sectionIndex", 2);
    QQuickItem* card = nullptr;
    QTRY_VERIFY((card = card_with_id(window->contentItem(), fixture.alpha)) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(card, "contextMenuRequested",
                                      Q_ARG(QPointF, card->boundingRect().center())));
    QObject* menu = nullptr;
    QTRY_VERIFY((menu = window->findChild<QObject*>(QStringLiteral("videoContextMenu"))) !=
                nullptr);
    QTRY_VERIFY(menu->property("visible").toBool());
    QQuickItem* play_from_beginning = nullptr;
    QVERIFY(QMetaObject::invokeMethod(
        menu, "itemAt", Q_RETURN_ARG(QQuickItem*, play_from_beginning), Q_ARG(int, 0)));
    QVERIFY(play_from_beginning != nullptr);
    QCOMPARE(play_from_beginning->property("text").toString(),
             QStringLiteral("Play from Beginning"));
    QVERIFY(QMetaObject::invokeMethod(play_from_beginning, "triggered"));

    auto* video = window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    QVERIFY(video != nullptr);
    QTRY_VERIFY(video->isVisible());
    QQuickItem* player = nullptr;
    QTRY_VERIFY((player = video->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"))) !=
                nullptr);
    QCOMPARE(player->property("startFromBeginning").toBool(), true);
    QTRY_COMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(fixture.alpha));
}

// B4, B8: the page's full-screen toggle drives the Video window and leaves
// the main window alone.
void VideoWindowTest::video_window_is_its_own_full_screen() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQmlApplicationEngine engine;
    auto* window = load_main_window(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->setProperty("sectionIndex", 2);
    QQuickItem* card = nullptr;
    QTRY_VERIFY((card = card_with_id(window->contentItem(), fixture.alpha)) != nullptr);
    auto* video = window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    QVERIFY(video != nullptr);
    click(card);
    QTRY_VERIFY(video->isVisible());
    QQuickItem* player = nullptr;
    QTRY_VERIFY((player = video->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"))) !=
                nullptr);

    QCOMPARE(player->window(), video);
    QCOMPARE(int(window->visibility()), int(QWindow::Windowed));
    QVERIFY(QMetaObject::invokeMethod(player, "toggleFullScreen"));
    QTRY_COMPARE(int(video->visibility()), int(QWindow::FullScreen));
    QCOMPARE(int(window->visibility()), int(QWindow::Windowed));
    QVERIFY(QMetaObject::invokeMethod(player, "toggleFullScreen"));
    QTRY_COMPARE(int(video->visibility()), int(QWindow::Windowed));
    QCOMPARE(int(window->visibility()), int(QWindow::Windowed));
}

// B2, B5: an open shows the window with the path, subtitle and flag; a second
// open replaces the media in the same window.
void VideoWindowTest::opening_reuses_one_window_and_passes_the_request() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQmlApplicationEngine engine;
    auto* window = load_main_window(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->setProperty("sectionIndex", 2);
    QQuickItem* card = nullptr;
    QTRY_VERIFY((card = card_with_id(window->contentItem(), fixture.alpha)) != nullptr);

    auto* video = window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    QVERIFY(video != nullptr);
    click(card);
    QTRY_VERIFY(video->isVisible());

    QQuickItem* player = nullptr;
    QTRY_VERIFY((player = video->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"))) !=
                nullptr);
    QCOMPARE(player->property("initialVideoUrl").toUrl(), QUrl::fromLocalFile(fixture.alpha));
    QCOMPARE(player->property("initialSubtitleUrl").toUrl(), QUrl::fromLocalFile(fixture.subtitle));
    QCOMPARE(player->property("startFromBeginning").toBool(), false);
    QTRY_COMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(fixture.alpha));
    QTRY_COMPARE(player->property("pendingSubtitleUrl").toUrl(),
                 QUrl::fromLocalFile(fixture.subtitle));

    QQuickItem* second = nullptr;
    QTRY_VERIFY((second = card_with_id(window->contentItem(), fixture.beta)) != nullptr);
    click(second);
    QTRY_COMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(fixture.beta));
    QTRY_VERIFY(player->property("pendingSubtitleUrl").toUrl().toString().isEmpty());
    QCOMPARE(window->findChildren<QQuickWindow*>(QStringLiteral("videoWindow")).size(), 1);
    QCOMPARE(video->findChildren<QQuickItem*>(QStringLiteral("videoPlayerPage")).size(), 1);
}

// B6: closing hides the window, unloads the player and refreshes the library's
// playback history while the Video section keeps its mode.
void VideoWindowTest::closing_unloads_and_refreshes_the_library() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQmlApplicationEngine engine;
    auto* window = load_main_window(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->setProperty("sectionIndex", 2);
    QObject* model = window->findChild<QObject*>(QStringLiteral("videoLibraryModel"));
    QVERIFY(model != nullptr);
    QVERIFY(model->setProperty("displayMode", QStringLiteral("all")));
    QVERIFY(model->setProperty("sortOption", QStringLiteral("title")));
    QQuickItem* card = nullptr;
    QTRY_VERIFY((card = card_with_id(window->contentItem(), fixture.alpha)) != nullptr);
    const QString alpha_id = card->property("rowID").toString();
    auto* video = window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    QVERIFY(video != nullptr);
    click(card);
    QTRY_VERIFY(video->isVisible());

    QQuickItem* player = nullptr;
    QTRY_VERIFY((player = video->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"))) !=
                nullptr);
    QVERIFY(QMetaObject::invokeMethod(player, "toggleFullScreen"));
    QTRY_COMPARE(int(video->visibility()), int(QWindow::FullScreen));
    QVERIFY(model->setProperty("layoutMode", QStringLiteral("list")));
    niratan::player::PlaybackHistory history(niratan::storage::data_root() /
                                             "video_playback_history.json");
    history.save(fixture.alpha, 50, 100);

    QVERIFY(QMetaObject::invokeMethod(video, "close"));
    QTRY_VERIFY(!video->isVisible());
    QTRY_VERIFY(!(video->windowStates() & Qt::WindowFullScreen));
    QCOMPARE(model->property("displayMode").toString(), QStringLiteral("all"));
    QCOMPARE(model->property("sortOption").toString(), QStringLiteral("title"));
    QCOMPARE(model->property("layoutMode").toString(), QStringLiteral("list"));
    QCOMPARE(model->property("selectedItemID").toString(), alpha_id);
    QTRY_VERIFY(player->property("videoFile").toUrl().toString().isEmpty());
    QTRY_VERIFY(visual_texts(window->contentItem()).contains(QStringLiteral("0:50 left")));
    QVERIFY2(window->isVisible(), "closing the Video window hid the main window");
    QVERIFY(window->findChild<QObject*>(QStringLiteral("videoLibraryModel")) != nullptr);
}

// B6: closing resets every per-video state; reopening shows only the new
// clip in the same window.
void VideoWindowTest::closing_resets_the_player_and_reopening_shows_only_the_new_clip() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQmlApplicationEngine engine;
    auto* window = load_main_window(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->setProperty("sectionIndex", 2);

    QQuickItem* alpha = nullptr;
    QTRY_VERIFY((alpha = card_with_id(window->contentItem(), fixture.alpha)) != nullptr);
    auto* video = window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    QVERIFY(video != nullptr);
    click(alpha);
    QTRY_VERIFY(video->isVisible());

    QQuickItem* player = nullptr;
    QTRY_VERIFY((player = video->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"))) !=
                nullptr);
    QTRY_COMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(fixture.alpha));
    QCOMPARE(player->property("pendingSubtitleUrl").toUrl(), QUrl::fromLocalFile(fixture.subtitle));

    // Per-video and transient state that must not survive the close.
    player->setProperty("studyOpen", true);
    player->setProperty("speedPanelOpen", true);
    player->setProperty("osdTitle", QStringLiteral("alpha title"));
    player->setProperty("osdValue", QStringLiteral("alpha value"));
    QObject* error_dialog = nullptr;
    for (QObject* candidate : player->findChildren<QObject*>()) {
        if (candidate->metaObject()->indexOfMethod("showError(QString)") >= 0) {
            error_dialog = candidate;
            break;
        }
    }
    QVERIFY2(error_dialog != nullptr, "the player has no error alert");
    QVERIFY(QMetaObject::invokeMethod(error_dialog, "showError",
                                      Q_ARG(QString, QStringLiteral("alpha failure"))));
    QTRY_VERIFY(error_dialog->property("visible").toBool());

    QVERIFY(QMetaObject::invokeMethod(video, "close"));
    QTRY_VERIFY(!video->isVisible());
    QTRY_VERIFY(player->property("videoFile").toUrl().toString().isEmpty());
    QVERIFY(player->property("pendingSubtitleUrl").toUrl().toString().isEmpty());
    QCOMPARE(player->property("studyOpen").toBool(), false);
    QCOMPARE(player->property("speedPanelOpen").toBool(), false);
    QVERIFY(player->property("osdTitle").toString().isEmpty());
    QVERIFY(player->property("osdValue").toString().isEmpty());
    QTRY_VERIFY(!error_dialog->property("visible").toBool());

    QQuickItem* beta = nullptr;
    QTRY_VERIFY((beta = card_with_id(window->contentItem(), fixture.beta)) != nullptr);
    click(beta);
    QTRY_VERIFY(video->isVisible());
    QCOMPARE(window->findChildren<QQuickWindow*>(QStringLiteral("videoWindow")).size(), 1);
    QTRY_COMPARE(video->findChildren<QQuickItem*>(QStringLiteral("videoPlayerPage")).size(), 1);
    QQuickItem* reopened = video->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"));
    QVERIFY(reopened != nullptr);
    QTRY_COMPARE(reopened->property("videoFile").toUrl(), QUrl::fromLocalFile(fixture.beta));
    QVERIFY(reopened->property("pendingSubtitleUrl").toUrl().toString().isEmpty());
    QCOMPARE(reopened->property("studyOpen").toBool(), false);
    QCOMPARE(reopened->property("speedPanelOpen").toBool(), false);
    QVERIFY(reopened->property("osdTitle").toString().isEmpty());
    QVERIFY(reopened->property("osdValue").toString().isEmpty());

    int visible_windows = 0;
    for (QWindow* top : QGuiApplication::topLevelWindows()) {
        if (top->objectName() == QStringLiteral("videoWindow") && top->isVisible()) {
            ++visible_windows;
        }
    }
    QCOMPARE(visible_windows, 1);
}

// B7: with no video the window draws the Swift empty copy and its action.
void VideoWindowTest::empty_window_shows_the_no_video_copy() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/shell/VideoWindow.qml")));
    auto* video = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
    QVERIFY(video != nullptr);
    QCOMPARE(video->objectName(), QStringLiteral("videoWindow"));
    QVERIFY(QMetaObject::invokeMethod(video, "show"));
    QVERIFY(QTest::qWaitForWindowExposed(video));

    const QStringList texts = visual_texts(video->contentItem());
    QVERIFY2(texts.contains(QStringLiteral("No Video Open")),
             qPrintable(texts.join(QStringLiteral(" | "))));
    QVERIFY2(texts.contains(QStringLiteral("Open a local video file to start watching.")),
             "the empty description is missing");
    QQuickItem* empty = item_named(video->contentItem(), QStringLiteral("playerEmptyState"));
    QVERIFY2(empty != nullptr, "the empty state is missing");
    QVERIFY(empty->isVisible());
    QQuickItem* loading = item_named(video->contentItem(), QStringLiteral("loadingOverlay"));
    QVERIFY2(loading != nullptr, "the loading overlay is missing");
    QVERIFY2(!loading->isVisible(), "the loading overlay shows without an open");
    QVERIFY2(!texts.contains(QStringLiteral("Video Settings")),
             "the Swift empty view has no Video Settings control");
    QVERIFY2(std::none_of(texts.cbegin(), texts.cend(),
                          [](const QString& text) {
                              return text.startsWith(QStringLiteral("Supported files:"));
                          }),
             "the Swift empty view has no supported-file list");
    QQuickItem* open_button = item_named(video->contentItem(), QStringLiteral("openVideoButton"));
    QVERIFY2(open_button != nullptr, "the Open Video action is missing");
    QVERIFY(QMetaObject::invokeMethod(open_button, "clicked"));
    QObject* picker = video->findChild<QObject*>(QStringLiteral("openVideoDialog"));
    QVERIFY2(picker != nullptr, "the Open Video picker is missing");
    picker->setProperty("selectedFile", QUrl::fromLocalFile(fixture.beta));
    QVERIFY(QMetaObject::invokeMethod(picker, "accepted"));
    QQuickItem* opened = nullptr;
    QTRY_VERIFY((opened = video->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"))) !=
                nullptr);
    QTRY_COMPARE(opened->property("videoFile").toUrl(), QUrl::fromLocalFile(fixture.beta));
    QTRY_VERIFY(!empty->isVisible());
}

// B6, B7: a Video session suspends the library's poster generation from the
// open request until the window closes, and the main window sees the flag.
void VideoWindowTest::opening_a_video_suspends_library_thumbnails_and_closing_resumes() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQmlApplicationEngine engine;
    auto* window = load_main_window(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->setProperty("sectionIndex", 2);
    QObject* model = window->findChild<QObject*>(QStringLiteral("videoLibraryModel"));
    QVERIFY(model != nullptr);
    QVERIFY(model->setProperty("displayMode", QStringLiteral("all")));
    QVERIFY(!model->property("thumbnailsSuspended").toBool());

    QQuickItem* card = nullptr;
    QTRY_VERIFY((card = card_with_id(window->contentItem(), fixture.alpha)) != nullptr);
    auto* video = window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    QVERIFY(video != nullptr);
    click(card);
    QTRY_VERIFY(video->isVisible());
    QQuickItem* player = nullptr;
    QTRY_VERIFY((player = video->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"))) !=
                nullptr);
    QTRY_COMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(fixture.alpha));
    QTRY_VERIFY(player->property("videoSessionActive").toBool());
    QTRY_VERIFY(video->property("videoSessionActive").toBool());
    QTRY_VERIFY(model->property("thumbnailsSuspended").toBool());

    QVERIFY(QMetaObject::invokeMethod(video, "close"));
    QTRY_VERIFY(!video->isVisible());
    QTRY_VERIFY(!player->property("videoSessionActive").toBool());
    QTRY_VERIFY(!video->property("videoSessionActive").toBool());
    QTRY_VERIFY(!model->property("thumbnailsSuspended").toBool());
    QVERIFY(window->isVisible());

    const QString warnings = warnings_after(QStringLiteral("Video session"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
    engine.clearComponentCache();
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    VideoWindowTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_video_window.moc"

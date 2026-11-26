// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video Inspector panel, following Features/Video/VideoInspectorView.swift
// (`VideoInspectorTab`, `header`, `VideoInspectorTabBar`, `episodesTab`,
// `videoTab`, `audioTab`, `subtitlesTab`, `trackSection`,
// `subtitleSourcesSection`, `subtitleTimingSection`,
// `subtitleAppearanceSection`), Features/Video/VideoPlayerScreen.swift
// (`inspectorOverlay`, `selectedInspectorTab`, `toggleInspector`,
// `shouldShowPlaybackChrome`, `dismissVideoOverlaysFromCanvas`),
// Features/Video/VideoControlsView.swift (`inspectorButton`,
// `utilityControlGroup`, `condensedControlGroup`, `minimalControlGroup`),
// Features/Video/Playback/PlaybackEngine.swift (`VideoAspectRatio`,
// `VideoSubtitleTiming`, `VideoPlaybackSpeed`) and
// Features/Video/Subtitles/VideoSubtitleEdgeStyle.swift.
//
// Every interaction starts at the control the user clicks: the bar's
// Inspector button, the panel's tab strip and rows. Tracks are injected
// through the Inspector model because mpv cannot report tracks without media.

#include "../viewmodels/data_root.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QWheelEvent>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

#include <algorithm>
#include <clocale>
#include <memory>

Q_IMPORT_QML_PLUGIN(NiratanPlugin)
Q_IMPORT_QML_PLUGIN(NiratanStylePlugin)

namespace {

QQuickItem* item_named(QQuickItem* root, const QString& name) {
    if (root == nullptr) {
        return nullptr;
    }
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = item_named(child, name)) {
            return found;
        }
    }
    return nullptr;
}

void collect_named(QQuickItem* item, const QString& name, QList<QQuickItem*>& found) {
    if (item->objectName() == name) {
        found.append(item);
    }
    for (QQuickItem* child : item->childItems()) {
        collect_named(child, name, found);
    }
}

QList<QQuickItem*> named_items(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> found;
    if (root != nullptr) {
        collect_named(root, name, found);
    }
    return found;
}

bool shown(QQuickItem* root, const QString& name) {
    QQuickItem* item = item_named(root, name);
    return item != nullptr && item->isVisible();
}

void click(QQuickItem* item) {
    QVERIFY(item != nullptr);
    QVERIFY2(QMetaObject::invokeMethod(item, "clicked"), qPrintable(item->objectName()));
}

QString touch_file(const QString& path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write("x");
    file.close();
    return QFileInfo(path).absoluteFilePath();
}

QString make_video(const QString& folder, const QString& name) {
    return touch_file(QDir(folder).filePath(name));
}

QQuickItem* open_player(QQmlApplicationEngine& engine, const QString& video,
                        QQuickWindow** video_window) {
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/shell/VideoWindow.qml")));
    if (engine.rootObjects().isEmpty()) {
        return nullptr;
    }
    *video_window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (*video_window == nullptr) {
        return nullptr;
    }
    if (!QMetaObject::invokeMethod(
            *video_window, "openVideo", Q_ARG(QVariant, QVariant(QUrl::fromLocalFile(video))),
            Q_ARG(QVariant, QVariant(QUrl())), Q_ARG(QVariant, QVariant(false)))) {
        return nullptr;
    }
    if (!QTest::qWaitForWindowExposed(*video_window)) {
        return nullptr;
    }
    return (*video_window)->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"));
}

QQuickItem* bar_of(QQuickWindow* window) {
    return item_named(window->contentItem(), QStringLiteral("videoControlBar"));
}

QQuickItem* panel_of(QQuickWindow* window) {
    return item_named(window->contentItem(), QStringLiteral("videoInspectorPanel"));
}

QObject* inspector_model(QQuickItem* panel) {
    return qvariant_cast<QObject*>(panel->property("model"));
}

QObject* engine_of(QQuickItem* panel) {
    return qvariant_cast<QObject*>(panel->property("engine"));
}

bool panel_visible(QQuickItem* panel) {
    return panel != nullptr && panel->property("visible").toBool();
}

bool show_inspector(QQuickWindow* window) {
    QQuickItem* bar = bar_of(window);
    QQuickItem* panel = panel_of(window);
    if (bar == nullptr || panel == nullptr) {
        return false;
    }
    click(item_named(bar, QStringLiteral("inspectorButton")));
    return panel_visible(panel);
}

QQuickItem* tab_item(QQuickItem* panel, const QString& tab) {
    for (QQuickItem* item : named_items(panel, QStringLiteral("inspectorTab"))) {
        if (item->property("tab").toString() == tab) {
            return item;
        }
    }
    return nullptr;
}

QVariantMap make_track(const QString& type, int id, const QString& title, const QString& language,
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

void inject_tracks(QQuickItem* panel) {
    QObject* model = inspector_model(panel);
    QVERIFY(model != nullptr);
    const QVariantList tracks{
        make_track(QStringLiteral("video"), 1, QStringLiteral("Main"), QString(),
                   QStringLiteral("h264"), true),
        make_track(QStringLiteral("audio"), 2, QStringLiteral("Japanese"), QStringLiteral("ja"),
                   QStringLiteral("aac"), true),
        make_track(QStringLiteral("sub"), 4, QStringLiteral("Full"), QStringLiteral("ja"),
                   QStringLiteral("ass"), true),
        make_track(QStringLiteral("sub"), 5, QStringLiteral("External"), QStringLiteral("en"),
                   QStringLiteral("ass"), false, QStringLiteral("/tmp/external.ass")),
    };
    QVERIFY(QMetaObject::invokeMethod(model, "setTracks", Q_ARG(QVariantList, tracks)));
}

QQuickItem* choice_row(QQuickItem* panel, const QString& type, int id) {
    for (QQuickItem* item : named_items(panel, QStringLiteral("inspectorChoiceRow"))) {
        if (item->property("rowType").toString() == type && item->property("rowId").toInt() == id) {
            return item;
        }
    }
    return nullptr;
}

QObject* popup_titled(QObject* root, const QString& title) {
    const QList<QObject*> children = root->findChildren<QObject*>();
    for (QObject* child : children) {
        if (child->property("title").toString() == title) {
            return child;
        }
    }
    return nullptr;
}

QJsonObject stored_settings(const QString& data_root) {
    QFile file(data_root + QStringLiteral("/data/niratan/settings.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

}  // namespace

class VideoInspectorTest : public QObject {
    Q_OBJECT

   private slots:
    void inspector_button_toggles_the_panel();
    void tabs_list_four_and_swap_content();
    void header_shows_title_caption_and_closes();
    void episodes_list_and_selection();
    void video_tab_speed();
    void video_tab_picture_and_loop();
    void video_tab_enhancement_and_equalizer();
    void audio_tab_selection_and_timing();
    void subtitles_tab_tracks_and_timing();
    void subtitle_file_dialog_loads();
    void subtitle_appearance_writes_keys();
    void transcript_button_opens_the_sidebar();
    void transcript_row_opens_on_the_first_mouse_click();
    void panel_fits_the_video_window();
};

void VideoInspectorTest::inspector_button_toggles_the_panel() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");

    QQuickItem* bar = bar_of(window);
    QQuickItem* panel = panel_of(window);
    QVERIFY2(bar != nullptr, "the player does not host VideoControlBar");
    QVERIFY2(panel != nullptr, "the player does not host the Inspector panel");
    QCOMPARE(engine_of(panel),
             static_cast<QObject*>(item_named(player, QStringLiteral("mpvItem"))));
    QCOMPARE(inspector_model(panel), player->findChild<QObject*>(QStringLiteral("inspectorModel")));
    QVERIFY2(!panel_visible(panel), "the Inspector shows before it is opened");

    QQuickItem* button = item_named(bar, QStringLiteral("inspectorButton"));
    QVERIFY2(button != nullptr, "the Inspector button is missing");
    QVERIFY2(item_named(button, QStringLiteral("inspectorGlyph"))->property("mirrored").toBool(),
             "the Inspector glyph is not mirrored");
    click(button);
    QTRY_VERIFY(panel_visible(panel));
    QTRY_VERIFY(button->property("active").toBool());
    QCOMPARE(panel->property("selectedTab").toInt(), 3);

    click(button);
    QTRY_VERIFY(!panel_visible(panel));
    QTRY_VERIFY(!button->property("active").toBool());

    bar->setProperty("availableWidth", 712.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("full"));
    QVERIFY2(item_named(bar, QStringLiteral("inspectorButton")) != nullptr,
             "the Inspector button is missing at full density");
    bar->setProperty("availableWidth", 500.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("condensed"));
    QVERIFY2(item_named(bar, QStringLiteral("inspectorButton")) != nullptr,
             "the Inspector button is missing at condensed density");
    bar->setProperty("availableWidth", 421.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("minimal"));
    QVERIFY2(!shown(bar, QStringLiteral("inspectorButton")),
             "the Inspector button stays at minimal density");
}

void VideoInspectorTest::tabs_list_four_and_swap_content() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);

    const QList<QQuickItem*> tabs = named_items(panel, QStringLiteral("inspectorTab"));
    QCOMPARE(tabs.size(), 4);
    QCOMPARE(tabs.at(0)->property("tab").toString(), QStringLiteral("episodes"));
    QCOMPARE(tabs.at(1)->property("tab").toString(), QStringLiteral("video"));
    QCOMPARE(tabs.at(2)->property("tab").toString(), QStringLiteral("audio"));
    QCOMPARE(tabs.at(3)->property("tab").toString(), QStringLiteral("subtitles"));
    QVERIFY(tabs.at(3)->property("selected").toBool());
    const QStringList titles{QStringLiteral("Episodes"), QStringLiteral("Video"),
                             QStringLiteral("Audio"), QStringLiteral("Subtitles")};
    const QStringList icons{QStringLiteral("list.number"), QStringLiteral("film"),
                            QStringLiteral("waveform"), QStringLiteral("captions.bubble")};
    for (int i = 0; i < tabs.size(); ++i) {
        QCOMPARE(tabs.at(i)->property("title").toString(), titles.at(i));
        QCOMPARE(tabs.at(i)->property("icon").toString(), icons.at(i));
    }

    click(tab_item(panel, QStringLiteral("video")));
    QTRY_COMPARE(panel->property("selectedTab").toInt(), 1);
    QVERIFY(shown(panel, QStringLiteral("inspectorSpeedSlider")));
    QVERIFY(!shown(panel, QStringLiteral("inspectorSubtitleSlider")));

    click(tab_item(panel, QStringLiteral("audio")));
    QTRY_COMPARE(panel->property("selectedTab").toInt(), 2);
    QVERIFY(shown(panel, QStringLiteral("inspectorAudioValue")));

    click(tab_item(panel, QStringLiteral("episodes")));
    QTRY_COMPARE(panel->property("selectedTab").toInt(), 0);
    QVERIFY(panel_visible(panel));
}

void VideoInspectorTest::header_shows_title_caption_and_closes() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString folder = root.path() + QStringLiteral("/Anime");
    make_video(folder, QStringLiteral("Show 1.mkv"));
    const QString video = make_video(folder, QStringLiteral("Show 2.mkv"));
    make_video(folder, QStringLiteral("Show 3.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);

    QCOMPARE(item_named(panel, QStringLiteral("inspectorTitle"))->property("text").toString(),
             QStringLiteral("Show 2"));
    QCOMPARE(item_named(panel, QStringLiteral("inspectorCaption"))->property("text").toString(),
             QStringLiteral("Now Playing · 2 / 3"));

    click(item_named(panel, QStringLiteral("inspectorCloseButton")));
    QTRY_VERIFY(!panel_visible(panel));
    QVERIFY2(panel_of(window)->property("visible").toBool() == false, "the panel stayed open");
    QTRY_VERIFY(player->hasActiveFocus());
}

void VideoInspectorTest::episodes_list_and_selection() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString folder = root.path() + QStringLiteral("/Anime");
    make_video(folder, QStringLiteral("Show 1.mkv"));
    const QString video = make_video(folder, QStringLiteral("Show 2.mkv"));
    make_video(folder, QStringLiteral("Show 10.mkv"));
    touch_file(QDir(folder).filePath(QStringLiteral("notes.txt")));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);
    click(tab_item(panel, QStringLiteral("episodes")));

    const QList<QQuickItem*> rows = named_items(panel, QStringLiteral("inspectorEpisodeRow"));
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0)->property("number").toInt(), 1);
    QCOMPARE(QFileInfo(rows.at(0)->property("episodeUrl").toUrl().toLocalFile()).fileName(),
             QStringLiteral("Show 1.mkv"));
    QCOMPARE(rows.at(1)->property("isCurrent").toBool(), true);
    QCOMPARE(QFileInfo(rows.at(2)->property("episodeUrl").toUrl().toLocalFile()).fileName(),
             QStringLiteral("Show 10.mkv"));

    click(rows.at(2));
    QTRY_COMPARE(player->property("videoFile").toUrl(),
                 QUrl::fromLocalFile(QDir(folder).filePath(QStringLiteral("Show 10.mkv"))));
}

void VideoInspectorTest::video_tab_speed() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);
    click(tab_item(panel, QStringLiteral("video")));

    QQuickItem* preset = nullptr;
    for (QQuickItem* item : named_items(panel, QStringLiteral("inspectorSpeedPreset"))) {
        if (item->property("speedValue").toDouble() == 2.0) {
            preset = item;
        }
    }
    QVERIFY2(preset != nullptr, "the 2x preset is missing");
    QSignalSpy speed_spy(panel, SIGNAL(speedRequested(double)));
    click(preset);
    QObject* model = inspector_model(panel);
    QCOMPARE(model->property("speedLabel").toString(), QStringLiteral("2x"));
    QCOMPARE(speed_spy.count(), 1);
    QCOMPARE(speed_spy.at(0).at(0).toDouble(), 2.0);

    QQuickItem* slider = item_named(panel, QStringLiteral("inspectorSpeedSlider"));
    QVERIFY2(slider != nullptr, "the speed slider is missing");
    slider->setProperty("value", 1.5);
    QVERIFY(QMetaObject::invokeMethod(slider, "moved"));
    QTRY_COMPARE(model->property("speedLabel").toString(), QStringLiteral("1.5x"));
    QCOMPARE(speed_spy.count(), 2);
    QCOMPARE(speed_spy.at(1).at(0).toDouble(), 1.5);
    QCOMPARE(slider->property("from").toDouble(), 0.3);
    QCOMPARE(slider->property("to").toDouble(), 5.0);
    QCOMPARE(slider->property("stepSize").toDouble(), 0.1);
}

void VideoInspectorTest::video_tab_picture_and_loop() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);
    QObject* mpv = engine_of(panel);
    QVERIFY(mpv != nullptr);
    click(tab_item(panel, QStringLiteral("video")));

    QQuickItem* aspect = item_named(panel, QStringLiteral("inspectorAspectMenu"));
    QVERIFY2(aspect != nullptr, "the aspect ratio menu is missing");
    struct AspectCase {
        int index;
        const char* value;
    };
    for (const AspectCase& entry :
         {AspectCase{1, "16:9"}, AspectCase{2, "4:3"}, AspectCase{3, "1:1"}, AspectCase{4, "21:9"},
          AspectCase{0, "-1"}}) {
        aspect->setProperty("currentIndex", entry.index);
        QVERIFY(QMetaObject::invokeMethod(aspect, "activated", Q_ARG(int, entry.index)));
        QTRY_COMPARE(mpv->property("aspectRatio").toString(), QString::fromLatin1(entry.value));
    }

    QQuickItem* loop = item_named(panel, QStringLiteral("inspectorLoopToggle"));
    QVERIFY2(loop != nullptr, "the loop toggle is missing");
    loop->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(loop, "toggled"));
    QTRY_COMPARE(mpv->property("looping").toBool(), true);

    QVERIFY(item_named(panel, QStringLiteral("inspectorRotateButton")) != nullptr);
    QVERIFY(!item_named(panel, QStringLiteral("inspectorLoopClearButton"))
                 ->property("enabled")
                 .toBool());
}

void VideoInspectorTest::video_tab_enhancement_and_equalizer() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);
    QObject* mpv = engine_of(panel);
    click(tab_item(panel, QStringLiteral("video")));

    const QList<QPair<QString, QString>> toggles{
        {QStringLiteral("inspectorHardwareToggle"), QStringLiteral("videoHardwareDecodingEnabled")},
        {QStringLiteral("inspectorDeinterlaceToggle"), QStringLiteral("videoDeinterlacingEnabled")},
        {QStringLiteral("inspectorHdrToggle"), QStringLiteral("videoHDREnhancementEnabled")},
    };
    for (const auto& entry : toggles) {
        QQuickItem* toggle = item_named(panel, entry.first);
        QVERIFY2(toggle != nullptr, qPrintable(entry.first));
        toggle->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(toggle, "toggled"));
        QTRY_COMPARE(mpv->property("videoSettings").toMap().value(entry.second).toBool(), true);
    }
    const QJsonObject stored = stored_settings(root.path());
    QCOMPARE(stored.value(QStringLiteral("videoHardwareDecodingEnabled")).toBool(), true);
    QCOMPARE(stored.value(QStringLiteral("videoDeinterlacingEnabled")).toBool(), true);
    QCOMPARE(stored.value(QStringLiteral("videoHDREnhancementEnabled")).toBool(), true);

    QQuickItem* reset = item_named(panel, QStringLiteral("inspectorEqualizerReset"));
    QVERIFY2(reset != nullptr, "the equalizer reset is missing");
    QVERIFY(!reset->isVisible());

    QQuickItem* brightness = nullptr;
    for (QQuickItem* item : named_items(panel, QStringLiteral("inspectorEqualizer"))) {
        if (item->property("settingKey").toString() == QStringLiteral("videoBrightness")) {
            brightness = item;
        }
    }
    QVERIFY2(brightness != nullptr, "the brightness slider is missing");
    brightness->setProperty("value", 25.0);
    QVERIFY(QMetaObject::invokeMethod(brightness, "moved"));
    QTRY_COMPARE(
        mpv->property("videoSettings").toMap().value(QStringLiteral("videoBrightness")).toInt(),
        25);

    QQuickItem* contrast = nullptr;
    for (QQuickItem* item : named_items(panel, QStringLiteral("inspectorEqualizer"))) {
        if (item->property("settingKey").toString() == QStringLiteral("videoContrast")) {
            contrast = item;
        }
    }
    QVERIFY2(contrast != nullptr, "the contrast slider is missing");
    contrast->setProperty("value", -40.0);
    QVERIFY(QMetaObject::invokeMethod(contrast, "moved"));
    QTRY_COMPARE(
        mpv->property("videoSettings").toMap().value(QStringLiteral("videoContrast")).toInt(), -40);
    QTRY_VERIFY(reset->isVisible());

    click(reset);
    for (const char* key :
         {"videoBrightness", "videoContrast", "videoSaturation", "videoGamma", "videoHue"}) {
        QTRY_COMPARE(mpv->property("videoSettings").toMap().value(QString::fromLatin1(key)).toInt(),
                     0);
    }
}

void VideoInspectorTest::audio_tab_selection_and_timing() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);
    QObject* mpv = engine_of(panel);
    inject_tracks(panel);
    click(tab_item(panel, QStringLiteral("audio")));

    QQuickItem* off = choice_row(panel, QStringLiteral("audio"), 0);
    QVERIFY2(off != nullptr, "the audio Off row is missing");
    QCOMPARE(off->property("selected").toBool(), false);
    QQuickItem* row_two = choice_row(panel, QStringLiteral("audio"), 2);
    QVERIFY2(row_two != nullptr, "the audio track row is missing");
    QCOMPARE(row_two->property("title").toString(), QStringLiteral("Japanese · ja"));

    QSignalSpy track_spy(panel, SIGNAL(selectTrackRequested(QString, int)));
    click(row_two);
    QCOMPARE(track_spy.count(), 1);
    QCOMPARE(track_spy.at(0).at(0).toString(), QStringLiteral("audio"));
    QCOMPARE(track_spy.at(0).at(1).toInt(), 2);

    QQuickItem* reset = item_named(panel, QStringLiteral("inspectorAudioReset"));
    QVERIFY2(reset != nullptr, "the audio reset is missing");
    QCOMPARE(reset->property("enabled").toBool(), false);
    QCOMPARE(item_named(panel, QStringLiteral("inspectorAudioValue"))->property("text").toString(),
             QStringLiteral("+0.0 s"));

    click(item_named(panel, QStringLiteral("inspectorAudioPlus")));
    QTRY_COMPARE(mpv->property("audioDelay").toDouble(), 0.5);
    QTRY_COMPARE(
        item_named(panel, QStringLiteral("inspectorAudioValue"))->property("text").toString(),
        QStringLiteral("+0.5 s"));
    QTRY_VERIFY(reset->property("enabled").toBool());

    click(reset);
    QTRY_COMPARE(mpv->property("audioDelay").toDouble(), 0.0);
    QTRY_COMPARE(reset->property("enabled").toBool(), false);
}

void VideoInspectorTest::subtitles_tab_tracks_and_timing() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);
    QObject* mpv = engine_of(panel);
    inject_tracks(panel);

    QQuickItem* external = choice_row(panel, QStringLiteral("sub"), 5);
    QVERIFY2(external != nullptr, "the external subtitle row is missing");
    QCOMPARE(external->property("detail").toString(), QStringLiteral("External Subtitles · ass"));

    click(choice_row(panel, QStringLiteral("sub"), 0));
    QTRY_COMPARE(mpv->property("subtitlesVisible").toBool(), false);
    click(choice_row(panel, QStringLiteral("sub"), 4));
    QTRY_COMPARE(mpv->property("subtitlesVisible").toBool(), true);

    click(item_named(panel, QStringLiteral("inspectorSubtitleForward50")));
    QTRY_COMPARE(mpv->property("subtitleDelay").toDouble(), 0.05);
    QTRY_COMPARE(
        item_named(panel, QStringLiteral("inspectorSubtitleValue"))->property("text").toString(),
        QStringLiteral("+50 ms"));

    QQuickItem* offset = item_named(panel, QStringLiteral("inspectorSubtitleOffsetField"));
    QVERIFY2(offset != nullptr, "the offset field is missing");
    offset->setProperty("text", QStringLiteral("250"));
    QVERIFY(QMetaObject::invokeMethod(offset, "accepted"));
    QTRY_COMPARE(mpv->property("subtitleDelay").toDouble(), 0.25);

    QQuickItem* slider = item_named(panel, QStringLiteral("inspectorSubtitleSlider"));
    QVERIFY2(slider != nullptr, "the subtitle slider is missing");
    slider->setProperty("value", 10000.0);
    QVERIFY(QMetaObject::invokeMethod(slider, "moved"));
    QTRY_COMPARE(mpv->property("subtitleDelay").toDouble(), 10.0);
}

void VideoInspectorTest::subtitle_file_dialog_loads() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);

    QVERIFY2(item_named(panel, QStringLiteral("inspectorFindSubtitles")) == nullptr,
             "the online Find Subtitles row is drawn");
    click(item_named(panel, QStringLiteral("inspectorOpenSubtitles")));

    QObject* dialog = window->findChild<QObject*>(QStringLiteral("inspectorSubtitleFileDialog"));
    QVERIFY2(dialog != nullptr, "the subtitle file dialog is missing");
    QTRY_VERIFY(dialog->property("visible").toBool());

    const QString subtitle = touch_file(root.path() + QStringLiteral("/External.srt"));
    dialog->setProperty("selectedFile", QUrl::fromLocalFile(subtitle));
    QVERIFY(QMetaObject::invokeMethod(dialog, "accepted"));

    QObject* alert = nullptr;
    QTRY_VERIFY((alert = popup_titled(window, QStringLiteral("Video Error"))) != nullptr);
    QTRY_COMPARE(alert->property("message").toString(),
                 QStringLiteral("Could not load the subtitle file."));
}

void VideoInspectorTest::subtitle_appearance_writes_keys() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);
    QObject* mpv = engine_of(panel);

    const auto setting = [mpv](const QString& key) {
        return mpv->property("videoSettings").toMap().value(key);
    };

    QQuickItem* ass = item_named(panel, QStringLiteral("inspectorAssToggle"));
    QVERIFY2(ass != nullptr, "the ASS toggle is missing");
    ass->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(ass, "toggled"));
    QTRY_COMPARE(setting(QStringLiteral("videoRespectASSStyle")).toBool(), true);

    QQuickItem* family = item_named(panel, QStringLiteral("inspectorFontFamily"));
    QVERIFY2(family != nullptr, "the font family control is missing");
    family->setProperty("currentIndex", 1);
    QVERIFY(QMetaObject::invokeMethod(family, "activated", Q_ARG(int, 1)));
    QTRY_VERIFY(!setting(QStringLiteral("videoSubtitleFontFamily")).toString().isEmpty());

    QQuickItem* size = item_named(panel, QStringLiteral("inspectorFontSize"));
    QVERIFY2(size != nullptr, "the font size control is missing");
    size->setProperty("value", 48.0);
    QVERIFY(QMetaObject::invokeMethod(size, "moved"));
    QTRY_COMPARE(setting(QStringLiteral("videoSubtitleFontSize")).toInt(), 48);

    QQuickItem* weight = item_named(panel, QStringLiteral("inspectorFontWeight"));
    QVERIFY2(weight != nullptr, "the font weight control is missing");
    weight->setProperty("value", 800);
    QVERIFY(QMetaObject::invokeMethod(weight, "valueModified"));
    QTRY_COMPARE(setting(QStringLiteral("videoSubtitleFontWeight")).toInt(), 800);

    QQuickItem* edge = item_named(panel, QStringLiteral("inspectorEdgeStyle"));
    QVERIFY2(edge != nullptr, "the edge style control is missing");
    edge->setProperty("currentIndex", 0);
    QVERIFY(QMetaObject::invokeMethod(edge, "activated", Q_ARG(int, 0)));
    QTRY_COMPARE(setting(QStringLiteral("videoSubtitleEdgeStyle")).toString(),
                 QStringLiteral("off"));
    QQuickItem* strength = item_named(panel, QStringLiteral("inspectorEdgeStrength"));
    QVERIFY2(strength != nullptr, "the edge strength control is missing");
    QTRY_COMPARE(strength->property("enabled").toBool(), false);

    edge->setProperty("currentIndex", 3);
    QVERIFY(QMetaObject::invokeMethod(edge, "activated", Q_ARG(int, 3)));
    QTRY_COMPARE(setting(QStringLiteral("videoSubtitleEdgeStyle")).toString(),
                 QStringLiteral("highContrast"));
    QTRY_VERIFY(strength->property("enabled").toBool());
    strength->setProperty("value", 0.5);
    QVERIFY(QMetaObject::invokeMethod(strength, "moved"));
    QTRY_COMPARE(setting(QStringLiteral("videoSubtitleEdgeStrength")).toDouble(), 0.5);

    QQuickItem* no_background = item_named(panel, QStringLiteral("inspectorNoBackground"));
    QVERIFY2(no_background != nullptr, "the background toggle is missing");
    no_background->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(no_background, "toggled"));
    QTRY_COMPARE(setting(QStringLiteral("videoSubtitleBackgroundDisabled")).toBool(), true);
    QQuickItem* opacity = item_named(panel, QStringLiteral("inspectorBackgroundOpacity"));
    QVERIFY2(opacity != nullptr, "the background opacity control is missing");
    QTRY_COMPARE(opacity->property("enabled").toBool(), false);

    QQuickItem* position = item_named(panel, QStringLiteral("inspectorVerticalPosition"));
    QVERIFY2(position != nullptr, "the vertical position control is missing");
    position->setProperty("value", 0.5);
    QVERIFY(QMetaObject::invokeMethod(position, "moved"));
    QTRY_COMPARE(setting(QStringLiteral("videoSubtitleVerticalPositionFraction")).toDouble(), 0.5);

    click(item_named(panel, QStringLiteral("inspectorSubtitleColor")));
    QObject* picker = nullptr;
    QTRY_VERIFY((picker = popup_titled(window, QStringLiteral("Subtitle Color"))) != nullptr);
    picker->setProperty("selectedColor", QColor::fromRgb(0x11, 0x22, 0x33, 0x44));
    QVERIFY(QMetaObject::invokeMethod(picker, "accepted"));
    QTRY_COMPARE(setting(QStringLiteral("videoSubtitleColor")).toString(),
                 QStringLiteral("#11223344"));
    QCOMPARE(stored_settings(root.path()).value(QStringLiteral("videoSubtitleColor")).toString(),
             QStringLiteral("#11223344"));

    struct ColorCase {
        const char* control;
        const char* dialog;
        const char* key;
        int r;
        int g;
        int b;
        int a;
    };
    for (const ColorCase& entry :
         {ColorCase{"inspectorLookupHighlightColor", "Lookup Highlight Color",
                    "videoSubtitleLookupHighlightColor", 0x55, 0x66, 0x77, 0x88},
          ColorCase{"inspectorLookupHighlightTextColor", "Lookup Highlight Text Color",
                    "videoSubtitleLookupHighlightTextColor", 0x99, 0xAA, 0xBB, 0xCC}}) {
        click(item_named(panel, QString::fromLatin1(entry.control)));
        QObject* color_picker = nullptr;
        QTRY_VERIFY((color_picker = popup_titled(window, QString::fromLatin1(entry.dialog))) !=
                    nullptr);
        color_picker->setProperty("selectedColor",
                                  QColor::fromRgb(entry.r, entry.g, entry.b, entry.a));
        QVERIFY(QMetaObject::invokeMethod(color_picker, "accepted"));
        const QString expected = QStringLiteral("#%1%2%3%4")
                                     .arg(entry.r, 2, 16, QLatin1Char('0'))
                                     .arg(entry.g, 2, 16, QLatin1Char('0'))
                                     .arg(entry.b, 2, 16, QLatin1Char('0'))
                                     .arg(entry.a, 2, 16, QLatin1Char('0'))
                                     .toUpper();
        QTRY_COMPARE(setting(QString::fromLatin1(entry.key)).toString(), expected);
        QCOMPARE(stored_settings(root.path()).value(QString::fromLatin1(entry.key)).toString(),
                 expected);
    }

    no_background->setProperty("checked", false);
    QVERIFY(QMetaObject::invokeMethod(no_background, "toggled"));
    QTRY_COMPARE(setting(QStringLiteral("videoSubtitleBackgroundDisabled")).toBool(), false);
    opacity->setProperty("value", 0.4);
    QVERIFY(QMetaObject::invokeMethod(opacity, "moved"));
    QTRY_COMPARE(setting(QStringLiteral("videoSubtitleBackgroundOpacity")).toDouble(), 0.4);

    click(item_named(panel, QStringLiteral("inspectorRestoreDefaults")));
    QTRY_VERIFY(setting(QStringLiteral("videoSubtitleColor")).toString() !=
                QStringLiteral("#11223344"));
    QCOMPARE(stored_settings(root.path()).value(QStringLiteral("videoSubtitleColor")).toString(),
             setting(QStringLiteral("videoSubtitleColor")).toString());
}

void VideoInspectorTest::transcript_button_opens_the_sidebar() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);

    QVERIFY2(item_named(panel, QStringLiteral("inspectorOpenTranscript")) != nullptr,
             "the transcript row is missing");
    click(item_named(panel, QStringLiteral("inspectorOpenTranscript")));
    QTRY_COMPARE(player->property("studyOpen").toBool(), true);
    QTRY_COMPARE(item_named(window->contentItem(), QStringLiteral("studySidebar"))
                     ->property("selectedTab")
                     .toInt(),
                 1);
    QTRY_VERIFY(!panel_visible(panel));
}

// The owner's report: the first real click on Open Transcript did nothing.
// Drive it with the mouse, not the clicked signal, after scrolling the row
// into view as a user must.
void VideoInspectorTest::transcript_row_opens_on_the_first_mouse_click() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);
    QQuickItem* row = item_named(panel, QStringLiteral("inspectorOpenTranscript"));
    QVERIFY2(row != nullptr, "the transcript row is missing");

    QQuickItem* flick = row->parentItem();
    while (flick != nullptr && !flick->inherits("QQuickFlickable")) {
        flick = flick->parentItem();
    }
    QVERIFY2(flick != nullptr, "the transcript row is not in a scroll area");
    // Scroll with the wheel, as the owner does, until the row is in view.
    const QPointF over = flick->mapToScene(QPointF(flick->width() / 2, flick->height() / 2));
    ulong stamp = 100000;
    const auto wheel_by = [&](int delta) {
        QWheelEvent wheel(over, window->mapToGlobal(over), QPoint(), QPoint(0, delta), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        wheel.setTimestamp(stamp += 120);
        QCoreApplication::sendEvent(window, &wheel);
    };
    for (int i = 0; i < 30 && !flick->property("atYEnd").toBool(); ++i) {
        wheel_by(-120);
        QTest::qWait(120);
    }
    QTRY_VERIFY(flick->property("atYEnd").toBool() && !flick->property("moving").toBool());

    const QPointF centre = row->mapToScene(QPointF(row->width() / 2, row->height() / 2));
    QVERIFY2(QRectF(QPointF(0, 0), window->size()).contains(centre),
             "the transcript row is outside the window");
    // A real pointer arrives over the row first; then one more wheel tick
    // and a click before the scroll it starts has settled.
    QTest::mouseMove(window, (centre - QPointF(40, 0)).toPoint());
    QTest::mouseMove(window, centre.toPoint());
    wheel_by(120);
    QVERIFY2(flick->property("moving").toBool(), "the wheel tick did not start a scroll");
    QTest::mouseClick(window, Qt::LeftButton, {}, centre.toPoint());
    QTRY_COMPARE(player->property("studyOpen").toBool(), true);
    QCOMPARE(item_named(window->contentItem(), QStringLiteral("studySidebar"))
                 ->property("selectedTab")
                 .toInt(),
             1);
}

void VideoInspectorTest::panel_fits_the_video_window() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path(), QStringLiteral("Alpha.mkv"));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = open_player(engine, video, &window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    window->setWidth(960);
    window->setHeight(640);
    QVERIFY2(show_inspector(window), "the Inspector did not open");
    QQuickItem* panel = panel_of(window);
    QTRY_VERIFY(panel->width() >= 300 && panel->width() <= 400);
    QTRY_VERIFY(panel->height() > 0);
    QQuickItem* bar = bar_of(window);
    QVERIFY(bar != nullptr);
    QTRY_VERIFY(bar->height() > 0);
    QVERIFY(panel->mapRectToScene(panel->boundingRect()).bottom() <=
            bar->mapRectToScene(bar->boundingRect()).top());
    const QRectF rect = panel->mapRectToScene(panel->boundingRect());
    const QRectF window_rect(0, 0, window->width(), window->height());
    QVERIFY2(window_rect.contains(rect), "the Inspector does not fit the video window");
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    VideoInspectorTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_video_inspector.moc"

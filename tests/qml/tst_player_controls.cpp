// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video control bar, following Features/Video/VideoControlsView.swift
// (`VideoControlsMetrics`, `metrics(for:)`, `chromeSize(for:availableWidth:)`,
// `ControlDensity`, `controlDensity`, `body`, `floatingControls`,
// `compactBottomControls`, `primaryControlGroup`,
// `responsiveCompactControlGroup`, `condensedControlGroup`,
// `minimalControlGroup`, `progressControlStrip`, `episodeControls`,
// `volumeControl`, `volumeSymbolName`, `remainingTimeText`, `compactTimeText`,
// `speedControlPanel`, `setSpeed`, `commitSpeedInput`, `speedLabel`,
// `miningHistoryButton`, `openVideoButton`, `mineCurrentSubtitleButton`,
// `fullScreenButton`), Features/Video/VideoPlayerScreen.swift `videoSurface`
// and `shouldShowPlaybackChrome`, Features/Video/VideoPlayerScreen+Chrome.swift
// (`revealPlaybackChrome`, `schedulePlaybackChromeAutoHide`), and
// Features/Video/Playback/PlaybackEngine.swift (`VideoTimeFormatter`,
// `VideoPlaybackSpeed`).
//
// The tests cover the density thresholds, group membership at each density,
// the time and speed labels, the volume glyph, the transport and utility
// signals, the speed panel entry rules, the chrome fade and hit testing, and
// the page wiring. They assert presence, order, visibility and wiring only,
// never a pixel geometry value (Decision). The component interfaces the tests
// rely on are documented in each QML file's header: `density`, `interactive`,
// `trackWidth`, the time and speed texts, the button object names and `tip`
// strings, and the speed panel's `presets`, `speedValue`, `selected`,
// `inputText` and `commitInput`.
//
// The page tests open a clip through VideoWindow.openVideo; playback does not
// start, so the bar is not visible, but its controls still route their signals.

#include "../viewmodels/data_root.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMetaObject>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

#include <clocale>
#include <cmath>
#include <functional>
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

void collect_texts(QQuickItem* item, QStringList& texts) {
    const QVariant text = item->property("text");
    if (text.isValid() && !text.toString().isEmpty()) {
        texts.append(text.toString());
    }
    for (QQuickItem* child : item->childItems()) {
        collect_texts(child, texts);
    }
}

QStringList visual_texts(QQuickItem* root) {
    QStringList texts;
    if (root != nullptr) {
        collect_texts(root, texts);
    }
    return texts;
}

void collect_named(QQuickItem* item, const QStringList& known, QStringList& found) {
    const QString name = item->objectName();
    if (known.contains(name)) {
        found.append(name);
    }
    for (QQuickItem* child : item->childItems()) {
        collect_named(child, known, found);
    }
}

QStringList named_in_order(QQuickItem* root, const QStringList& known) {
    QStringList found;
    if (root != nullptr) {
        collect_named(root, known, found);
    }
    return found;
}

bool shown(QQuickItem* root, const QString& name) {
    QQuickItem* item = item_named(root, name);
    return item != nullptr && item->isVisible();
}

void click(QQuickItem* item) {
    QVERIFY(item != nullptr);
    QVERIFY(QMetaObject::invokeMethod(item, "clicked"));
}

std::unique_ptr<QObject> create_player_component(QQmlApplicationEngine& engine, const QString& name,
                                                 const QVariantMap& properties, QString& error) {
    const QUrl url(QStringLiteral("qrc:/qt/qml/Niratan/src/views/player/%1.qml").arg(name));
    QQmlComponent component(&engine, url);
    if (component.isError()) {
        for (const QQmlError& entry : component.errors()) {
            error += entry.toString() + QLatin1Char('\n');
        }
        return {};
    }
    std::unique_ptr<QObject> object(component.createWithInitialProperties(properties));
    if (!object) {
        for (const QQmlError& entry : component.errors()) {
            error += entry.toString() + QLatin1Char('\n');
        }
    }
    return object;
}

QQuickItem* attach(QQuickWindow& window, QObject* object) {
    auto* item = qobject_cast<QQuickItem*>(object);
    if (item == nullptr) {
        return nullptr;
    }
    item->setParentItem(window.contentItem());
    window.resize(1000, 400);
    window.show();
    return item;
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

QString make_video(const QString& base) {
    const QString source = base + QStringLiteral("/Anime");
    QDir().mkpath(source);
    return touch_file(source + QStringLiteral("/Alpha.mp4"), 10);
}

}  // namespace

class PlayerControlsTest : public QObject {
    Q_OBJECT

   private slots:
    void floating_density_follows_the_bar_width();
    void compact_density_follows_the_stage_width();
    void floating_groups_follow_the_density();
    void compact_groups_follow_the_density();
    void progress_strip_labels_follow_swift();
    void volume_symbol_follows_swift();
    void playback_glyphs_and_transport_signals();
    void utility_group_order_and_signals();
    void speed_panel_presets_and_entry();
    void speed_pill_follows_the_speed_label();
    void chrome_visibility_and_hit_testing();
    void page_hosts_the_bar_and_wires_the_actions();
    void legacy_controls_are_gone();
    void bar_glyphs_keep_their_size();
};

namespace {

struct BarCase {
    double availableWidth;
    const char* density;
};

const QStringList kUtilityButtons{
    QStringLiteral("miningHistoryButton"), QStringLiteral("openVideoButton"),
    QStringLiteral("mineSubtitleButton"),  QStringLiteral("inspectorButton"),
    QStringLiteral("fullScreenButton"),
};

const QStringList kFullDensityButtons{
    QStringLiteral("miningHistoryButton"), QStringLiteral("openVideoButton"),
    QStringLiteral("mineSubtitleButton"),  QStringLiteral("inspectorButton"),
    QStringLiteral("fullScreenButton"),    QStringLiteral("volumeButton"),
    QStringLiteral("previousButton"),      QStringLiteral("nextButton"),
    QStringLiteral("playButton"),          QStringLiteral("speedButton"),
};

void verify_density_cases(QQmlApplicationEngine& engine, const QString& layout,
                          const QList<BarCase>& cases) {
    for (const BarCase& entry : cases) {
        QString error;
        std::unique_ptr<QObject> object =
            create_player_component(engine, QStringLiteral("VideoControlBar"),
                                    {{QStringLiteral("layoutMode"), layout},
                                     {QStringLiteral("availableWidth"), entry.availableWidth}},
                                    error);
        QVERIFY2(object != nullptr, qPrintable(error));
        auto* bar = qobject_cast<QQuickItem*>(object.get());
        QVERIFY(bar != nullptr);
        QTRY_COMPARE(bar->property("density").toString(), QString::fromLatin1(entry.density));
    }
}

}  // namespace

// B6: density follows the active bar width, not the stage width.
void PlayerControlsTest::floating_density_follows_the_bar_width() {
    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    // The floating bar is the stage width minus its 32 side insets, so the
    // thresholds on 680 and 390 apply to the shrunken width.
    verify_density_cases(engine, QStringLiteral("floating"),
                         {{712, "full"},
                          {711, "condensed"},
                          {422, "condensed"},
                          {421, "minimal"},
                          {285, "minimal"}});
}

// B6: the compact bar is the stage width, so 720 and 390 apply directly.
void PlayerControlsTest::compact_density_follows_the_stage_width() {
    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    verify_density_cases(engine, QStringLiteral("compactBottom"),
                         {{720, "full"},
                          {719, "condensed"},
                          {390, "condensed"},
                          {389, "minimal"},
                          {285, "minimal"}});
}

// B3, B4, B6: the floating bar's groups at each density.
void PlayerControlsTest::floating_groups_follow_the_density() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object =
        create_player_component(engine, QStringLiteral("VideoControlBar"),
                                {{QStringLiteral("layoutMode"), QStringLiteral("floating")},
                                 {QStringLiteral("availableWidth"), 712.0},
                                 {QStringLiteral("videoFileSet"), true},
                                 {QStringLiteral("chromeShown"), true}},
                                error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* bar = attach(window, object.get());
    QVERIFY(bar != nullptr);

    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("full"));
    QVERIFY(shown(bar, QStringLiteral("videoProgressStrip")));
    QVERIFY2(
        named_in_order(bar,
                       {QStringLiteral("volumeButton"), QStringLiteral("previousButton"),
                        QStringLiteral("playButton"), QStringLiteral("nextButton"),
                        QStringLiteral("speedButton"), QStringLiteral("miningHistoryButton"),
                        QStringLiteral("openVideoButton"), QStringLiteral("mineSubtitleButton"),
                        QStringLiteral("inspectorButton"), QStringLiteral("fullScreenButton")}) ==
            QStringList({QStringLiteral("volumeButton"), QStringLiteral("previousButton"),
                         QStringLiteral("playButton"), QStringLiteral("nextButton"),
                         QStringLiteral("speedButton"), QStringLiteral("miningHistoryButton"),
                         QStringLiteral("openVideoButton"), QStringLiteral("mineSubtitleButton"),
                         QStringLiteral("inspectorButton"), QStringLiteral("fullScreenButton")}),
        "the lower row clusters are out of order");
    for (const QString& name : kFullDensityButtons) {
        QVERIFY2(shown(bar, name), qPrintable(name + QStringLiteral(" is missing at full")));
    }
    for (const QString& name :
         {QStringLiteral("screenshotButton"), QStringLiteral("cycleSubtitlesButton"),
          QStringLiteral("videoSettingsButton"), QStringLiteral("gapFastForwardButton")}) {
        QVERIFY2(item_named(bar, name) == nullptr, qPrintable(name + QStringLiteral(" is built")));
    }

    bar->setProperty("availableWidth", 500.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("condensed"));
    for (const QString& name :
         {QStringLiteral("previousButton"), QStringLiteral("playButton"),
          QStringLiteral("nextButton"), QStringLiteral("speedButton"),
          QStringLiteral("openVideoButton"), QStringLiteral("inspectorButton"),
          QStringLiteral("fullScreenButton")}) {
        QVERIFY2(shown(bar, name), qPrintable(name + QStringLiteral(" is missing condensed")));
    }
    for (const QString& name :
         {QStringLiteral("volumeButton"), QStringLiteral("miningHistoryButton"),
          QStringLiteral("mineSubtitleButton")}) {
        QVERIFY2(!shown(bar, name), qPrintable(name + QStringLiteral(" stays condensed")));
    }

    QSignalSpy close_spy(bar, SIGNAL(speedPanelCloseRequested()));
    bar->setProperty("speedPanelOpen", true);
    bar->setProperty("availableWidth", 421.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("minimal"));
    QTRY_COMPARE(close_spy.count(), 1);
    for (const QString& name : {QStringLiteral("previousButton"), QStringLiteral("playButton"),
                                QStringLiteral("nextButton"), QStringLiteral("fullScreenButton")}) {
        QVERIFY2(shown(bar, name), qPrintable(name + QStringLiteral(" is missing minimal")));
    }
    for (const QString& name :
         {QStringLiteral("speedButton"), QStringLiteral("openVideoButton"),
          QStringLiteral("volumeButton"), QStringLiteral("miningHistoryButton"),
          QStringLiteral("mineSubtitleButton"), QStringLiteral("inspectorButton")}) {
        QVERIFY2(!shown(bar, name), qPrintable(name + QStringLiteral(" stays minimal")));
    }
}

// B5: the compact bar's full row order and its condensed and minimal groups.
void PlayerControlsTest::compact_groups_follow_the_density() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object =
        create_player_component(engine, QStringLiteral("VideoControlBar"),
                                {{QStringLiteral("layoutMode"), QStringLiteral("compactBottom")},
                                 {QStringLiteral("availableWidth"), 720.0},
                                 {QStringLiteral("videoFileSet"), true},
                                 {QStringLiteral("chromeShown"), true}},
                                error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* bar = attach(window, object.get());
    QVERIFY(bar != nullptr);

    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("full"));
    const QStringList compact_row{
        QStringLiteral("previousButton"),      QStringLiteral("playButton"),
        QStringLiteral("nextButton"),          QStringLiteral("volumeButton"),
        QStringLiteral("compactTime"),         QStringLiteral("speedButton"),
        QStringLiteral("miningHistoryButton"), QStringLiteral("openVideoButton"),
        QStringLiteral("mineSubtitleButton"),  QStringLiteral("inspectorButton"),
        QStringLiteral("fullScreenButton"),
    };
    QVERIFY2(named_in_order(bar, compact_row) == compact_row,
             "the compact full row is out of order");
    for (const QString& name : kFullDensityButtons) {
        QVERIFY2(shown(bar, name), qPrintable(name + QStringLiteral(" is missing compact full")));
    }
    QCOMPARE(bar->property("compactTimeText").toString(), QStringLiteral("0:00 / 0:00"));
    bar->setProperty("duration", 65.0);
    bar->setProperty("currentTime", 65.0);
    QTRY_COMPARE(bar->property("compactTimeText").toString(), QStringLiteral("1:05 / 1:05"));

    bar->setProperty("availableWidth", 719.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("condensed"));
    for (const QString& name :
         {QStringLiteral("previousButton"), QStringLiteral("playButton"),
          QStringLiteral("nextButton"), QStringLiteral("speedButton"),
          QStringLiteral("openVideoButton"), QStringLiteral("inspectorButton"),
          QStringLiteral("fullScreenButton")}) {
        QVERIFY2(shown(bar, name),
                 qPrintable(name + QStringLiteral(" is missing compact condensed")));
    }
    QVERIFY2(!shown(bar, QStringLiteral("volumeButton")),
             "the volume track stays in the compact full row");

    bar->setProperty("availableWidth", 389.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("minimal"));
    for (const QString& name : {QStringLiteral("previousButton"), QStringLiteral("playButton"),
                                QStringLiteral("nextButton"), QStringLiteral("fullScreenButton")}) {
        QVERIFY2(shown(bar, name),
                 qPrintable(name + QStringLiteral(" is missing compact minimal")));
    }
    for (const QString& name : {QStringLiteral("speedButton"), QStringLiteral("inspectorButton")}) {
        QVERIFY2(!shown(bar, name), qPrintable(name + QStringLiteral(" stays in compact minimal")));
    }
}

// B2: VideoTimeFormatter.string and remainingTimeText.
void PlayerControlsTest::progress_strip_labels_follow_swift() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object = create_player_component(
        engine, QStringLiteral("VideoProgressStrip"),
        {{QStringLiteral("currentTime"), 65.9}, {QStringLiteral("duration"), 0.0}}, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* strip = attach(window, object.get());
    QVERIFY(strip != nullptr);

    strip->setWidth(400);
    strip->setHeight(18);
    QCOMPARE(strip->property("currentText").toString(), QStringLiteral("1:05"));
    QCOMPARE(strip->property("remainingText").toString(), QStringLiteral("-0:00"));
    QCOMPARE(named_in_order(strip,
                            {QStringLiteral("currentTimeLabel"), QStringLiteral("timeline"),
                             QStringLiteral("remainingTimeLabel")}),
             QStringList({QStringLiteral("currentTimeLabel"), QStringLiteral("timeline"),
                          QStringLiteral("remainingTimeLabel")}));

    strip->setProperty("duration", 3725.0);
    QTRY_COMPARE(strip->property("remainingText").toString(), QStringLiteral("-1:00:59"));
    strip->setProperty("currentTime", 65.0);
    QTRY_COMPARE(strip->property("remainingText").toString(), QStringLiteral("-1:01:00"));
    QTRY_COMPARE(strip->property("currentText").toString(), QStringLiteral("1:05"));

    // The scrub time drives both labels, so the remaining label follows it.
    strip->setProperty("scrubbing", true);
    strip->setProperty("scrubTime", 100.0);
    QTRY_COMPARE(strip->property("currentText").toString(), QStringLiteral("1:40"));
    QTRY_COMPARE(strip->property("remainingText").toString(), QStringLiteral("-1:00:25"));

    // Chapters, the scrub bubble and the hover knob live on the timeline; a
    // click seeks only on release.
    strip->setProperty("duration", 100.0);
    strip->setProperty("scrubTime", 50.0);
    strip->setProperty("chapters",
                       QVariantList{QVariantMap{{QStringLiteral("id"), 1},
                                                {QStringLiteral("start"), 25.0},
                                                {QStringLiteral("duration"), 0.0}}});
    QQuickItem* timeline = item_named(strip, QStringLiteral("timeline"));
    QVERIFY2(timeline != nullptr, "the timeline is missing");
    QTRY_VERIFY(item_named(strip, QStringLiteral("chapterMarker")) != nullptr);
    QCOMPARE(named_in_order(strip, {QStringLiteral("chapterMarker")}).size(), 1);
    QQuickItem* bubble = item_named(strip, QStringLiteral("scrubBubble"));
    QVERIFY2(bubble != nullptr, "the scrub bubble is missing");
    QVERIFY(bubble->isVisible());

    QQuickItem* knob = item_named(strip, QStringLiteral("hoverKnob"));
    QVERIFY2(knob != nullptr, "the hover knob is missing");
    const QPoint centre =
        timeline->mapToScene(QPointF(timeline->width() / 2.0, timeline->height() / 2.0)).toPoint();
    QTest::mouseMove(&window, centre);
    QTRY_VERIFY(knob->isVisible());

    QSignalSpy seek_spy(strip, SIGNAL(seekRequested(double)));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centre);
    QTRY_COMPARE(seek_spy.count(), 1);
    QVERIFY(std::abs(seek_spy.first().first().toDouble() - 50.0) <= 1.0);
}

// B3: volumeSymbolName.
void PlayerControlsTest::volume_symbol_follows_swift() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object =
        create_player_component(engine, QStringLiteral("VideoTransportControls"),
                                {{QStringLiteral("volume"), 50.0}}, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* transport = attach(window, object.get());
    QVERIFY(transport != nullptr);

    transport->setProperty("isMuted", true);
    QTRY_COMPARE(transport->property("volumeSymbol").toString(),
                 QStringLiteral("speaker.slash.fill"));
    transport->setProperty("isMuted", false);
    transport->setProperty("volume", 0.0);
    QTRY_COMPARE(transport->property("volumeSymbol").toString(),
                 QStringLiteral("speaker.slash.fill"));
    transport->setProperty("volume", 33.0);
    QTRY_COMPARE(transport->property("volumeSymbol").toString(),
                 QStringLiteral("speaker.wave.1.fill"));
    transport->setProperty("volume", 34.0);
    QTRY_COMPARE(transport->property("volumeSymbol").toString(),
                 QStringLiteral("speaker.wave.2.fill"));
    transport->setProperty("volume", 66.0);
    QTRY_COMPARE(transport->property("volumeSymbol").toString(),
                 QStringLiteral("speaker.wave.2.fill"));
    transport->setProperty("volume", 67.0);
    QTRY_COMPARE(transport->property("volumeSymbol").toString(),
                 QStringLiteral("speaker.wave.3.fill"));
}

// B3: the play glyph and the transport intents; no playlist, so previous and
// next stay disabled.
void PlayerControlsTest::playback_glyphs_and_transport_signals() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object =
        create_player_component(engine, QStringLiteral("VideoTransportControls"),
                                {{QStringLiteral("isPlaying"), true}}, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* transport = attach(window, object.get());
    QVERIFY(transport != nullptr);

    QQuickItem* play_icon = item_named(transport, QStringLiteral("playIcon"));
    QVERIFY2(play_icon != nullptr, "the play glyph is missing");
    QTRY_COMPARE(play_icon->property("name").toString(), QStringLiteral("pause.fill"));
    QQuickItem* play_button = item_named(transport, QStringLiteral("playButton"));
    QVERIFY(play_button != nullptr);
    QTRY_COMPARE(play_button->property("tip").toString(), QStringLiteral("Pause"));

    QSignalSpy toggle_spy(transport, SIGNAL(togglePlaybackRequested()));
    click(play_button);
    QCOMPARE(toggle_spy.count(), 1);

    transport->setProperty("isPlaying", false);
    QTRY_COMPARE(play_icon->property("name").toString(), QStringLiteral("play.fill"));
    QTRY_COMPARE(play_button->property("tip").toString(), QStringLiteral("Play"));

    QQuickItem* previous = item_named(transport, QStringLiteral("previousButton"));
    QQuickItem* next = item_named(transport, QStringLiteral("nextButton"));
    QVERIFY(previous != nullptr);
    QVERIFY(next != nullptr);
    QVERIFY(!previous->property("enabled").toBool());
    QVERIFY(!next->property("enabled").toBool());

    QSignalSpy mute_spy(transport, SIGNAL(toggleMuteRequested()));
    click(item_named(transport, QStringLiteral("volumeButton")));
    QCOMPARE(mute_spy.count(), 1);

    QQuickItem* track = item_named(transport, QStringLiteral("volumeTrack"));
    QVERIFY2(track != nullptr, "the volume track is missing");
    QSignalSpy volume_spy(transport, SIGNAL(volumeChangeRequested(double)));
    track->setProperty("value", 42.0);
    QVERIFY(QMetaObject::invokeMethod(track, "moved"));
    QTRY_COMPARE(volume_spy.count(), 1);
    QCOMPARE(volume_spy.first().first().toDouble(), 42.0);
}

// B4: the utility group's order, active states and intents.
void PlayerControlsTest::utility_group_order_and_signals() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object =
        create_player_component(engine, QStringLiteral("VideoUtilityControls"),
                                {{QStringLiteral("density"), QStringLiteral("full")},
                                 {QStringLiteral("studyOpen"), true},
                                 {QStringLiteral("canMine"), true}},
                                error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* utility = attach(window, object.get());
    QVERIFY(utility != nullptr);

    QCOMPARE(named_in_order(utility, kUtilityButtons), kUtilityButtons);
    QVERIFY(QMetaObject::invokeMethod(item_named(utility, QStringLiteral("miningHistoryButton")),
                                      "clicked"));
    QVERIFY(
        item_named(utility, QStringLiteral("miningHistoryButton"))->property("active").toBool());
    QVERIFY(
        item_named(utility, QStringLiteral("mineSubtitleButton"))->property("enabled").toBool());
    utility->setProperty("studyOpen", false);
    QTRY_VERIFY(
        !item_named(utility, QStringLiteral("miningHistoryButton"))->property("active").toBool());
    utility->setProperty("studyOpen", true);
    QTRY_VERIFY(
        item_named(utility, QStringLiteral("miningHistoryButton"))->property("active").toBool());

    QSignalSpy study_spy(utility, SIGNAL(toggleStudyRequested()));
    QSignalSpy open_spy(utility, SIGNAL(openVideoRequested()));
    QSignalSpy mine_spy(utility, SIGNAL(mineSubtitleRequested()));
    QSignalSpy inspector_spy(utility, SIGNAL(toggleInspectorRequested()));
    QSignalSpy full_screen_spy(utility, SIGNAL(toggleFullScreenRequested()));

    click(item_named(utility, QStringLiteral("miningHistoryButton")));
    click(item_named(utility, QStringLiteral("openVideoButton")));
    click(item_named(utility, QStringLiteral("mineSubtitleButton")));
    click(item_named(utility, QStringLiteral("inspectorButton")));
    QCOMPARE(study_spy.count(), 1);
    QCOMPARE(open_spy.count(), 1);
    QCOMPARE(mine_spy.count(), 1);
    QCOMPARE(inspector_spy.count(), 1);

    utility->setProperty("inspectorOpen", true);
    QTRY_VERIFY(
        item_named(utility, QStringLiteral("inspectorButton"))->property("active").toBool());
    utility->setProperty("inspectorOpen", false);
    QTRY_VERIFY(
        !item_named(utility, QStringLiteral("inspectorButton"))->property("active").toBool());

    QQuickItem* full_screen = item_named(utility, QStringLiteral("fullScreenButton"));
    QVERIFY(full_screen != nullptr);
    QCOMPARE(full_screen->property("tip").toString(), QStringLiteral("Toggle Full Screen"));
    QQuickItem* full_screen_icon = item_named(full_screen, QStringLiteral("fullScreenIcon"));
    QVERIFY2(full_screen_icon != nullptr, "the full-screen glyph is missing");
    QTRY_COMPARE(full_screen_icon->property("name").toString(), QStringLiteral("arrows-out"));
    utility->setProperty("isFullScreen", true);
    QTRY_COMPARE(full_screen_icon->property("name").toString(), QStringLiteral("arrows-in"));

    click(full_screen);
    QCOMPARE(full_screen_spy.count(), 1);

    utility->setProperty("canMine", false);
    QTRY_VERIFY(
        !item_named(utility, QStringLiteral("mineSubtitleButton"))->property("enabled").toBool());
}

// B8: presets, the label and commitSpeedInput.
void PlayerControlsTest::speed_panel_presets_and_entry() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object = create_player_component(
        engine, QStringLiteral("VideoSpeedPanel"), {{QStringLiteral("speed"), 1.5}}, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* panel = attach(window, object.get());
    QVERIFY(panel != nullptr);

    QTRY_COMPARE(panel->property("currentLabel").toString(), QStringLiteral("1.5"));
    QStringList presets;
    QStringList texts;
    std::function<void(QQuickItem*)> collect_presets = [&](QQuickItem* item) {
        if (item->objectName() == QStringLiteral("speedPreset")) {
            presets.append(QString::number(item->property("speedValue").toDouble()));
            texts.append(item->property("text").toString());
        }
        for (QQuickItem* child : item->childItems()) {
            collect_presets(child);
        }
    };
    collect_presets(panel);
    QCOMPARE(presets,
             QStringList({QStringLiteral("0.25"), QStringLiteral("0.5"), QStringLiteral("1"),
                          QStringLiteral("1.5"), QStringLiteral("2"), QStringLiteral("3"),
                          QStringLiteral("4"), QStringLiteral("5")}));
    QCOMPARE(texts,
             QStringList({QStringLiteral("0.25x"), QStringLiteral("0.5x"), QStringLiteral("1x"),
                          QStringLiteral("1.5x"), QStringLiteral("2x"), QStringLiteral("3x"),
                          QStringLiteral("4x"), QStringLiteral("5x")}));
    QQuickItem* selected = nullptr;
    std::function<void(QQuickItem*)> find_selected = [&](QQuickItem* item) {
        if (item->objectName() == QStringLiteral("speedPreset") &&
            item->property("selected").toBool()) {
            selected = item;
        }
        for (QQuickItem* child : item->childItems()) {
            find_selected(child);
        }
    };
    find_selected(panel);
    QVERIFY2(selected != nullptr, "the current speed is not selected");
    QCOMPARE(selected->property("speedValue").toDouble(), 1.5);

    QQuickItem* slider = item_named(panel, QStringLiteral("speedSlider"));
    QVERIFY2(slider != nullptr, "the speed slider is missing");
    QCOMPARE(slider->property("from").toDouble(), 0.3);
    QCOMPARE(slider->property("to").toDouble(), 5.0);
    QCOMPARE(slider->property("stepSize").toDouble(), 0.1);

    QSignalSpy speed_spy(panel, SIGNAL(speedChangeRequested(double)));
    QQuickItem* preset_two = nullptr;
    std::function<void(QQuickItem*)> find_preset = [&](QQuickItem* item) {
        if (item->objectName() == QStringLiteral("speedPreset") &&
            item->property("speedValue").toDouble() == 2.0) {
            preset_two = item;
        }
        for (QQuickItem* child : item->childItems()) {
            find_preset(child);
        }
    };
    find_preset(panel);
    QVERIFY2(preset_two != nullptr, "the 2x preset is missing");
    QVERIFY(QMetaObject::invokeMethod(preset_two, "clicked"));
    QTRY_COMPARE(speed_spy.count(), 1);
    QCOMPARE(speed_spy.first().first().toDouble(), 2.0);
    QCOMPARE(panel->property("inputText").toString(), QStringLiteral("2"));
    panel->setProperty("inputText", QStringLiteral(" 2,4 "));
    QVERIFY(QMetaObject::invokeMethod(panel, "commitInput"));
    QTRY_COMPARE(speed_spy.count(), 2);
    QCOMPARE(speed_spy.at(1).first().toDouble(), 2.4);
    QCOMPARE(panel->property("inputText").toString(), QStringLiteral("2.4"));

    panel->setProperty("inputText", QStringLiteral("0.2"));
    QVERIFY(QMetaObject::invokeMethod(panel, "commitInput"));
    QTRY_COMPARE(speed_spy.count(), 3);
    QCOMPARE(speed_spy.at(2).first().toDouble(), 0.25);

    panel->setProperty("inputText", QStringLiteral("6"));
    QVERIFY(QMetaObject::invokeMethod(panel, "commitInput"));
    QTRY_COMPARE(speed_spy.count(), 4);
    QCOMPARE(speed_spy.at(3).first().toDouble(), 5.0);

    // A non-number resets the field to the current label and sets nothing.
    panel->setProperty("inputText", QStringLiteral("fast"));
    QVERIFY(QMetaObject::invokeMethod(panel, "commitInput"));
    QTest::qWait(50);
    QCOMPARE(speed_spy.count(), 4);
    QCOMPARE(panel->property("inputText").toString(), QStringLiteral("1.5"));
}

// B8: the pill shows VideoPlaybackSpeed.label plus "x".
void PlayerControlsTest::speed_pill_follows_the_speed_label() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object =
        create_player_component(engine, QStringLiteral("VideoControlBar"),
                                {{QStringLiteral("availableWidth"), 712.0},
                                 {QStringLiteral("videoFileSet"), true},
                                 {QStringLiteral("chromeShown"), true},
                                 {QStringLiteral("speed"), 1.26}},
                                error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* bar = attach(window, object.get());
    QVERIFY(bar != nullptr);

    QQuickItem* pill = item_named(bar, QStringLiteral("speedButton"));
    QVERIFY2(pill != nullptr, "the speed pill is missing");
    QTRY_COMPARE(pill->property("text").toString(), QStringLiteral("1.3x"));
    bar->setProperty("speed", 0.25);
    QTRY_COMPARE(pill->property("text").toString(), QStringLiteral("0.25x"));
    bar->setProperty("speed", 2.0);
    QTRY_COMPARE(pill->property("text").toString(), QStringLiteral("2x"));
}

// B1: drawn only with a clip and the chrome rule, faded over 180 ms, and no
// pointer input while hidden.
void PlayerControlsTest::chrome_visibility_and_hit_testing() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object =
        create_player_component(engine, QStringLiteral("VideoControlBar"),
                                {{QStringLiteral("availableWidth"), 712.0}}, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* bar = attach(window, object.get());
    QVERIFY(bar != nullptr);

    QVERIFY2(!bar->isVisible(), "the bar shows without a clip");
    bar->setProperty("videoFileSet", true);
    bar->setProperty("chromeShown", true);
    QTRY_VERIFY(bar->isVisible());
    QTRY_COMPARE(bar->property("interactive").toBool(), true);

    bar->setProperty("chromeShown", false);
    QTRY_VERIFY2(!bar->isVisible(), "the bar stays visible after the chrome hides");
    QVERIFY(!bar->property("interactive").toBool());

    bar->setProperty("videoFileSet", false);
    bar->setProperty("chromeShown", true);
    QTRY_VERIFY2(!bar->isVisible(), "the bar shows with no clip open");
}

namespace {

QQuickItem* open_video_in_window(QQmlApplicationEngine& engine, const QString& video,
                                 QQuickWindow** video_window) {
    engine.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    if (engine.rootObjects().isEmpty()) {
        return nullptr;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (window == nullptr) {
        return nullptr;
    }
    if (!QTest::qWaitForWindowExposed(window)) {
        return nullptr;
    }
    *video_window = window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    if (*video_window == nullptr) {
        return nullptr;
    }
    // VideoWindowPresenter.open: the window hands the request to PlayerPage.
    if (!QMetaObject::invokeMethod(
            *video_window, "openVideo", Q_ARG(QVariant, QVariant(QUrl::fromLocalFile(video))),
            Q_ARG(QVariant, QVariant(QUrl())), Q_ARG(QVariant, QVariant(false)))) {
        return nullptr;
    }
    QQuickItem* player = nullptr;
    if (!QTest::qWaitForWindowExposed(*video_window)) {
        return nullptr;
    }
    player = (*video_window)->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"));
    return player;
}

}  // namespace

// B4, B6, B9: the page hosts the bar and every control reaches a page action.
void PlayerControlsTest::page_hosts_the_bar_and_wires_the_actions() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QTRY_COMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(video));

    QQuickItem* bar = item_named(video_window->contentItem(), QStringLiteral("videoControlBar"));
    QVERIFY2(bar != nullptr, "the player does not host VideoControlBar");
    QTRY_COMPARE(bar->property("layoutMode").toString(), QStringLiteral("floating"));
    QTRY_COMPARE(bar->property("videoFileSet").toBool(), true);
    QVERIFY2(!bar->isVisible(), "the bar shows before playback chrome is up");

    QVERIFY(item_named(bar, QStringLiteral("inspectorButton")) != nullptr);
    QVERIFY2(item_named(video_window->contentItem(), QStringLiteral("videoInspectorPanel")) !=
                 nullptr,
             "the player does not host the Inspector panel");

    QVERIFY(
        QMetaObject::invokeMethod(item_named(bar, QStringLiteral("openVideoButton")), "clicked"));
    QTRY_VERIFY(item_named(video_window->contentItem(), QStringLiteral("videoOpenMenu"))
                    ->property("visible")
                    .toBool());
    QVERIFY(QMetaObject::invokeMethod(
        item_named(video_window->contentItem(), QStringLiteral("videoOpenMenu")), "close"));

    QVERIFY(QMetaObject::invokeMethod(item_named(bar, QStringLiteral("miningHistoryButton")),
                                      "clicked"));
    QTRY_COMPARE(player->property("studyOpen").toBool(), true);

    QVERIFY(QMetaObject::invokeMethod(item_named(bar, QStringLiteral("speedButton")), "clicked"));
    QTRY_COMPARE(player->property("speedPanelOpen").toBool(), true);

    QVERIFY(
        QMetaObject::invokeMethod(item_named(bar, QStringLiteral("fullScreenButton")), "clicked"));
    QTRY_COMPARE(int(video_window->visibility()), int(QWindow::FullScreen));
    QVERIFY(
        QMetaObject::invokeMethod(item_named(bar, QStringLiteral("fullScreenButton")), "clicked"));
    QTRY_COMPARE(int(video_window->visibility()), int(QWindow::Windowed));
}

// B7: the interim stack controls are gone; the keyboard shortcuts stay.
void PlayerControlsTest::legacy_controls_are_gone() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");

    QQuickItem* bar = item_named(video_window->contentItem(), QStringLiteral("videoControlBar"));
    QVERIFY(bar != nullptr);
    const QStringList texts = visual_texts(bar);
    const QString seek_interval = QString::number(player->property("seekInterval").toInt());
    const QStringList legacy{
        QStringLiteral("\u2039"),
        QStringLiteral("\u21A4"),
        QStringLiteral("\u2212") + seek_interval,
        QStringLiteral("+") + seek_interval,
        QStringLiteral("S\u2212"),
        QStringLiteral("S+"),
        QStringLiteral("a\u2212"),
        QStringLiteral("a+"),
        QStringLiteral("A"),
        QStringLiteral("B"),
        QStringLiteral("AB"),
        QStringLiteral("\u21BB"),
        QStringLiteral("History"),
    };
    QVERIFY2(texts.contains(QStringLiteral("0:00")), "the bar shows no current time");
    QVERIFY2(texts.contains(QStringLiteral("-0:00")), "the bar shows no remaining time");
    for (const QString& text : legacy) {
        QVERIFY2(!texts.contains(text), qPrintable(QStringLiteral("%1 is still drawn").arg(text)));
    }

    // F, R and the delay and A-B keys stay with PlayerPage.handleKey, which
    // requires a loaded clip; the bar draws none of their buttons. Full screen
    // itself is covered by the utility group test.
}

// The round bar buttons reset the style's capsule inset, so each glyph keeps
// its size instead of being squeezed into what the inset leaves of the
// button, and the episode buttons share the play button's centre line, as
// VideoControlsView.episodeControls' HStack centres them.
void PlayerControlsTest::bar_glyphs_keep_their_size() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object =
        create_player_component(engine, QStringLiteral("VideoControlBar"),
                                {{QStringLiteral("layoutMode"), QStringLiteral("floating")},
                                 {QStringLiteral("availableWidth"), 712.0},
                                 {QStringLiteral("videoFileSet"), true},
                                 {QStringLiteral("chromeShown"), true}},
                                error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* bar = attach(window, object.get());
    QVERIFY(bar != nullptr);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("full"));

    for (const QString& name :
         {QStringLiteral("volumeButton"), QStringLiteral("previousButton"),
          QStringLiteral("playButton"), QStringLiteral("nextButton"),
          QStringLiteral("miningHistoryButton"), QStringLiteral("openVideoButton"),
          QStringLiteral("mineSubtitleButton"), QStringLiteral("inspectorButton"),
          QStringLiteral("fullScreenButton")}) {
        QQuickItem* button = item_named(bar, name);
        QVERIFY2(button != nullptr, qPrintable(name));
        auto* content = qvariant_cast<QQuickItem*>(button->property("contentItem"));
        QVERIFY2(content != nullptr, qPrintable(name));
        QVERIFY2(content->property("known").toBool(),
                 qPrintable(QStringLiteral("%1 names no bundled glyph").arg(name)));
        QVERIFY2(content->width() >= 14 && content->height() >= 14,
                 qPrintable(QStringLiteral("%1 glyph is %2x%3")
                                .arg(name)
                                .arg(content->width())
                                .arg(content->height())));
    }

    const auto center_y = [bar](const QString& name) {
        QQuickItem* item = item_named(bar, name);
        return item == nullptr ? -1.0 : item->mapToScene(item->boundingRect().center()).y();
    };
    const double play = center_y(QStringLiteral("playButton"));
    QVERIFY(play >= 0);
    QCOMPARE(center_y(QStringLiteral("previousButton")), play);
    QCOMPARE(center_y(QStringLiteral("nextButton")), play);

    // primaryControlGroup: the trailing tools keep their width and push the
    // episode group left rather than overlapping it.
    const auto scene_rect = [bar](const QString& name) {
        QQuickItem* item = item_named(bar, name);
        return item == nullptr ? QRectF() : item->mapRectToScene(item->boundingRect());
    };
    QVERIFY2(scene_rect(QStringLiteral("nextButton")).right() + 8 <=
                 scene_rect(QStringLiteral("speedButton")).left(),
             "the episode group overlaps the speed pill");
    QVERIFY2(scene_rect(QStringLiteral("volumeTrack")).right() <=
                 scene_rect(QStringLiteral("previousButton")).left(),
             "the episode group overlaps the volume control");

    // With room on both sides, play/pause sits at the bar's centre.
    bar->setProperty("availableWidth", 2000.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("full"));
    QTRY_VERIFY(std::abs(scene_rect(QStringLiteral("playButton")).center().x() -
                         bar->mapRectToScene(bar->boundingRect()).center().x()) <= 1.0);
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    PlayerControlsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_player_controls.moc"

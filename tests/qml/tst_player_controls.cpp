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
// Episode navigation and autoplay follow VideoPlaylist.previousURL/nextURL,
// VideoPlayerViewModel.playPrevious/playNext/openPlaylistItem,
// VideoPlayerScreen+Shortcuts.swift, VideoShortcutActions.swift
// previousEpisode/nextEpisode with Niratan-win's Ctrl+Shift mapping, and
// UserConfig.videoAutoPlayNext through the page's playbackReady seam. The
// subtitle gap fast-forward toggle follows VideoControlsView.swift
// subtitleGapFastForwardButton, VideoShortcutActions.swift
// toggleSubtitleGapFastForward and UserConfig.videoSubtitleGapFastForwardEnabled.

#include "../viewmodels/data_root.h"
#include "services/player/mining_history.h"
#include "services/player/mining_history_store.h"
#include "services/player/playback_history.h"
#include "services/player/video_settings.h"
#include "services/storage/app_paths.h"
#include "viewmodels/player/player_inspector_view_model.h"
#include "viewmodels/player/player_study_view_model.h"

#include <QClipboard>
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
#include <QtTest>

#include <clocale>
#include <cmath>
#include <functional>
#include <memory>

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

// Three episodes in one folder, in the Inspector's natural order.
QString make_episode_folder(const QString& base) {
    const QString source = base + QStringLiteral("/Episodes");
    for (const char* name : {"Show 1.mkv", "Show 2.mkv", "Show 3.mkv"}) {
        const QString path = source + QLatin1Char('/') + QString::fromLatin1(name);
        if (touch_file(path, 10).isEmpty()) {
            return {};
        }
    }
    return source;
}

QUrl episode_url(const QString& folder, const char* name) {
    return QUrl::fromLocalFile(QDir(folder).filePath(QString::fromLatin1(name)));
}

// One cue per two seconds, numbered from one, with the given text.
QByteArray numbered_cues_srt(int count, const std::function<QString(int)>& text) {
    const auto stamp = [](int seconds) {
        return QStringLiteral("00:%1:%2,000")
            .arg(seconds / 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'))
            .toUtf8();
    };
    QByteArray out;
    for (int i = 0; i < count; ++i) {
        out += QByteArray::number(i + 1) + '\n' + stamp(i * 2) + " --> " + stamp(i * 2 + 1) + '\n' +
            text(i).toUtf8() + "\n\n";
    }
    return out;
}

void collect_named_items(QQuickItem* item, const QString& name, QList<QQuickItem*>& found) {
    if (item->objectName() == name) {
        found.append(item);
    }
    for (QQuickItem* child : item->childItems()) {
        collect_named_items(child, name, found);
    }
}

QList<QQuickItem*> items_named(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> found;
    if (root != nullptr) {
        collect_named_items(root, name, found);
    }
    return found;
}

bool is_ancestor_of(QQuickItem* ancestor, QQuickItem* item) {
    for (QQuickItem* current = item; current != nullptr; current = current->parentItem()) {
        if (current == ancestor) {
            return true;
        }
    }
    return false;
}

// The study sidebar over a live model, for transcript and chapter wiring.
QQuickItem* make_study_sidebar(QQmlApplicationEngine& engine,
                               niratan::viewmodels::PlayerStudyViewModel& model, int tab,
                               QQuickWindow& window, std::unique_ptr<QObject>& holder) {
    QString error;
    holder = create_player_component(
        engine, QStringLiteral("StudySidebar"),
        {{QStringLiteral("study"),
          QVariant::fromValue<niratan::viewmodels::PlayerStudyViewModel*>(&model)},
         {QStringLiteral("preferredWidth"), 420},
         {QStringLiteral("playbackReady"), true},
         {QStringLiteral("selectedTab"), tab}},
        error);
    if (holder == nullptr) {
        return nullptr;
    }
    QQuickItem* sidebar = qobject_cast<QQuickItem*>(holder.get());
    if (sidebar == nullptr) {
        return nullptr;
    }
    sidebar->setParentItem(window.contentItem());
    sidebar->setWidth(420);
    sidebar->setHeight(700);
    window.resize(520, 760);
    window.show();
    return sidebar;
}

class KeyEventProbe : public QObject {
    Q_OBJECT
    Q_PROPERTY(int key READ key CONSTANT)
    Q_PROPERTY(int modifiers READ modifiers CONSTANT)
    Q_PROPERTY(bool isAutoRepeat READ is_auto_repeat CONSTANT)
    Q_PROPERTY(bool accepted READ accepted WRITE set_accepted)

   public:
    KeyEventProbe(int key, int modifiers, bool auto_repeat)
        : key_(key), modifiers_(modifiers), auto_repeat_(auto_repeat) {}

    int key() const { return key_; }
    int modifiers() const { return modifiers_; }
    bool is_auto_repeat() const { return auto_repeat_; }
    bool accepted() const { return accepted_; }
    void set_accepted(bool accepted) { accepted_ = accepted; }

   private:
    int key_ = 0;
    int modifiers_ = 0;
    bool auto_repeat_ = false;
    bool accepted_ = false;
};

void send_handle_key(QQuickItem* player, KeyEventProbe& event) {
    QMetaObject::invokeMethod(player, "handleKey",
                              Q_ARG(QVariant, QVariant::fromValue<QObject*>(&event)));
}

}  // namespace

// Stands in for PlayerPage's historyJumpRequested handler in sidebar tests.
class HistoryJumpRelay : public QObject {
    Q_OBJECT

   public:
    explicit HistoryJumpRelay(niratan::viewmodels::PlayerStudyViewModel& model) : model_(model) {}

   public slots:
    void jump(const QString& id) { model_.jumpToHistory(id); }

   private:
    niratan::viewmodels::PlayerStudyViewModel& model_;
};

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
    void subtitle_shortcuts_navigate_and_align();
    void subtitle_alignment_clamps_delay_and_dismisses_lookup();
    void transcript_toolbar_exists_without_search_matches();
    void episode_transport_navigation();
    void episode_open_preserves_resume_history();
    void episode_shortcuts_navigate_neighbors();
    void autoplay_consumes_setting_at_eof();
    void gap_fast_forward_toggle_from_button_and_shortcut();
    void transcript_search_caption_and_highlights();
    void transcript_search_clearing_resumes_following();
    void chapter_progress_follows_raw_playback();
    void history_search_sections_and_highlights();
    void history_jump_preserves_pause();
    void screenshot_control_is_available_at_every_density();
    void page_drop_picks_the_first_media_and_subtitle();
    void page_wheel_volume_follows_the_scroll_delta();
    void timeline_preview_capsule_follows_hover_and_linger();
    void screenshot_name_follows_swift();
    void failed_screenshot_discards_the_capture();
    void full_screen_sidebar_keeps_the_stored_width();
};

namespace {

struct BarCase {
    double availableWidth;
    const char* density;
};

const QStringList kUtilityButtons{
    QStringLiteral("gapFastForwardButton"), QStringLiteral("miningHistoryButton"),
    QStringLiteral("openVideoButton"),      QStringLiteral("mineSubtitleButton"),
    QStringLiteral("inspectorButton"),      QStringLiteral("screenshotButton"),
    QStringLiteral("fullScreenButton"),
};

const QStringList kFullDensityButtons{
    QStringLiteral("gapFastForwardButton"), QStringLiteral("miningHistoryButton"),
    QStringLiteral("openVideoButton"),      QStringLiteral("mineSubtitleButton"),
    QStringLiteral("inspectorButton"),      QStringLiteral("screenshotButton"),
    QStringLiteral("fullScreenButton"),     QStringLiteral("volumeButton"),
    QStringLiteral("previousButton"),       QStringLiteral("nextButton"),
    QStringLiteral("playButton"),           QStringLiteral("speedButton"),
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
                        QStringLiteral("speedButton"), QStringLiteral("gapFastForwardButton"),
                        QStringLiteral("miningHistoryButton"), QStringLiteral("openVideoButton"),
                        QStringLiteral("mineSubtitleButton"), QStringLiteral("inspectorButton"),
                        QStringLiteral("screenshotButton"), QStringLiteral("fullScreenButton")}) ==
            QStringList({QStringLiteral("volumeButton"), QStringLiteral("previousButton"),
                         QStringLiteral("playButton"), QStringLiteral("nextButton"),
                         QStringLiteral("speedButton"), QStringLiteral("gapFastForwardButton"),
                         QStringLiteral("miningHistoryButton"), QStringLiteral("openVideoButton"),
                         QStringLiteral("mineSubtitleButton"), QStringLiteral("inspectorButton"),
                         QStringLiteral("screenshotButton"), QStringLiteral("fullScreenButton")}),
        "the lower row clusters are out of order");
    for (const QString& name : kFullDensityButtons) {
        QVERIFY2(shown(bar, name), qPrintable(name + QStringLiteral(" is missing at full")));
    }
    for (const QString& name :
         {QStringLiteral("cycleSubtitlesButton"), QStringLiteral("videoSettingsButton")}) {
        QVERIFY2(item_named(bar, name) == nullptr, qPrintable(name + QStringLiteral(" is built")));
    }

    bar->setProperty("availableWidth", 500.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("condensed"));
    for (const QString& name :
         {QStringLiteral("previousButton"), QStringLiteral("playButton"),
          QStringLiteral("nextButton"), QStringLiteral("speedButton"),
          QStringLiteral("openVideoButton"), QStringLiteral("inspectorButton"),
          QStringLiteral("screenshotButton"), QStringLiteral("fullScreenButton")}) {
        QVERIFY2(shown(bar, name), qPrintable(name + QStringLiteral(" is missing condensed")));
    }
    for (const QString& name :
         {QStringLiteral("volumeButton"), QStringLiteral("miningHistoryButton"),
          QStringLiteral("mineSubtitleButton"), QStringLiteral("gapFastForwardButton")}) {
        QVERIFY2(!shown(bar, name), qPrintable(name + QStringLiteral(" stays condensed")));
    }

    QSignalSpy close_spy(bar, SIGNAL(speedPanelCloseRequested()));
    bar->setProperty("speedPanelOpen", true);
    bar->setProperty("availableWidth", 421.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("minimal"));
    QTRY_COMPARE(close_spy.count(), 1);
    for (const QString& name : {QStringLiteral("previousButton"), QStringLiteral("playButton"),
                                QStringLiteral("nextButton"), QStringLiteral("screenshotButton"),
                                QStringLiteral("fullScreenButton")}) {
        QVERIFY2(shown(bar, name), qPrintable(name + QStringLiteral(" is missing minimal")));
    }
    for (const QString& name :
         {QStringLiteral("speedButton"), QStringLiteral("openVideoButton"),
          QStringLiteral("volumeButton"), QStringLiteral("miningHistoryButton"),
          QStringLiteral("mineSubtitleButton"), QStringLiteral("inspectorButton"),
          QStringLiteral("gapFastForwardButton")}) {
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
        QStringLiteral("previousButton"),       QStringLiteral("playButton"),
        QStringLiteral("nextButton"),           QStringLiteral("volumeButton"),
        QStringLiteral("compactTime"),          QStringLiteral("speedButton"),
        QStringLiteral("gapFastForwardButton"), QStringLiteral("miningHistoryButton"),
        QStringLiteral("openVideoButton"),      QStringLiteral("mineSubtitleButton"),
        QStringLiteral("inspectorButton"),      QStringLiteral("screenshotButton"),
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
          QStringLiteral("screenshotButton"), QStringLiteral("fullScreenButton")}) {
        QVERIFY2(shown(bar, name),
                 qPrintable(name + QStringLiteral(" is missing compact condensed")));
    }
    QVERIFY2(!shown(bar, QStringLiteral("volumeButton")),
             "the volume track stays in the compact full row");
    QVERIFY2(!shown(bar, QStringLiteral("gapFastForwardButton")),
             "the gap fast-forward button stays in the compact condensed row");

    bar->setProperty("availableWidth", 389.0);
    QTRY_COMPARE(bar->property("density").toString(), QStringLiteral("minimal"));
    for (const QString& name : {QStringLiteral("previousButton"), QStringLiteral("playButton"),
                                QStringLiteral("nextButton"), QStringLiteral("screenshotButton"),
                                QStringLiteral("fullScreenButton")}) {
        QVERIFY2(shown(bar, name),
                 qPrintable(name + QStringLiteral(" is missing compact minimal")));
    }
    for (const QString& name : {QStringLiteral("speedButton"), QStringLiteral("inspectorButton"),
                                QStringLiteral("gapFastForwardButton")}) {
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
                                 {QStringLiteral("canMine"), true},
                                 {QStringLiteral("gapFastForwardEnabled"), true}},
                                error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* utility = attach(window, object.get());
    QVERIFY(utility != nullptr);

    QCOMPARE(named_in_order(utility, kUtilityButtons), kUtilityButtons);
    QQuickItem* gap_button = item_named(utility, QStringLiteral("gapFastForwardButton"));
    QVERIFY(gap_button != nullptr);
    QCOMPARE(gap_button->property("tip").toString(), QStringLiteral("Fast-forward Subtitle Gaps"));
    QVERIFY(gap_button->property("active").toBool());
    utility->setProperty("gapFastForwardEnabled", false);
    QTRY_VERIFY(!gap_button->property("active").toBool());
    utility->setProperty("gapFastForwardEnabled", true);
    QTRY_VERIFY(gap_button->property("active").toBool());
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
    QSignalSpy gap_spy(utility, SIGNAL(toggleGapFastForwardRequested()));

    click(item_named(utility, QStringLiteral("miningHistoryButton")));
    click(item_named(utility, QStringLiteral("openVideoButton")));
    click(item_named(utility, QStringLiteral("mineSubtitleButton")));
    click(item_named(utility, QStringLiteral("inspectorButton")));
    click(gap_button);
    QCOMPARE(study_spy.count(), 1);
    QCOMPARE(open_spy.count(), 1);
    QCOMPARE(mine_spy.count(), 1);
    QCOMPARE(inspector_spy.count(), 1);
    QCOMPARE(gap_spy.count(), 1);

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

namespace {

const QByteArray kNavigationSrt = QByteArray("1\n00:00:00,000 --> 00:00:01,500\nfirst\n\n"
                                             "2\n00:00:02,000 --> 00:00:04,000\nsecond\n\n"
                                             "3\n00:00:05,000 --> 00:00:07,000\nthird\n");
const QByteArray kLongSrt = QByteArray("1\n00:00:00,000 --> 00:00:01,000\nfirst\n\n"
                                       "2\n00:02:20,000 --> 00:02:21,000\nsecond\n");

}  // namespace

// B1, B3, B5: Alt+Left/Right seek the adjacent primary cue; Shift+Left/Right
// align it through the study model, and only actionable, non-autorepeat
// modified arrows are consumed.
void PlayerControlsTest::subtitle_shortcuts_navigate_and_align() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY(player->setProperty("playbackReady", true));
    auto* study = player->findChild<niratan::viewmodels::PlayerStudyViewModel*>();
    QVERIFY(study != nullptr);
    study->setVideo(QUrl::fromLocalFile(video));
    study->setPlaybackReady(true);
    study->setSubtitleDocument(QUrl::fromLocalFile(QStringLiteral("/tmp/navigation.srt")),
                               kNavigationSrt, true);
    study->updatePlayback(3.25, 0.0);
    QSignalSpy seeks(study, &niratan::viewmodels::PlayerStudyViewModel::seekRequested);

    // An active transcript text field keeps the arrow in the text editor.
    QVERIFY(QMetaObject::invokeMethod(player, "toggleTranscript"));
    QQuickItem* sidebar = item_named(video_window->contentItem(), QStringLiteral("studySidebar"));
    QVERIFY(sidebar != nullptr);
    sidebar->setProperty("selectedTab", 1);
    QQuickItem* search = item_named(sidebar, QStringLiteral("transcriptSearchField"));
    QVERIFY(search != nullptr);
    search->forceActiveFocus();
    QTRY_VERIFY(video_window->activeFocusItem() == search ||
                (video_window->activeFocusItem() != nullptr &&
                 search->isAncestorOf(video_window->activeFocusItem())));
    KeyEventProbe in_text(Qt::Key_Right, Qt::AltModifier, false);
    send_handle_key(player, in_text);
    QVERIFY(!in_text.accepted());
    QCOMPARE(seeks.size(), 0);

    // With the player focused, Alt+Right seeks the next line and is consumed.
    player->forceActiveFocus();
    QTRY_VERIFY(!search->hasActiveFocus());
    KeyEventProbe next(Qt::Key_Right, Qt::AltModifier, false);
    send_handle_key(player, next);
    QVERIFY(next.accepted());
    QCOMPARE(seeks.size(), 1);
    QCOMPARE(seeks.takeFirst().front().toDouble(), 5.0);

    study->updatePlayback(3.25, 0.0);
    KeyEventProbe previous(Qt::Key_Left, Qt::AltModifier, false);
    send_handle_key(player, previous);
    QVERIFY(previous.accepted());
    QCOMPARE(seeks.size(), 1);
    QCOMPARE(seeks.takeFirst().front().toDouble(), 0.0);

    QSignalSpy alignments(study,
                          &niratan::viewmodels::PlayerStudyViewModel::subtitleAlignmentRequested);
    study->updatePlayback(3.25, 0.0);
    KeyEventProbe align_previous(Qt::Key_Left, Qt::ShiftModifier, false);
    send_handle_key(player, align_previous);
    QVERIFY(align_previous.accepted());
    QCOMPARE(alignments.size(), 1);
    QCOMPARE(alignments.takeFirst().front().toDouble(), 3.25);
    study->updatePlayback(3.25, 0.0);
    KeyEventProbe align_next(Qt::Key_Right, Qt::ShiftModifier, false);
    send_handle_key(player, align_next);
    QVERIFY(align_next.accepted());
    QCOMPARE(alignments.size(), 1);
    QCOMPARE(alignments.takeFirst().front().toDouble(), -1.75);

    KeyEventProbe repeated(Qt::Key_Right, Qt::AltModifier, true);
    send_handle_key(player, repeated);
    QVERIFY(!repeated.accepted());
    KeyEventProbe plain(Qt::Key_Right, Qt::NoModifier, false);
    send_handle_key(player, plain);
    QVERIFY(plain.accepted());
    KeyEventProbe both(Qt::Key_Right, int(Qt::AltModifier | Qt::ShiftModifier), false);
    send_handle_key(player, both);
    QVERIFY(!both.accepted());
    QCOMPARE(seeks.size(), 0);
    QCOMPARE(alignments.size(), 0);
}

// B5: the page applies the alignment delay through the engine and its existing
// clamp, without seeking.
void PlayerControlsTest::subtitle_alignment_clamps_delay_and_dismisses_lookup() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY(player->setProperty("playbackReady", true));
    QQuickItem* mpv = item_named(video_window->contentItem(), QStringLiteral("mpvItem"));
    QVERIFY(mpv != nullptr);
    auto* study = player->findChild<niratan::viewmodels::PlayerStudyViewModel*>();
    QVERIFY(study != nullptr);
    study->setVideo(QUrl::fromLocalFile(video));
    study->setPlaybackReady(true);
    study->setSubtitleDocument(QUrl::fromLocalFile(QStringLiteral("/tmp/long.srt")), kLongSrt,
                               true);
    study->updatePlayback(70.0, 0.0);
    mpv->setProperty("subtitleDelay", 0.0);
    QSignalSpy seeks(study, &niratan::viewmodels::PlayerStudyViewModel::seekRequested);

    KeyEventProbe previous(Qt::Key_Left, Qt::ShiftModifier, false);
    send_handle_key(player, previous);
    QVERIFY(previous.accepted());
    QTRY_COMPARE(mpv->property("subtitleDelay").toDouble(), 60.0);

    mpv->setProperty("subtitleDelay", 0.0);
    study->updatePlayback(70.0, 0.0);
    KeyEventProbe next(Qt::Key_Right, Qt::ShiftModifier, false);
    send_handle_key(player, next);
    QVERIFY(next.accepted());
    QTRY_COMPARE(mpv->property("subtitleDelay").toDouble(), -60.0);
    QCOMPARE(seeks.size(), 0);
}

// B6: the transcript toolbar appears from the unfiltered cue list, even when
// the active search matches nothing, and its buttons share the study commands.
void PlayerControlsTest::transcript_toolbar_exists_without_search_matches() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    auto* study = player->findChild<niratan::viewmodels::PlayerStudyViewModel*>();
    QVERIFY(study != nullptr);
    study->setVideo(QUrl::fromLocalFile(video));
    study->setPlaybackReady(true);
    study->setSubtitleDocument(QUrl::fromLocalFile(QStringLiteral("/tmp/navigation.srt")),
                               kNavigationSrt, true);
    study->updatePlayback(0.0, 0.0);
    study->set_search(QStringLiteral("no match"));
    QVERIFY(study->transcript().isEmpty());

    QQuickItem* sidebar = item_named(video_window->contentItem(), QStringLiteral("studySidebar"));
    QVERIFY(sidebar != nullptr);
    QVERIFY(QMetaObject::invokeMethod(player, "toggleTranscript"));
    QTRY_VERIFY(sidebar->isVisible());
    sidebar->setProperty("selectedTab", 1);

    QQuickItem* toolbar = item_named(sidebar, QStringLiteral("transcriptAlignToolbar"));
    QVERIFY2(toolbar != nullptr, "the transcript toolbar is missing");
    QTRY_VERIFY(toolbar->isVisible());
    QQuickItem* previous = item_named(toolbar, QStringLiteral("alignPreviousSubtitleButton"));
    QQuickItem* next = item_named(toolbar, QStringLiteral("alignNextSubtitleButton"));
    QVERIFY(previous != nullptr);
    QVERIFY(next != nullptr);
    QVERIFY(!previous->property("enabled").toBool());
    QVERIFY(next->property("enabled").toBool());

    QSignalSpy alignments(study,
                          &niratan::viewmodels::PlayerStudyViewModel::subtitleAlignmentRequested);
    QVERIFY(QMetaObject::invokeMethod(next, "clicked"));
    QCOMPARE(alignments.size(), 1);
    QCOMPARE(alignments.takeFirst().front().toDouble(), -2.0);

    study->updatePlayback(7.5, 0.0);
    QTRY_VERIFY(previous->property("enabled").toBool());
    QTRY_VERIFY(!next->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(previous, "clicked"));
    QCOMPARE(alignments.size(), 1);
    QCOMPARE(alignments.takeFirst().front().toDouble(), 2.5);

    study->setSubtitleDocument({}, {}, false);
    QTRY_VERIFY(!toolbar->isVisible());
}

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

// B3: the transport Previous/Next controls follow the Inspector's playlist and
// route through the same episode-open command, on their sides of Play/Pause.
void PlayerControlsTest::episode_transport_navigation() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString folder = make_episode_folder(root.path());
    QVERIFY(!folder.isEmpty());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(
        engine, episode_url(folder, "Show 2.mkv").toLocalFile(), &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY(player->setProperty("playbackReady", true));
    auto* inspector = player->findChild<niratan::viewmodels::PlayerInspectorViewModel*>(
        QStringLiteral("inspectorModel"));
    QVERIFY(inspector != nullptr);
    QTRY_VERIFY(inspector->can_play_previous_episode());
    QTRY_VERIFY(inspector->can_play_next_episode());
    QQuickItem* mpv = item_named(video_window->contentItem(), QStringLiteral("mpvItem"));
    QVERIFY(mpv != nullptr);

    QQuickItem* bar = item_named(player, QStringLiteral("videoControlBar"));
    QVERIFY(bar != nullptr);
    QQuickItem* previous = item_named(bar, QStringLiteral("previousButton"));
    QQuickItem* next = item_named(bar, QStringLiteral("nextButton"));
    QVERIFY(previous != nullptr);
    QVERIFY(next != nullptr);
    QVERIFY(previous->property("enabled").toBool());
    QVERIFY(next->property("enabled").toBool());

    QSignalSpy spy(inspector, &niratan::viewmodels::PlayerInspectorViewModel::episodeRequested);
    player->setProperty("chromeVisible", false);
    click(next);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUrl(), episode_url(folder, "Show 3.mkv"));
    QVERIFY(player->property("chromeVisible").toBool());
    QTRY_COMPARE(player->property("videoFile").toUrl(), episode_url(folder, "Show 3.mkv"));
    QTRY_VERIFY(!next->property("enabled").toBool());
    QVERIFY(previous->property("enabled").toBool());

    click(previous);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toUrl(), episode_url(folder, "Show 2.mkv"));
    QTRY_COMPARE(player->property("videoFile").toUrl(), episode_url(folder, "Show 2.mkv"));

    // B3: the compact transport carries the same availability and commands.
    bool stored = false;
    QVERIFY(QMetaObject::invokeMethod(mpv, "changeVideoSetting", Q_RETURN_ARG(bool, stored),
                                      Q_ARG(QString, QStringLiteral("videoControlBarLayout")),
                                      Q_ARG(QVariant, QVariant(QStringLiteral("compactBottom")))));
    QVERIFY(stored);
    QTRY_COMPARE(bar->property("layoutMode").toString(), QStringLiteral("compactBottom"));
    previous = item_named(bar, QStringLiteral("previousButton"));
    next = item_named(bar, QStringLiteral("nextButton"));
    QVERIFY(previous != nullptr);
    QVERIFY(next != nullptr);
    QVERIFY(previous->property("enabled").toBool());
    QVERIFY(next->property("enabled").toBool());

    click(previous);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(2).at(0).toUrl(), episode_url(folder, "Show 1.mkv"));
    QTRY_COMPARE(player->property("videoFile").toUrl(), episode_url(folder, "Show 1.mkv"));
    QTRY_VERIFY(!previous->property("enabled").toBool());
    QTRY_VERIFY(next->property("enabled").toBool());
}

// B2: the ordinary episode open keeps Show 3's stored 12.0 s resumable state
// instead of starting from the beginning. Swift prepares the pending restore
// position on open and consumes it after loading, so the store is the check.
void PlayerControlsTest::episode_open_preserves_resume_history() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString folder = make_episode_folder(root.path());
    QVERIFY(!folder.isEmpty());
    const QUrl third = episode_url(folder, "Show 3.mkv");

    niratan::player::PlaybackHistory history = niratan::player::PlaybackHistory::at_data_root();
    QVERIFY(history.save(third.toLocalFile(), 12.0, 100.0));
    QCOMPARE(history.resume_position(third.toLocalFile()).value_or(-1.0), 12.0);

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(
        engine, episode_url(folder, "Show 2.mkv").toLocalFile(), &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    auto* inspector = player->findChild<niratan::viewmodels::PlayerInspectorViewModel*>(
        QStringLiteral("inspectorModel"));
    QVERIFY(inspector != nullptr);
    QTRY_VERIFY(inspector->can_play_next_episode());
    QQuickItem* mpv = item_named(video_window->contentItem(), QStringLiteral("mpvItem"));
    QVERIFY(mpv != nullptr);
    QVERIFY(mpv->property("videoSettings").toMap().value("videoRememberPlaybackPosition").toBool());

    QSignalSpy spy(inspector, &niratan::viewmodels::PlayerInspectorViewModel::episodeRequested);
    QQuickItem* bar = item_named(player, QStringLiteral("videoControlBar"));
    QVERIFY(bar != nullptr);
    click(item_named(bar, QStringLiteral("nextButton")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUrl(), third);
    QTRY_COMPARE(player->property("videoFile").toUrl(), third);

    const auto state = history.state(third.toLocalFile());
    QVERIFY(state.has_value());
    QCOMPARE(state->position, 12.0);
    QVERIFY(state->resumable());
}

// B4: Ctrl+Shift+Left/Right are Niratan-win's episode bindings: exactly one
// request per press, no autorepeat, no wrap, and text input keeps the keys.
void PlayerControlsTest::episode_shortcuts_navigate_neighbors() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString folder = make_episode_folder(root.path());
    QVERIFY(!folder.isEmpty());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(
        engine, episode_url(folder, "Show 2.mkv").toLocalFile(), &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY(player->setProperty("playbackReady", true));
    auto* inspector = player->findChild<niratan::viewmodels::PlayerInspectorViewModel*>(
        QStringLiteral("inspectorModel"));
    QVERIFY(inspector != nullptr);
    QTRY_VERIFY(inspector->can_play_next_episode());
    QSignalSpy spy(inspector, &niratan::viewmodels::PlayerInspectorViewModel::episodeRequested);

    KeyEventProbe next(Qt::Key_Right, int(Qt::ControlModifier | Qt::ShiftModifier), false);
    send_handle_key(player, next);
    QVERIFY(next.accepted());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUrl(), episode_url(folder, "Show 3.mkv"));
    QTRY_COMPARE(player->property("videoFile").toUrl(), episode_url(folder, "Show 3.mkv"));

    // The last episode has no next neighbor, so the key is not consumed.
    KeyEventProbe beyond(Qt::Key_Right, int(Qt::ControlModifier | Qt::ShiftModifier), false);
    send_handle_key(player, beyond);
    QVERIFY(!beyond.accepted());
    QCOMPARE(spy.count(), 1);

    KeyEventProbe previous(Qt::Key_Left, int(Qt::ControlModifier | Qt::ShiftModifier), false);
    send_handle_key(player, previous);
    QVERIFY(previous.accepted());
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toUrl(), episode_url(folder, "Show 2.mkv"));
    QTRY_COMPARE(player->property("videoFile").toUrl(), episode_url(folder, "Show 2.mkv"));

    // Exact modifiers: repeat, Control alone and Alt added do not navigate.
    KeyEventProbe repeated(Qt::Key_Right, int(Qt::ControlModifier | Qt::ShiftModifier), true);
    send_handle_key(player, repeated);
    QVERIFY(!repeated.accepted());
    KeyEventProbe control_only(Qt::Key_Right, Qt::ControlModifier, false);
    send_handle_key(player, control_only);
    QVERIFY(!control_only.accepted());
    KeyEventProbe with_alt(Qt::Key_Right,
                           int(Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier), false);
    send_handle_key(player, with_alt);
    QVERIFY(!with_alt.accepted());
    QCOMPARE(spy.count(), 2);

    // The first episode has no previous neighbor.
    QVERIFY(QMetaObject::invokeMethod(player, "openVideo",
                                      Q_ARG(QVariant, QVariant(episode_url(folder, "Show 1.mkv"))),
                                      Q_ARG(QVariant, QVariant(false))));
    QTRY_COMPARE(player->property("videoFile").toUrl(), episode_url(folder, "Show 1.mkv"));
    QTRY_VERIFY(!inspector->can_play_previous_episode());
    KeyEventProbe before(Qt::Key_Left, int(Qt::ControlModifier | Qt::ShiftModifier), false);
    send_handle_key(player, before);
    QVERIFY(!before.accepted());
    QCOMPARE(spy.count(), 2);

    // A focused transcript field keeps the keys for text editing.
    QVERIFY(QMetaObject::invokeMethod(player, "toggleTranscript"));
    QQuickItem* sidebar = item_named(video_window->contentItem(), QStringLiteral("studySidebar"));
    QVERIFY(sidebar != nullptr);
    sidebar->setProperty("selectedTab", 1);
    QQuickItem* search = item_named(sidebar, QStringLiteral("transcriptSearchField"));
    QVERIFY(search != nullptr);
    search->forceActiveFocus();
    QTRY_VERIFY(search->hasActiveFocus());
    KeyEventProbe in_text(Qt::Key_Right, int(Qt::ControlModifier | Qt::ShiftModifier), false);
    send_handle_key(player, in_text);
    QVERIFY(!in_text.accepted());
    QCOMPARE(spy.count(), 2);
}

// B5, B6: EOF reads videoAutoPlayNext each time; a change applies without
// reopening, the final episode opens nothing, and manual transport ignores it.
void PlayerControlsTest::autoplay_consumes_setting_at_eof() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString folder = make_episode_folder(root.path());
    QVERIFY(!folder.isEmpty());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(
        engine, episode_url(folder, "Show 2.mkv").toLocalFile(), &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    auto* inspector = player->findChild<niratan::viewmodels::PlayerInspectorViewModel*>(
        QStringLiteral("inspectorModel"));
    QVERIFY(inspector != nullptr);
    QTRY_VERIFY(inspector->can_play_next_episode());
    QSignalSpy spy(inspector, &niratan::viewmodels::PlayerInspectorViewModel::episodeRequested);

    QQuickItem* mpv = item_named(video_window->contentItem(), QStringLiteral("mpvItem"));
    QVERIFY(mpv != nullptr);
    QVERIFY(mpv->property("videoSettings").toMap().value("videoAutoPlayNext").toBool());

    bool stored = false;
    QVERIFY(QMetaObject::invokeMethod(mpv, "changeVideoSetting", Q_RETURN_ARG(bool, stored),
                                      Q_ARG(QString, QStringLiteral("videoAutoPlayNext")),
                                      Q_ARG(QVariant, QVariant(false))));
    QVERIFY(stored);
    QVERIFY(QMetaObject::invokeMethod(mpv, "ended"));
    QCOMPARE(spy.count(), 0);

    QVERIFY(QMetaObject::invokeMethod(mpv, "changeVideoSetting", Q_RETURN_ARG(bool, stored),
                                      Q_ARG(QString, QStringLiteral("videoAutoPlayNext")),
                                      Q_ARG(QVariant, QVariant(true))));
    QVERIFY(stored);
    QVERIFY(QMetaObject::invokeMethod(mpv, "ended"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUrl(), episode_url(folder, "Show 3.mkv"));
    QTRY_COMPARE(player->property("videoFile").toUrl(), episode_url(folder, "Show 3.mkv"));

    // Last episode: autoplay has no next neighbor to open.
    QVERIFY(QMetaObject::invokeMethod(mpv, "ended"));
    QCOMPARE(spy.count(), 1);

    // Manual transport still navigates while autoplay is off.
    QVERIFY(QMetaObject::invokeMethod(mpv, "changeVideoSetting", Q_RETURN_ARG(bool, stored),
                                      Q_ARG(QString, QStringLiteral("videoAutoPlayNext")),
                                      Q_ARG(QVariant, QVariant(false))));
    QVERIFY(stored);
    QQuickItem* bar = item_named(player, QStringLiteral("videoControlBar"));
    QVERIFY(bar != nullptr);
    QQuickItem* previous = item_named(bar, QStringLiteral("previousButton"));
    QVERIFY(previous != nullptr);
    QTRY_VERIFY(previous->property("enabled").toBool());
    click(previous);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toUrl(), episode_url(folder, "Show 2.mkv"));
}

// B6: the utility toggle and Shift+F (VideoShortcutActions.swift) change the
// durable preference and its button state; a repeat or a focused text field
// leaves the preference alone.
void PlayerControlsTest::gap_fast_forward_toggle_from_button_and_shortcut() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QVERIFY(player->setProperty("playbackReady", true));
    QQuickItem* mpv = item_named(video_window->contentItem(), QStringLiteral("mpvItem"));
    QVERIFY(mpv != nullptr);
    QQuickItem* bar = item_named(player, QStringLiteral("videoControlBar"));
    QVERIFY(bar != nullptr);
    QQuickItem* button = item_named(bar, QStringLiteral("gapFastForwardButton"));
    QVERIFY2(button != nullptr, "the gap fast-forward button is missing");
    QCOMPARE(button->property("tip").toString(), QStringLiteral("Fast-forward Subtitle Gaps"));

    const auto gap_enabled = [mpv] {
        return mpv->property("videoSettings")
            .toMap()
            .value("videoSubtitleGapFastForwardEnabled")
            .toBool();
    };
    QVERIFY(!gap_enabled());
    QVERIFY(!button->property("active").toBool());

    click(button);
    QTRY_VERIFY(gap_enabled());
    QTRY_VERIFY(button->property("active").toBool());

    KeyEventProbe toggle(Qt::Key_F, Qt::ShiftModifier, false);
    send_handle_key(player, toggle);
    QVERIFY(toggle.accepted());
    QTRY_VERIFY(!gap_enabled());
    QTRY_VERIFY(!button->property("active").toBool());

    KeyEventProbe repeated(Qt::Key_F, Qt::ShiftModifier, true);
    send_handle_key(player, repeated);
    QVERIFY(!repeated.accepted());
    QVERIFY(!gap_enabled());

    // A focused transcript field keeps the key for text editing.
    QVERIFY(QMetaObject::invokeMethod(player, "toggleTranscript"));
    QQuickItem* sidebar = item_named(video_window->contentItem(), QStringLiteral("studySidebar"));
    QVERIFY(sidebar != nullptr);
    sidebar->setProperty("selectedTab", 1);
    QQuickItem* search = item_named(sidebar, QStringLiteral("transcriptSearchField"));
    QVERIFY(search != nullptr);
    search->forceActiveFocus();
    QTRY_VERIFY(search->hasActiveFocus());
    KeyEventProbe in_text(Qt::Key_F, Qt::ShiftModifier, false);
    send_handle_key(player, in_text);
    QVERIFY(!in_text.accepted());
    QVERIFY(!gap_enabled());
}

// B3, B4: search results carry the match count in a caption that scrolls with
// them, and each row carries the query's ranges in its original cue text.
void PlayerControlsTest::transcript_search_caption_and_highlights() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    niratan::viewmodels::PlayerStudyViewModel model;
    model.setSubtitleDocument(QUrl::fromLocalFile(QStringLiteral("/tmp/search.srt")),
                              QByteArray("1\n00:00:01,000 --> 00:00:02,000\none needle here\n\n"
                                         "2\n00:00:03,000 --> 00:00:04,000\ntwo\n\n"
                                         "3\n00:00:05,000 --> 00:00:06,000\nneedle again\n"),
                              true);
    model.setPlaybackReady(true);
    model.updatePlayback(1.5, 0.0);

    QQmlApplicationEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> holder;
    QQuickItem* sidebar = make_study_sidebar(engine, model, 1, window, holder);
    QVERIFY(sidebar != nullptr);
    QQuickItem* list = item_named(sidebar, QStringLiteral("transcriptList"));
    QVERIFY2(list != nullptr, "the transcript list is missing");
    QQuickItem* caption = item_named(sidebar, QStringLiteral("searchMatchCountLabel"));
    QVERIFY2(caption != nullptr, "the match caption is missing");
    QVERIFY(!caption->isVisible());

    model.set_search(QStringLiteral("NEEDLE"));
    QTRY_COMPARE(model.search_match_count(), 2);
    QTRY_VERIFY((caption = item_named(sidebar, QStringLiteral("searchMatchCountLabel"))) !=
                nullptr);
    QTRY_COMPARE(caption->property("text").toString(), QStringLiteral("2 Matches"));
    QVERIFY(caption->isVisible());
    QQuickItem* header = list->property("headerItem").value<QQuickItem*>();
    QVERIFY2(header != nullptr, "the caption is not part of the scrolling results");
    QVERIFY(is_ancestor_of(header, caption));

    const QVariantList expected =
        model.transcript().first().toMap().value(QStringLiteral("highlights")).toList();
    QVERIFY(!expected.isEmpty());
    QQuickItem* row = nullptr;
    QTRY_VERIFY((row = item_named(sidebar, QStringLiteral("transcriptRow"))) != nullptr);
    QTRY_COMPARE(row->property("highlights").toList(), expected);

    model.set_search(QStringLiteral("absent"));
    QTRY_VERIFY((caption = item_named(sidebar, QStringLiteral("searchMatchCountLabel"))) !=
                nullptr);
    QTRY_VERIFY(!caption->isVisible());
    QQuickItem* empty = item_named(sidebar, QStringLiteral("transcriptEmptyState"));
    QVERIFY(empty != nullptr);
    QVERIFY(empty->isVisible());
}

// B2, B5: clearing a query resumes following and recenters on the current row;
// while the query is active playback does not move the result list.
void PlayerControlsTest::transcript_search_clearing_resumes_following() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    niratan::viewmodels::PlayerStudyViewModel model;
    model.setSubtitleDocument(
        QUrl::fromLocalFile(QStringLiteral("/tmp/many.srt")),
        numbered_cues_srt(200, [](int i) { return QStringLiteral("line %1").arg(i); }), true);
    model.setPlaybackReady(true);
    model.updatePlayback(241.0, 0.0);

    QQmlApplicationEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> holder;
    QQuickItem* sidebar = make_study_sidebar(engine, model, 1, window, holder);
    QVERIFY(sidebar != nullptr);
    QQuickItem* resume = item_named(sidebar, QStringLiteral("resumeFollowingButton"));
    QVERIFY2(resume != nullptr, "the resume-following control is missing");

    sidebar->setProperty("followingTranscript", false);
    QTRY_VERIFY(resume->isVisible());

    model.set_search(QStringLiteral("line 1"));
    QTRY_VERIFY(!resume->isVisible());
    const QVariantList results = model.transcript();
    model.updatePlayback(361.0, 0.0);
    QCOMPARE(model.transcript(), results);

    model.set_search({});
    QTRY_VERIFY(sidebar->property("followingTranscript").toBool());
    QVERIFY(!resume->isVisible());
    QVERIFY(model.current_visible_row() >= 0);

    // Typing starts the results at the top; clearing through the field
    // recenters on the current row instead of jumping to the top.
    QQuickItem* field = item_named(sidebar, QStringLiteral("transcriptSearchField"));
    QVERIFY(field != nullptr);
    QQuickItem* list = item_named(sidebar, QStringLiteral("transcriptList"));
    QVERIFY(list != nullptr);
    field->setProperty("text", QStringLiteral("line 1"));
    QVERIFY(QMetaObject::invokeMethod(field, "textEdited"));
    QTRY_VERIFY(model.search_active());
    QTRY_VERIFY(list->property("atYBeginning").toBool());

    field->setProperty("text", QString());
    QVERIFY(QMetaObject::invokeMethod(field, "textEdited"));
    QTRY_VERIFY(!model.search_active());
    QTRY_VERIFY(sidebar->property("followingTranscript").toBool());
    QTRY_VERIFY(!list->property("atYBeginning").toBool());

    sidebar->setProperty("followingTranscript", false);
    QTRY_VERIFY(resume->isVisible());
    model.setSubtitleDocument(
        QUrl::fromLocalFile(QStringLiteral("/tmp/new.srt")),
        numbered_cues_srt(3, [](int i) { return QStringLiteral("new %1").arg(i); }), true);
    QTRY_VERIFY(sidebar->property("followingTranscript").toBool());
    QVERIFY(!resume->isVisible());
}

// B6: the current chapter row shows raw playback progress; other rows and
// chapters without an end show none.
void PlayerControlsTest::chapter_progress_follows_raw_playback() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    niratan::viewmodels::PlayerStudyViewModel model;
    const auto chapter = [](int id, const char* title, double start, double duration) {
        return QVariantMap{{QStringLiteral("id"), id},
                           {QStringLiteral("title"), QString::fromLatin1(title)},
                           {QStringLiteral("start"), start},
                           {QStringLiteral("duration"), duration}};
    };
    model.set_chapters({chapter(0, "A", 0.0, 10.0), chapter(1, "B", 10.0, 10.0)});
    model.updatePlayback(15.0, 3.0);

    QQmlApplicationEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> holder;
    QQuickItem* sidebar = make_study_sidebar(engine, model, 2, window, holder);
    QVERIFY(sidebar != nullptr);
    QTRY_COMPARE(items_named(sidebar, QStringLiteral("chapterRow")).size(), qsizetype(2));

    const QList<QQuickItem*> rows = items_named(sidebar, QStringLiteral("chapterRow"));
    QQuickItem* progress = item_named(rows.at(1), QStringLiteral("chapterProgressBar"));
    QVERIFY2(progress != nullptr, "the current chapter has no progress bar");
    QVERIFY(progress->isVisible());
    QCOMPARE(progress->property("value").toDouble(), 0.5);

    QQuickItem* other = item_named(rows.at(0), QStringLiteral("chapterProgressBar"));
    QVERIFY2(other == nullptr || !other->isVisible(), "a non-current chapter shows a bar");

    // Without an end there is nothing to measure.
    model.set_chapters({QVariantMap{{QStringLiteral("id"), 0},
                                    {QStringLiteral("title"), QStringLiteral("A")},
                                    {QStringLiteral("start"), 0.0}}});
    model.updatePlayback(5.0, 0.0);
    QTRY_VERIFY(item_named(sidebar, QStringLiteral("chapterProgressBar")) == nullptr ||
                !item_named(sidebar, QStringLiteral("chapterProgressBar"))->isVisible());
}

// B3, B4: the history tab has its own search field, groups consecutive
// same-title records under a section header with a count, highlights matches
// in the row and distinguishes the no-results state from an empty history.
void PlayerControlsTest::history_search_sections_and_highlights() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const auto add = [](const char* id, double created_at, const char* title, const char* text) {
        niratan::player::MiningHistoryItem item;
        item.id = id;
        item.created_at = created_at;
        item.video_file_name = title;
        item.video_title = title;
        item.subtitle_source_name = title;
        item.subtitle_text = text;
        item.cue_start = 1.0;
        item.cue_end = 2.0;
        return niratan::player::MiningHistoryStore::at_data_root().add(item, 25);
    };
    auto store = niratan::player::MiningHistoryStore::at_data_root();
    QVERIFY(store.clear());
    QVERIFY(add("a-old", 100.0, "A.mkv", "alpha needle"));
    QVERIFY(add("b", 200.0, "B.mkv", "beta"));
    QVERIFY(add("b-blank", 250.0, "B.mkv", ""));
    QVERIFY(add("a-new", 300.0, "A.mkv", "alpha again"));

    niratan::viewmodels::PlayerStudyViewModel model;
    model.refreshHistory();

    QQmlApplicationEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> holder;
    QQuickItem* sidebar = make_study_sidebar(engine, model, 0, window, holder);
    QVERIFY(sidebar != nullptr);
    // PlayerPage routes this signal to studyModel.jumpToHistory and restores
    // the play state after the seek.
    HistoryJumpRelay relay(model);
    QObject::connect(sidebar, SIGNAL(historyJumpRequested(QString)), &relay, SLOT(jump(QString)));

    QQuickItem* field = item_named(sidebar, QStringLiteral("historySearchField"));
    QVERIFY2(field != nullptr, "the history search field is missing");
    QCOMPARE(field->property("placeholderText").toString(),
             QStringLiteral("Search Mining History"));

    const auto header_titles = [sidebar]() {
        QStringList titles;
        for (QQuickItem* header : items_named(sidebar, QStringLiteral("historySectionHeader"))) {
            titles.append(header->property("title").toString());
        }
        return titles;
    };
    QTRY_COMPARE(
        header_titles(),
        (QStringList{QStringLiteral("A.mkv"), QStringLiteral("B.mkv"), QStringLiteral("A.mkv")}));
    const QList<QQuickItem*> headers = items_named(sidebar, QStringLiteral("historySectionHeader"));
    const QStringList counts{QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("1")};
    for (qsizetype i = 0; i < headers.size(); ++i) {
        QCOMPARE(headers.at(i)->property("countText").toString(), counts.at(i));
    }

    // A blank subtitle keeps its row and is labelled instead of highlighted.
    QStringList row_titles;
    for (QQuickItem* history_row : items_named(sidebar, QStringLiteral("historyRow"))) {
        row_titles.append(history_row->property("title").toString());
    }
    QVERIFY(row_titles.contains(QStringLiteral("Blank Subtitle")));

    field = item_named(sidebar, QStringLiteral("historySearchField"));
    QVERIFY(field != nullptr);
    field->setProperty("text", QStringLiteral("needle"));
    QVERIFY(QMetaObject::invokeMethod(field, "textEdited"));
    QTRY_COMPARE(model.history_search(), QStringLiteral("needle"));
    QTRY_COMPARE(header_titles(), QStringList{QStringLiteral("A.mkv")});
    QCOMPARE(items_named(sidebar, QStringLiteral("historyRow")).size(), qsizetype(1));
    QQuickItem* row = items_named(sidebar, QStringLiteral("historyRow")).first();
    QCOMPARE(row->property("title").toString(), QStringLiteral("alpha needle"));
    const QVariantList highlights = row->property("highlights").toList();
    QCOMPARE(highlights.size(), qsizetype(1));
    QCOMPARE(highlights.at(0).toMap().value(QStringLiteral("start")).toInt(), 6);
    QCOMPARE(highlights.at(0).toMap().value(QStringLiteral("length")).toInt(), 6);

    // B4: the row carries its cue time, relative date and actions, and the
    // video title appears only in the section header.
    const QStringList row_texts = visual_texts(row);
    QVERIFY(row_texts.contains(QStringLiteral("0:01")));
    const QString relative_date = row->property("detail").toString();
    QVERIFY(!relative_date.isEmpty());
    QVERIFY(row_texts.contains(relative_date));
    QVERIFY(!row_texts.join(QLatin1Char('\n')).contains(QStringLiteral("A.mkv")));
    QQuickItem* copy_button = item_named(sidebar, QStringLiteral("copyHistoryButton"));
    QVERIFY2(copy_button != nullptr, "the history copy action is missing");
    QQuickItem* delete_button = item_named(sidebar, QStringLiteral("deleteHistoryButton"));
    QVERIFY2(delete_button != nullptr, "the history delete action is missing");

    // B6: a pathless record with the current video's file name requests its
    // cue with the subtitle delay; another current video is the legacy source.
    QSignalSpy history_requests(
        &model, &niratan::viewmodels::PlayerStudyViewModel::historySubtitleRequested);
    model.setVideo(QUrl::fromLocalFile(QStringLiteral("/tmp/A.mkv")));
    model.setPlaybackReady(true);
    model.updatePlayback(0.0, 0.5);
    QTRY_VERIFY((row = item_named(sidebar, QStringLiteral("historyRow"))) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(row, "clicked"));
    QCOMPARE(history_requests.size(), 1);
    QList<QVariant> request = history_requests.takeFirst();
    QCOMPARE(request.at(0).toUrl(), QUrl());
    QCOMPARE(request.at(1).toInt(), -1);
    QCOMPARE(request.at(2).toDouble(), 1.5);

    model.setVideo(QUrl::fromLocalFile(QStringLiteral("/tmp/Other.mkv")));
    model.setPlaybackReady(true);
    QTRY_VERIFY((row = item_named(sidebar, QStringLiteral("historyRow"))) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(row, "clicked"));
    QCOMPARE(model.last_mining_notice(),
             QStringLiteral("Open the matching video before using this older Mining History "
                            "item."));
    QCOMPARE(history_requests.size(), 0);

    QGuiApplication::clipboard()->setText(QStringLiteral("before"));
    QTRY_VERIFY((copy_button = item_named(sidebar, QStringLiteral("copyHistoryButton"))) !=
                nullptr);
    QVERIFY(QMetaObject::invokeMethod(copy_button, "clicked"));
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("alpha needle"));

    // B3: the clear button and Escape empty the field and restore all groups.
    QTRY_VERIFY((field = item_named(sidebar, QStringLiteral("historySearchField"))) != nullptr);
    QQuickItem* clear_button = nullptr;
    QTRY_VERIFY((clear_button = item_named(sidebar, QStringLiteral("historySearchClearButton"))) !=
                nullptr);
    QVERIFY(clear_button->isVisible());
    QVERIFY(QMetaObject::invokeMethod(clear_button, "clicked"));
    QTRY_COMPARE(model.history_search(), QString());
    QTRY_COMPARE(field->property("text").toString(), QString());
    QTRY_COMPARE(items_named(sidebar, QStringLiteral("historySectionHeader")).size(), qsizetype(3));

    QTRY_VERIFY((field = item_named(sidebar, QStringLiteral("historySearchField"))) != nullptr);
    field->setProperty("text", QStringLiteral("needle"));
    QVERIFY(QMetaObject::invokeMethod(field, "textEdited"));
    QTRY_COMPARE(model.history_search(), QStringLiteral("needle"));
    field->forceActiveFocus();
    QTRY_VERIFY(field->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_COMPARE(model.history_search(), QString());
    QTRY_COMPARE(field->property("text").toString(), QString());
    QTRY_COMPARE(items_named(sidebar, QStringLiteral("historySectionHeader")).size(), qsizetype(3));
    QTRY_VERIFY(!field->hasActiveFocus());

    field = item_named(sidebar, QStringLiteral("historySearchField"));
    QVERIFY(field != nullptr);
    field->setProperty("text", QStringLiteral("absent"));
    QVERIFY(QMetaObject::invokeMethod(field, "textEdited"));
    QTRY_COMPARE(model.history_search(), QStringLiteral("absent"));
    QTRY_COMPARE(items_named(sidebar, QStringLiteral("historySectionHeader")).size(), qsizetype(0));
    QQuickItem* no_results = nullptr;
    QTRY_VERIFY((no_results = item_named(sidebar, QStringLiteral("historyNoResults"))) != nullptr);
    QTRY_VERIFY(no_results->isVisible());
    QVERIFY(visual_texts(no_results).join(QLatin1Char('\n')).contains(QStringLiteral("absent")));

    QTRY_VERIFY((field = item_named(sidebar, QStringLiteral("historySearchField"))) != nullptr);
    field->setProperty("text", QString());
    QVERIFY(QMetaObject::invokeMethod(field, "textEdited"));
    QTRY_VERIFY(!model.history_search_active());
    QTRY_COMPARE(items_named(sidebar, QStringLiteral("historySectionHeader")).size(), qsizetype(3));
    QQuickItem* cleared_results = item_named(sidebar, QStringLiteral("historyNoResults"));
    QVERIFY(cleared_results == nullptr || !cleared_results->isVisible());

    // B4: the row context menu routes jump, copy and delete for its id.
    QTRY_VERIFY((field = item_named(sidebar, QStringLiteral("historySearchField"))) != nullptr);
    field->setProperty("text", QStringLiteral("needle"));
    QVERIFY(QMetaObject::invokeMethod(field, "textEdited"));
    QTRY_COMPARE(model.history_search(), QStringLiteral("needle"));
    model.setVideo(QUrl::fromLocalFile(QStringLiteral("/tmp/A.mkv")));
    model.setPlaybackReady(true);
    model.updatePlayback(0.0, 0.5);
    QTRY_VERIFY((row = item_named(sidebar, QStringLiteral("historyRow"))) != nullptr);
    QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier,
                      row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
    QQuickItem* menu_jump = nullptr;
    QTRY_VERIFY((menu_jump = item_named(window.contentItem(),
                                        QStringLiteral("historyJumpMenuItem"))) != nullptr);
    history_requests.clear();
    QVERIFY(QMetaObject::invokeMethod(menu_jump, "triggered"));
    QTRY_COMPARE(history_requests.size(), 1);
    QCOMPARE(history_requests.takeFirst().at(2).toDouble(), 1.5);

    QQuickItem* menu_copy = nullptr;
    QTRY_VERIFY((row = item_named(sidebar, QStringLiteral("historyRow"))) != nullptr);
    QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier,
                      row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
    QTRY_VERIFY((menu_copy = item_named(window.contentItem(),
                                        QStringLiteral("historyCopyMenuItem"))) != nullptr);
    QGuiApplication::clipboard()->setText(QStringLiteral("before"));
    QVERIFY(QMetaObject::invokeMethod(menu_copy, "triggered"));
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("alpha needle"));

    QQuickItem* menu_delete = nullptr;
    QTRY_VERIFY((row = item_named(sidebar, QStringLiteral("historyRow"))) != nullptr);
    QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier,
                      row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
    QTRY_VERIFY((menu_delete = item_named(window.contentItem(),
                                          QStringLiteral("historyDeleteMenuItem"))) != nullptr);
    const qsizetype history_before = model.history().size();
    QVERIFY(QMetaObject::invokeMethod(menu_delete, "triggered"));
    QTRY_COMPARE(model.history().size(), history_before - 1);
    QTRY_COMPARE(items_named(sidebar, QStringLiteral("historyRow")).size(), qsizetype(0));

    // Re-add the record and exercise the row's delete button as well.
    QVERIFY(add("a-old", 100.0, "A.mkv", "alpha needle"));
    model.refreshHistory();
    QTRY_VERIFY((field = item_named(sidebar, QStringLiteral("historySearchField"))) != nullptr);
    field->setProperty("text", QStringLiteral("needle"));
    QVERIFY(QMetaObject::invokeMethod(field, "textEdited"));
    QTRY_COMPARE(model.history_search(), QStringLiteral("needle"));
    QTRY_VERIFY((delete_button = item_named(sidebar, QStringLiteral("deleteHistoryButton"))) !=
                nullptr);
    const qsizetype button_before = model.history().size();
    QVERIFY(QMetaObject::invokeMethod(delete_button, "clicked"));
    QTRY_COMPARE(model.history().size(), button_before - 1);
    QTRY_COMPARE(items_named(sidebar, QStringLiteral("historyRow")).size(), qsizetype(0));

    // An empty history shows its own state and no toolbar.
    QVERIFY(store.clear());
    model.refreshHistory();
    QTRY_VERIFY(item_named(sidebar, QStringLiteral("historySearchField")) == nullptr);
    QQuickItem* empty = nullptr;
    QTRY_VERIFY((empty = item_named(sidebar, QStringLiteral("historyEmptyState"))) != nullptr);
    QTRY_VERIFY(empty->isVisible());
    QVERIFY(visual_texts(empty)
                .join(QLatin1Char('\n'))
                .contains(QStringLiteral("Mining History Empty")));
    QVERIFY(visual_texts(empty)
                .join(QLatin1Char('\n'))
                .contains(QStringLiteral("Mined video subtitles will appear here.")));

    // A zero limit is the disabled state, distinct from an empty history.
    QVERIFY(niratan::player::store_video_settings(
        niratan::storage::data_root(), {{QStringLiteral("videoMiningHistoryLimit"), 0}}));
    model.refreshHistory();
    QTRY_COMPARE(model.history_limit(), 0);
    QTRY_VERIFY(item_named(sidebar, QStringLiteral("historySearchField")) == nullptr);
    QTRY_COMPARE(items_named(sidebar, QStringLiteral("historySectionHeader")).size(), qsizetype(0));
    QTRY_VERIFY(visual_texts(sidebar)
                    .join(QLatin1Char('\n'))
                    .contains(QStringLiteral("Mining History is disabled in Video Settings.")));
}

// B6: PlayerPage captures the play state before a jump and restores it after
// the resolved seek, including when the jump opens another video.
void PlayerControlsTest::history_jump_preserves_pause() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());
    const QString other = touch_file(root.path() + QStringLiteral("/Anime/Beta.mp4"), 10);
    QVERIFY(!other.isEmpty());

    auto store = niratan::player::MiningHistoryStore::at_data_root();
    QVERIFY(store.clear());
    const auto record = [](const char* id, double created_at, const QString& file_name,
                           const QString& path, double cue_start) {
        niratan::player::MiningHistoryItem item;
        item.id = id;
        item.created_at = created_at;
        item.subtitle_text = "cue";
        item.video_file_name = file_name.toStdString();
        item.video_title = item.video_file_name;
        item.subtitle_source_name = item.video_file_name;
        if (!path.isEmpty()) {
            item.video_path = path.toStdString();
        }
        item.cue_start = cue_start;
        item.cue_end = cue_start + 1.0;
        return item;
    };
    QVERIFY(store.add(record("same", 100.0, QFileInfo(video).fileName(), QString(), 12.5), 25));
    QVERIFY(store.add(record("other", 200.0, QFileInfo(other).fileName(),
                             QFileInfo(other).canonicalFilePath(), 3.0),
                      25));

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY(player != nullptr);
    auto* study = player->findChild<niratan::viewmodels::PlayerStudyViewModel*>();
    QVERIFY(study != nullptr);
    study->refreshHistory();
    study->setVideo(QUrl::fromLocalFile(video));
    study->setPlaybackReady(true);
    study->updatePlayback(0.0, 0.0);
    QQuickItem* mpv = item_named(video_window->contentItem(), QStringLiteral("mpvItem"));
    QVERIFY(mpv != nullptr);
    QSignalSpy requests(study,
                        &niratan::viewmodels::PlayerStudyViewModel::historySubtitleRequested);

    // A jump inside the current video while paused stays paused after its seek.
    mpv->setProperty("paused", true);
    QVERIFY(QMetaObject::invokeMethod(player, "jumpToHistory",
                                      Q_ARG(QVariant, QVariant(QStringLiteral("same")))));
    QCOMPARE(requests.size(), 1);
    QVERIFY(mpv->property("paused").toBool());

    // A jump to another video captures the state before the open and restores
    // it after the new video's resolved seek.
    requests.clear();
    mpv->setProperty("paused", true);
    QVERIFY(QMetaObject::invokeMethod(player, "jumpToHistory",
                                      Q_ARG(QVariant, QVariant(QStringLiteral("other")))));
    QCOMPARE(player->property("historyJumpWasPlaying").toBool(), false);
    QCOMPARE(requests.size(), 0);
    study->setPlaybackReady(true);
    QCOMPARE(requests.size(), 1);
    QVERIFY(mpv->property("paused").toBool());
}

// B2: Save Clean Screenshot sits after Inspector and before Full Screen at
// every density, follows canSaveScreenshot and emits its request.
void PlayerControlsTest::screenshot_control_is_available_at_every_density() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object =
        create_player_component(engine, QStringLiteral("VideoUtilityControls"),
                                {{QStringLiteral("density"), QStringLiteral("full")}}, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* utility = attach(window, object.get());
    QVERIFY(utility != nullptr);

    for (const QString& density :
         {QStringLiteral("full"), QStringLiteral("condensed"), QStringLiteral("minimal")}) {
        utility->setProperty("density", density);
        QQuickItem* button = item_named(utility, QStringLiteral("screenshotButton"));
        QVERIFY2(button != nullptr, qPrintable(density));
        QTRY_VERIFY2(button->isVisible(), qPrintable(density));
        QCOMPARE(button->property("tip").toString(), QStringLiteral("Save Clean Screenshot"));
        QVERIFY(!button->property("enabled").toBool());
    }

    QQuickItem* button = item_named(utility, QStringLiteral("screenshotButton"));
    utility->setProperty("density", QStringLiteral("full"));
    utility->setProperty("canSaveScreenshot", true);
    QTRY_VERIFY(button->property("enabled").toBool());
    QSignalSpy request_spy(utility, SIGNAL(saveScreenshotRequested()));
    click(button);
    QCOMPARE(request_spy.count(), 1);

    const QStringList order =
        named_in_order(utility,
                       {QStringLiteral("inspectorButton"), QStringLiteral("screenshotButton"),
                        QStringLiteral("fullScreenButton")});
    QCOMPARE(order,
             QStringList({QStringLiteral("inspectorButton"), QStringLiteral("screenshotButton"),
                          QStringLiteral("fullScreenButton")}));
}

// B1: the first media file wins and binds the first subtitle; a subtitle
// alone loads into the open video; other and remote files do nothing.
void PlayerControlsTest::page_drop_picks_the_first_media_and_subtitle() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());
    const QString folder = QFileInfo(video).absolutePath();
    const QString other_video = touch_file(folder + QStringLiteral("/Beta.MKV"), 10);
    const QString subtitle = touch_file(folder + QStringLiteral("/Episode.SRT"), 10);
    const QString other = touch_file(folder + QStringLiteral("/notes.txt"), 10);
    QVERIFY(!other_video.isEmpty() && !subtitle.isEmpty() && !other.isEmpty());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");

    const QVariantList drop{QVariant(QUrl::fromLocalFile(subtitle)),
                            QVariant(QUrl::fromLocalFile(other)),
                            QVariant(QUrl::fromLocalFile(other_video))};
    QVERIFY(
        QMetaObject::invokeMethod(player, "handleDroppedUrls", Q_ARG(QVariant, QVariant(drop))));
    QTRY_COMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(other_video));
    QTRY_COMPARE(player->property("pendingSubtitleUrl").toUrl(), QUrl::fromLocalFile(subtitle));

    // A subtitle alone stays on the current video instead of opening media.
    const QVariantList subtitle_only{QVariant(QUrl::fromLocalFile(subtitle))};
    QVERIFY(QMetaObject::invokeMethod(player, "handleDroppedUrls",
                                      Q_ARG(QVariant, QVariant(subtitle_only))));
    QCOMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(other_video));

    // An unrelated file leaves the page alone.
    const QVariantList unrelated{QVariant(QUrl::fromLocalFile(other))};
    QVERIFY(QMetaObject::invokeMethod(player, "handleDroppedUrls",
                                      Q_ARG(QVariant, QVariant(unrelated))));
    QCOMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(other_video));

    // A remote URL is not a dropped file.
    const QVariantList remote{
        QVariant(QUrl(QStringLiteral("https://example.com/Episode%2001.mp4")))};
    QVERIFY(
        QMetaObject::invokeMethod(player, "handleDroppedUrls", Q_ARG(QVariant, QVariant(remote))));
    QCOMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(other_video));

    // With no video open a dropped subtitle does nothing.
    QVERIFY(QMetaObject::invokeMethod(player, "unloadVideo"));
    QCOMPARE(player->property("videoFile").toUrl(), QUrl());
    QVERIFY(QMetaObject::invokeMethod(player, "handleDroppedUrls",
                                      Q_ARG(QVariant, QVariant(subtitle_only))));
    QCOMPARE(player->property("videoFile").toUrl(), QUrl());
}

// B3: VideoVolumeScrollDelta.adjustment through the page: ±5 per notch, half
// a pixel delta capped at ±5, the 0.1 and 0.01 floors, horizontal dominance,
// the 0...100 clamp, the Volume OSD and the Inspector exclusion.
void PlayerControlsTest::page_wheel_volume_follows_the_scroll_delta() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QQuickItem* mpv = item_named(video_window->contentItem(), QStringLiteral("mpvItem"));
    QVERIFY(mpv != nullptr);

    const auto scroll = [player](double x, double y, double dx, double dy, bool precise) {
        QVariant result;
        if (!QMetaObject::invokeMethod(player, "handleVolumeScroll", Q_RETURN_ARG(QVariant, result),
                                       Q_ARG(QVariant, x), Q_ARG(QVariant, y), Q_ARG(QVariant, dx),
                                       Q_ARG(QVariant, dy), Q_ARG(QVariant, precise))) {
            return false;
        }
        return result.toBool();
    };

    mpv->setProperty("volume", 50.0);
    QVERIFY(scroll(10, 10, 0, 120, false));
    QTRY_COMPARE(mpv->property("volume").toDouble(), 55.0);
    QCOMPARE(player->property("osdTitle").toString(), QStringLiteral("Volume"));
    QCOMPARE(player->property("osdValue").toString(), QStringLiteral("55"));

    QVERIFY(scroll(10, 10, 0, -120, false));
    QTRY_COMPARE(mpv->property("volume").toDouble(), 50.0);

    QVERIFY(scroll(10, 10, 0, 4, true));
    QTRY_COMPARE(mpv->property("volume").toDouble(), 52.0);
    QVERIFY(!scroll(10, 10, 0, 0.1, true));
    QCOMPARE(mpv->property("volume").toDouble(), 52.0);
    QVERIFY(scroll(10, 10, 0, 100, true));
    QTRY_COMPARE(mpv->property("volume").toDouble(), 57.0);
    QVERIFY(scroll(10, 10, 0, -100, true));
    QTRY_COMPARE(mpv->property("volume").toDouble(), 52.0);

    QVERIFY(!scroll(10, 10, 5, 4, true));
    QCOMPARE(mpv->property("volume").toDouble(), 52.0);
    QVERIFY(!scroll(10, 10, 0, 0.005, true));
    QCOMPARE(mpv->property("volume").toDouble(), 52.0);

    mpv->setProperty("volume", 99.0);
    QVERIFY(scroll(10, 10, 0, 120, false));
    QTRY_COMPARE(mpv->property("volume").toDouble(), 100.0);
    mpv->setProperty("volume", 1.0);
    QVERIFY(scroll(10, 10, 0, -120, false));
    QTRY_COMPARE(mpv->property("volume").toDouble(), 0.0);

    // The open Inspector owns its area.
    player->setProperty("inspectorOpen", true);
    QQuickItem* inspector =
        item_named(video_window->contentItem(), QStringLiteral("videoInspectorPanel"));
    QVERIFY(inspector != nullptr);
    QTRY_VERIFY(inspector->isVisible());
    QTRY_VERIFY(inspector->width() > 0 && inspector->height() > 0);
    const QPointF inside = inspector->mapToItem(player, inspector->boundingRect().center());
    mpv->setProperty("volume", 50.0);
    QVERIFY(!scroll(inside.x(), inside.y(), 0, 120, false));
    QCOMPARE(mpv->property("volume").toDouble(), 50.0);

    // No video open means the stage does not take the wheel.
    QVERIFY(QMetaObject::invokeMethod(player, "unloadVideo"));
    QVERIFY(!scroll(10, 10, 0, 120, false));
    QCOMPARE(mpv->property("volume").toDouble(), 50.0);
}

// B4: the capsule follows the pointer inside the track, clears on exit, and
// lingers for eight seconds after a scrub or a scheduled hide.
void PlayerControlsTest::timeline_preview_capsule_follows_hover_and_linger() {
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> object = create_player_component(
        engine, QStringLiteral("VideoProgressStrip"), {{QStringLiteral("duration"), 100.0}}, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    QQuickWindow window;
    QQuickItem* strip = attach(window, object.get());
    QVERIFY(strip != nullptr);
    strip->setWidth(800);
    strip->setHeight(window.height());
    QQuickItem* timeline = item_named(strip, QStringLiteral("timeline"));
    QVERIFY(timeline != nullptr);
    QTRY_VERIFY(timeline->width() > 0);

    QQuickItem* bubble = item_named(strip, QStringLiteral("timelinePreviewBubble"));
    QVERIFY2(bubble != nullptr, "the timeline preview capsule is missing");
    QVERIFY(!bubble->isVisible());
    QCOMPARE(bubble->property("objectName").toString(), QStringLiteral("timelinePreviewBubble"));

    const double width = timeline->width();
    QVERIFY(QMetaObject::invokeMethod(strip, "handleProgressHover", Q_ARG(QVariant, width / 2)));
    QTRY_VERIFY(strip->property("previewActive").toBool());
    QTRY_VERIFY(bubble->isVisible());
    QCOMPARE(bubble->property("text").toString(), QStringLiteral("0:50"));
    QVERIFY(bubble->x() >= 0.0);
    QVERIFY(bubble->x() + bubble->width() <= width + 0.5);

    // The bubble is clamped inside the track at both ends.
    QVERIFY(QMetaObject::invokeMethod(strip, "handleProgressHover", Q_ARG(QVariant, width + 400)));
    QTRY_COMPARE(bubble->property("text").toString(), QStringLiteral("1:40"));
    QVERIFY(bubble->x() + bubble->width() <= width + 0.5);

    QVERIFY(QMetaObject::invokeMethod(strip, "handleProgressExit"));
    QTRY_VERIFY(!strip->property("previewActive").toBool());
    QTRY_VERIFY(!bubble->isVisible());

    // A scheduled hide keeps the capsule and clears it when the timer fires.
    QVERIFY(QMetaObject::invokeMethod(strip, "updateProgressPreview", Q_ARG(QVariant, 25)));
    QTRY_VERIFY(bubble->isVisible());
    QCOMPARE(bubble->property("text").toString(), QStringLiteral("0:25"));
    QVERIFY(QMetaObject::invokeMethod(strip, "scheduleProgressPreviewHide"));
    QObject* timer = strip->findChild<QObject*>(QStringLiteral("timelinePreviewHideTimer"));
    QVERIFY(timer != nullptr);
    QTRY_VERIFY(timer->property("running").toBool());
    QCOMPARE(timer->property("interval").toInt(), 8000);
    QVERIFY(QMetaObject::invokeMethod(strip, "handleProgressExit"));
    QVERIFY(strip->property("previewActive").toBool());
    timer->setProperty("interval", 30);
    QTRY_VERIFY(!strip->property("previewActive").toBool());
    QTRY_VERIFY(!bubble->isVisible());

    // The scrub bubble owns the capsule while dragging; the preview returns
    // after the scrub ends.
    QVERIFY(QMetaObject::invokeMethod(strip, "updateProgressPreview", Q_ARG(QVariant, 30)));
    strip->setProperty("scrubbing", true);
    QTRY_VERIFY(!bubble->isVisible());
    strip->setProperty("scrubbing", false);
    QTRY_VERIFY(bubble->isVisible());

    // No preview with an unknown duration.
    QVERIFY(QMetaObject::invokeMethod(strip, "clearTimelinePreview"));
    strip->setProperty("duration", 0.0);
    QVERIFY(QMetaObject::invokeMethod(strip, "handleProgressHover", Q_ARG(QVariant, width / 2)));
    QVERIFY(!strip->property("previewActive").toBool());
    QVERIFY(!bubble->isVisible());
}

// B2: the save panel's default file name is the video stem with ":" replaced
// by "-", then the current seconds with three decimals.
void PlayerControlsTest::screenshot_name_follows_swift() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");

    QVariant name;
    QVERIFY(QMetaObject::invokeMethod(player, "screenshotFileName", Q_RETURN_ARG(QVariant, name),
                                      Q_ARG(QVariant, 65.5)));
    QCOMPARE(name.toString(), QStringLiteral("Alpha-65.500.png"));
    QVERIFY(QMetaObject::invokeMethod(player, "screenshotFileName", Q_RETURN_ARG(QVariant, name),
                                      Q_ARG(QVariant, 12.3456)));
    QCOMPARE(name.toString(), QStringLiteral("Alpha-12.346.png"));

    player->setProperty("videoFile",
                        QUrl::fromLocalFile(QFileInfo(video).absolutePath() +
                                            QStringLiteral("/Alpha: Part 2.mkv")));
    QVERIFY(QMetaObject::invokeMethod(player, "screenshotFileName", Q_RETURN_ARG(QVariant, name),
                                      Q_ARG(QVariant, 0.0)));
    QCOMPARE(name.toString(), QStringLiteral("Alpha- Part 2-0.000.png"));

    QObject* dialog = video_window->findChild<QObject*>(QStringLiteral("screenshotFileDialog"));
    QVERIFY2(dialog != nullptr, "the screenshot save panel is missing");
    QCOMPARE(dialog->property("fileMode").toInt(), 2);
    QCOMPARE(dialog->property("defaultSuffix").toString(), QStringLiteral("png"));
    QVERIFY(!player->property("canSaveScreenshot").toBool());
}

// B2: a write that cannot reach the destination still discards the temporary
// capture, so a failed save leaves no stray file.
void PlayerControlsTest::failed_screenshot_discards_the_capture() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");

    QObject* controller = video_window->findChild<QObject*>(QStringLiteral("playerScreenshot"));
    QVERIFY2(controller != nullptr, "the screenshot controller is missing");
    const QString capture = touch_file(root.path() + QStringLiteral("/capture.png"), 16);
    QVERIFY(!capture.isEmpty());
    QVERIFY(controller->setProperty("pendingCapture", QUrl::fromLocalFile(capture)));
    QSignalSpy failed_spy(controller, SIGNAL(failed()));

    const QString missing = QFileInfo(capture).absolutePath() + QStringLiteral("/missing/out.png");
    QVERIFY(!QFileInfo(QFileInfo(missing).absolutePath()).exists());
    QVERIFY(QMetaObject::invokeMethod(controller, "write",
                                      Q_ARG(QVariant, QVariant(QUrl::fromLocalFile(missing)))));
    QCOMPARE(failed_spy.count(), 1);
    QVERIFY2(!QFile::exists(capture), "the failed save leaked the temporary capture");
    QVERIFY(!QFile::exists(missing));
    QVERIFY(controller->property("pendingCapture").toUrl().isEmpty());
}

// B6: in full screen the study sidebar ignores the video aspect ratio and
// keeps its clamped stored width.
void PlayerControlsTest::full_screen_sidebar_keeps_the_stored_width() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString video = make_video(root.path());

    QQmlApplicationEngine engine;
    QQuickWindow* video_window = nullptr;
    QQuickItem* player = open_video_in_window(engine, video, &video_window);
    QVERIFY2(player != nullptr, "the Video window did not open the player");
    QQuickItem* mpv = item_named(video_window->contentItem(), QStringLiteral("mpvItem"));
    QVERIFY(mpv != nullptr);
    QVERIFY(mpv->setProperty("aspectRatio", QStringLiteral("16:9")));
    QTRY_COMPARE(player->property("videoAspectRatio").toDouble(), 16.0 / 9.0);

    player->setProperty("studyOpen", true);
    QQuickItem* sidebar = item_named(video_window->contentItem(), QStringLiteral("studySidebar"));
    QVERIFY(sidebar != nullptr);
    QTRY_VERIFY(sidebar->isVisible());

    QVERIFY(QMetaObject::invokeMethod(player, "toggleFullScreen"));
    QTRY_COMPARE(int(video_window->visibility()), int(QWindow::FullScreen));
    QTRY_VERIFY(player->property("fullScreen").toBool());
    QTRY_COMPARE(player->property("studySidebarLayoutWidth").toDouble(), 340.0);
    QCOMPARE(sidebar->width(), 340.0);
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

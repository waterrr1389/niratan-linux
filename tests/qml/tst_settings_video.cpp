// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Video settings page, following Features/Settings/VideoSettingsView.swift
// body and Core/UserConfig.swift: the Playback, Video Enhancement, Mining and
// Subtitle Mask cards, their labels, values, ranges, footers and stored keys;
// the Video rail entry from NativeMac/NativeReuseViews.swift
// NativeSettingsReuseView.settingsSidebar; and the live reload chain from an
// open Settings page to an open PlayerPage (VideoPlayerScreen reading
// UserConfig live, MpvItem.reloadVideoSettings).
//
// Runs offscreen like tests/qml/tst_settings_appearance.cpp, with a fresh
// data root per test because every case writes settings.json.

#include "../viewmodels/data_root.h"
#include "services/player/mining_history.h"
#include "services/player/mining_history_store.h"
#include "services/player/video_settings.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"
#include "viewmodels/player/player_study_view_model.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest>

#include <clocale>
#include <memory>

namespace {

QQuickItem* find_item(QQuickItem* root, const QString& name) {
    if (root == nullptr) {
        return nullptr;
    }
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = find_item(child, name)) {
            return found;
        }
    }
    return nullptr;
}

QList<QQuickItem*> all_items(QQuickItem* root) {
    QList<QQuickItem*> items;
    if (root == nullptr) {
        return items;
    }
    items.append(root);
    for (QQuickItem* child : root->childItems()) {
        items.append(all_items(child));
    }
    return items;
}

QList<QQuickItem*> items_named(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> found;
    for (QQuickItem* item : all_items(root)) {
        if (item->objectName() == name) {
            found.append(item);
        }
    }
    return found;
}

QStringList visible_texts(QQuickItem* root) {
    QStringList texts;
    for (QQuickItem* item : all_items(root)) {
        if (!item->isVisible()) {
            continue;
        }
        const QVariant text = item->property("text");
        if (text.isValid() && !text.toString().isEmpty()) {
            texts.append(text.toString());
        }
    }
    return texts;
}

QJsonObject stored_settings() {
    const auto read =
        niratan::storage::read_json_file(niratan::storage::data_root() / "settings.json");
    return read.document.object();
}

void click(QQuickItem* item) {
    QVERIFY(item != nullptr);
    QVERIFY(QMetaObject::invokeMethod(item, "clicked"));
}

QQuickItem* form_of(QQuickWindow* window) {
    return find_item(window->contentItem(), QStringLiteral("settings-video"));
}

std::unique_ptr<QObject> create_form(QQmlApplicationEngine& engine, int height,
                                     QQuickWindow*& window) {
    auto host = std::make_unique<QQuickWindow>();
    host->resize(900, height);
    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/settings/VideoSettings.qml")));
    std::unique_ptr<QObject> object(component.beginCreate(engine.rootContext()));
    auto* form = qobject_cast<QQuickItem*>(object.get());
    if (form) {
        form->setParentItem(host->contentItem());
        form->setSize(QSizeF(host->size()));
    }
    if (object) {
        component.completeCreate();
    }
    if (!form || component.isError()) {
        return {};
    }
    object.release()->setParent(host.get());
    host->show();
    if (!QTest::qWaitForWindowExposed(host.get())) {
        return {};
    }
    window = host.get();
    return host;
}

std::unique_ptr<QObject> create_main_window(QQmlApplicationEngine& engine, int height,
                                            QQuickWindow*& window) {
    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    if (component.isError()) {
        return {};
    }
    std::unique_ptr<QObject> object(
        component.createWithInitialProperties({{QStringLiteral("sectionIndex"), 0}}));
    window = qobject_cast<QQuickWindow*>(object.get());
    if (window == nullptr) {
        return {};
    }
    window->resize(1000, height);
    if (!QTest::qWaitForWindowExposed(window)) {
        return {};
    }
    return object;
}

QString touch_video(const QString& root) {
    QDir().mkpath(root + QStringLiteral("/video"));
    const QString path = root + QStringLiteral("/video/Alpha.mp4");
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QByteArrayLiteral("fake"));
        file.close();
    }
    return QFileInfo(path).absoluteFilePath();
}

}  // namespace

class SettingsVideoTest : public QObject {
    Q_OBJECT

   private slots:
    void playback_card_follows_swift();
    void enhancement_and_equalizer_follow_swift();
    void mining_and_mask_follow_swift();
    void rail_has_video_section();
    void video_edits_reach_an_open_player();
};

// B1-B3: the Playback card's toggles, speed row, layout picker and seek
// interval write their keys and show Swift's values and footer.
void SettingsVideoTest::playback_card_follows_swift() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());
    QVERIFY(niratan::storage::write_json_file(
        niratan::storage::data_root() / "settings.json",
        QJsonDocument(QJsonObject{{"other", 42},
                                  {"videoSeekInterval", 12},
                                  {"videoRememberPlaybackPosition", false},
                                  {"videoAutoPauseOnLookup", false}})));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_form(engine, 1600, window);
    QVERIFY2(object != nullptr, "VideoSettings was not created");
    QQuickItem* form = form_of(window);
    QVERIFY(form);
    QTest::qWait(50);

    auto* playback = find_item(form, QStringLiteral("videoPlaybackSection"));
    QVERIFY(playback);
    QCOMPARE(playback->property("footer").toString(),
             QStringLiteral("Restores playback position, subtitles, speed, timing, and audio "
                            "track for each video."));

    const QList<QPair<QString, QString>> toggles{
        {QStringLiteral("videoAutoPlayNextToggle"), QStringLiteral("videoAutoPlayNext")},
        {QStringLiteral("videoRememberPlaybackToggle"),
         QStringLiteral("videoRememberPlaybackPosition")},
        {QStringLiteral("videoAutoPauseToggle"), QStringLiteral("videoAutoPauseOnLookup")},
        {QStringLiteral("videoGapFastForwardToggle"),
         QStringLiteral("videoSubtitleGapFastForwardEnabled")},
    };
    for (const auto& entry : toggles) {
        QQuickItem* toggle = find_item(form, entry.first);
        QVERIFY2(toggle != nullptr, qPrintable(entry.first));
        QVERIFY(toggle->isVisible());
    }
    QVERIFY(
        find_item(form, QStringLiteral("videoAutoPlayNextToggle"))->property("checked").toBool());
    QCOMPARE(find_item(form, QStringLiteral("videoRememberPlaybackToggle"))
                 ->property("checked")
                 .toBool(),
             false);
    QCOMPARE(find_item(form, QStringLiteral("videoAutoPauseToggle"))->property("checked").toBool(),
             false);
    QCOMPARE(
        find_item(form, QStringLiteral("videoGapFastForwardToggle"))->property("checked").toBool(),
        false);
    QCOMPARE(find_item(form, QStringLiteral("videoSeekInterval"))->property("value").toInt(), 12);

    QVERIFY(QMetaObject::invokeMethod(find_item(form, QStringLiteral("videoAutoPlayNextToggle")),
                                      "toggled", Q_ARG(bool, false)));
    QVERIFY(!stored_settings().value(QStringLiteral("videoAutoPlayNext")).toBool());
    QVERIFY(
        QMetaObject::invokeMethod(find_item(form, QStringLiteral("videoRememberPlaybackToggle")),
                                  "toggled", Q_ARG(bool, true)));
    QVERIFY(stored_settings().value(QStringLiteral("videoRememberPlaybackPosition")).toBool());
    QVERIFY(QMetaObject::invokeMethod(find_item(form, QStringLiteral("videoAutoPauseToggle")),
                                      "toggled", Q_ARG(bool, true)));
    QVERIFY(stored_settings().value(QStringLiteral("videoAutoPauseOnLookup")).toBool());
    QCOMPARE(stored_settings().value(QStringLiteral("other")).toInt(), 42);
    QCOMPARE(stored_settings().value(QStringLiteral("videoSeekInterval")).toInt(), 12);

    QQuickItem* speed = find_item(form, QStringLiteral("videoGapFastForwardSpeed"));
    QVERIFY(speed);
    QCOMPARE(speed->property("subtitle").toString(),
             QStringLiteral("Temporarily speeds through gaps between subtitle lines."));
    QCOMPARE(speed->property("value").toDouble(), 2.7);
    QCOMPARE(speed->property("from").toDouble(), 1.1);
    QCOMPARE(speed->property("to").toDouble(), 5.0);
    QCOMPARE(speed->property("stepSize").toDouble(), 0.1);
    QCOMPARE(speed->property("valueText").toString(), QStringLiteral("2.7x"));
    QVERIFY(!speed->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(find_item(form, QStringLiteral("videoGapFastForwardToggle")),
                                      "toggled", Q_ARG(bool, true)));
    QVERIFY(stored_settings().value(QStringLiteral("videoSubtitleGapFastForwardEnabled")).toBool());
    QTRY_VERIFY(speed->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(speed, "stepped", Q_ARG(double, 3.34)));
    QTRY_COMPARE(speed->property("value").toDouble(), 3.3);
    QCOMPARE(speed->property("valueText").toString(), QStringLiteral("3.3x"));
    QCOMPARE(stored_settings().value(QStringLiteral("videoSubtitleGapFastForwardSpeed")).toDouble(),
             3.3);

    QQuickItem* layout = find_item(form, QStringLiteral("videoControlBarLayout"));
    QVERIFY(layout);
    QCOMPARE(layout->property("model").toStringList(),
             (QStringList{QStringLiteral("Floating"), QStringLiteral("Compact Bottom")}));
    QCOMPARE(layout->property("currentIndex").toInt(), 0);
    QVERIFY(QMetaObject::invokeMethod(layout, "activated", Q_ARG(int, 1)));
    QTRY_COMPARE(stored_settings().value(QStringLiteral("videoControlBarLayout")).toString(),
                 QStringLiteral("compactBottom"));
    QCOMPARE(layout->property("currentIndex").toInt(), 1);

    QQuickItem* seek = find_item(form, QStringLiteral("videoSeekInterval"));
    QVERIFY(seek);
    QCOMPARE(seek->property("from").toDouble(), 1.0);
    QCOMPARE(seek->property("to").toDouble(), 60.0);
    QCOMPARE(seek->property("stepSize").toDouble(), 1.0);
    QCOMPARE(seek->property("valueText").toString(), QStringLiteral("12 s"));
    QVERIFY(QMetaObject::invokeMethod(seek, "stepped", Q_ARG(double, 30)));
    QTRY_COMPARE(stored_settings().value(QStringLiteral("videoSeekInterval")).toInt(), 30);
    QCOMPARE(seek->property("valueText").toString(), QStringLiteral("30 s"));
}

// B4: the enhancement toggles, five equalizer rows with their reset controls
// and Swift's footer; no Anime4K row.
void SettingsVideoTest::enhancement_and_equalizer_follow_swift() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_form(engine, 2200, window);
    QVERIFY2(object != nullptr, "VideoSettings was not created");
    QQuickItem* form = form_of(window);
    QVERIFY(form);
    QTest::qWait(50);

    auto* section = find_item(form, QStringLiteral("videoEnhancementSection"));
    QVERIFY(section);
    QCOMPARE(section->property("footer").toString(),
             QStringLiteral("Hardware decoding uses mpv's automatic safe decoder path. HDR "
                            "changes mpv tone-mapping peak handling."));
    QVERIFY(!visible_texts(form).join(QLatin1Char('\n')).contains(QStringLiteral("Anime4K")));

    QVERIFY(find_item(form, QStringLiteral("videoHardwareDecodingToggle"))
                ->property("checked")
                .toBool());
    QCOMPARE(
        find_item(form, QStringLiteral("videoDeinterlaceToggle"))->property("checked").toBool(),
        false);
    QCOMPARE(find_item(form, QStringLiteral("videoHdrToggle"))->property("checked").toBool(),
             false);

    QVERIFY(
        QMetaObject::invokeMethod(find_item(form, QStringLiteral("videoHardwareDecodingToggle")),
                                  "toggled", Q_ARG(bool, false)));
    QVERIFY(!stored_settings().value(QStringLiteral("videoHardwareDecodingEnabled")).toBool());
    QVERIFY(QMetaObject::invokeMethod(find_item(form, QStringLiteral("videoDeinterlaceToggle")),
                                      "toggled", Q_ARG(bool, true)));
    QVERIFY(stored_settings().value(QStringLiteral("videoDeinterlacingEnabled")).toBool());
    QVERIFY(QMetaObject::invokeMethod(find_item(form, QStringLiteral("videoHdrToggle")), "toggled",
                                      Q_ARG(bool, true)));
    QVERIFY(stored_settings().value(QStringLiteral("videoHDREnhancementEnabled")).toBool());

    const QList<QQuickItem*> equalizers = items_named(form, QStringLiteral("videoEqualizer"));
    QCOMPARE(equalizers.size(), 5);
    const QStringList keys{QStringLiteral("videoBrightness"), QStringLiteral("videoContrast"),
                           QStringLiteral("videoSaturation"), QStringLiteral("videoGamma"),
                           QStringLiteral("videoHue")};
    const QStringList labels{QStringLiteral("Brightness"), QStringLiteral("Contrast"),
                             QStringLiteral("Saturation"), QStringLiteral("Gamma"),
                             QStringLiteral("Hue")};
    const QList<int> values{40, -40, 20, -20, 15};
    for (qsizetype i = 0; i < equalizers.size(); ++i) {
        QCOMPARE(equalizers.at(i)->property("settingKey").toString(), keys.at(i));
        QCOMPARE(equalizers.at(i)->property("label").toString(), labels.at(i));
        QCOMPARE(equalizers.at(i)->property("from").toDouble(), -100.0);
        QCOMPARE(equalizers.at(i)->property("to").toDouble(), 100.0);
        QCOMPARE(equalizers.at(i)->property("stepSize").toDouble(), 1.0);
        QVERIFY(QMetaObject::invokeMethod(equalizers.at(i), "moved", Q_ARG(double, values.at(i))));
        QTRY_COMPARE(equalizers.at(i)->property("value").toInt(), values.at(i));
        QCOMPARE(equalizers.at(i)->property("valueText").toString(), QString::number(values.at(i)));
        QTRY_COMPARE(stored_settings().value(keys.at(i)).toInt(), values.at(i));
    }

    for (qsizetype i = 0; i < equalizers.size(); ++i) {
        QQuickItem* reset = nullptr;
        for (QQuickItem* candidate : items_named(form, QStringLiteral("videoEqualizerReset"))) {
            if (candidate->property("settingKey").toString() == keys.at(i)) {
                reset = candidate;
            }
        }
        QVERIFY2(reset != nullptr, qPrintable(keys.at(i)));
        click(reset);
        QTRY_COMPARE(stored_settings().value(keys.at(i)).toInt(), 0);
        QCOMPARE(equalizers.at(i)->property("value").toInt(), 0);
    }
}

// B5, B6: the Mining stepper and footer, and the mask toggle, mode and the one
// mode-dependent slider with its own footer.
void SettingsVideoTest::mining_and_mask_follow_swift() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_form(engine, 2400, window);
    QVERIFY2(object != nullptr, "VideoSettings was not created");
    QQuickItem* form = form_of(window);
    QVERIFY(form);
    QTest::qWait(50);

    auto* mining = find_item(form, QStringLiteral("videoMiningSection"));
    QVERIFY(mining);
    QCOMPARE(mining->property("footer").toString(),
             QStringLiteral("Set to 0 to disable and clear Mining History."));
    QQuickItem* limit = find_item(form, QStringLiteral("videoMiningLimit"));
    QVERIFY(limit);
    QCOMPARE(limit->property("label").toString(), QStringLiteral("Mining History Storage Limit"));
    QCOMPARE(limit->property("value").toInt(), 25);
    QCOMPARE(limit->property("from").toInt(), 0);
    QCOMPARE(limit->property("to").toInt(), 1000);
    QCOMPARE(limit->property("stepSize").toInt(), 1);
    QCOMPARE(limit->property("valueText").toString(), QStringLiteral("25"));
    QVERIFY(QMetaObject::invokeMethod(limit, "stepped", Q_ARG(double, 0)));
    QTRY_COMPARE(stored_settings().value(QStringLiteral("videoMiningHistoryLimit")).toInt(), 0);

    auto* mask = find_item(form, QStringLiteral("videoMaskSection"));
    QVERIFY(mask);
    QCOMPARE(mask->property("footer").toString(),
             QStringLiteral("Masked subtitles are shown normally while the pointer is over the "
                            "subtitle row."));
    QQuickItem* mask_toggle = find_item(form, QStringLiteral("videoMaskToggle"));
    QQuickItem* mode = find_item(form, QStringLiteral("videoMaskMode"));
    QQuickItem* blur = find_item(form, QStringLiteral("videoMaskBlurRadius"));
    QQuickItem* opacity = find_item(form, QStringLiteral("videoMaskHiddenOpacity"));
    QQuickItem* reset = find_item(form, QStringLiteral("videoEqualizerReset"));
    QVERIFY(mask_toggle);
    QVERIFY(mode);
    QVERIFY(blur);
    QVERIFY(opacity);
    QVERIFY(reset);
    QCOMPARE(mask_toggle->property("checked").toBool(), false);
    QVERIFY(!mode->property("enabled").toBool());
    QCOMPARE(mode->property("model").toStringList(),
             (QStringList{QStringLiteral("Blur"), QStringLiteral("Transparent")}));
    QVERIFY(blur->isVisible());
    QVERIFY(!opacity->isVisible());
    QCOMPARE(blur->property("value").toInt(), 10);
    QCOMPARE(blur->property("from").toInt(), 0);
    QCOMPARE(blur->property("to").toInt(), 20);
    QCOMPARE(blur->property("stepSize").toInt(), 1);
    QCOMPARE(blur->property("valueText").toString(), QStringLiteral("10 px"));

    QVERIFY(QMetaObject::invokeMethod(mask_toggle, "toggled", Q_ARG(bool, true)));
    QVERIFY(stored_settings().value(QStringLiteral("videoSubtitleMaskEnabled")).toBool());
    QTRY_VERIFY(mode->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(mode, "activated", Q_ARG(int, 1)));
    QTRY_COMPARE(stored_settings().value(QStringLiteral("videoSubtitleMaskMode")).toString(),
                 QStringLiteral("Transparent"));
    QTRY_VERIFY(opacity->isVisible());
    QVERIFY(!blur->isVisible());
    QCOMPARE(opacity->property("value").toDouble(), 0.0);
    QCOMPARE(opacity->property("from").toDouble(), 0.0);
    QCOMPARE(opacity->property("to").toDouble(), 1.0);
    QCOMPARE(opacity->property("stepSize").toDouble(), 0.05);
    QVERIFY(QMetaObject::invokeMethod(opacity, "moved", Q_ARG(double, 0.58)));
    QTRY_COMPARE(
        stored_settings().value(QStringLiteral("videoSubtitleMaskHiddenOpacity")).toDouble(), 0.6);
    QCOMPARE(opacity->property("valueText").toString(), QStringLiteral("60%"));

    QVERIFY(QMetaObject::invokeMethod(mode, "activated", Q_ARG(int, 0)));
    QTRY_COMPARE(stored_settings().value(QStringLiteral("videoSubtitleMaskMode")).toString(),
                 QStringLiteral("Blur"));
    QTRY_VERIFY(blur->isVisible());
    QVERIFY(!opacity->isVisible());
    QVERIFY(QMetaObject::invokeMethod(blur, "moved", Q_ARG(double, 12)));
    QTRY_COMPARE(stored_settings().value(QStringLiteral("videoSubtitleMaskBlurRadius")).toInt(),
                 12);
    QCOMPARE(blur->property("valueText").toString(), QStringLiteral("12 px"));
}

// B7: the rail gains Video in its own caption after Word Audio and before
// Sync & Data, and selecting it shows the video form with the Video title.
void SettingsVideoTest::rail_has_video_section() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_main_window(engine, 800, window);
    QVERIFY2(object != nullptr, "MainWindow was not created");
    window->setProperty("sectionIndex", 3);
    QTRY_VERIFY(find_item(window->contentItem(), QStringLiteral("settingsRow-video")) != nullptr);

    QQuickItem* rail = find_item(window->contentItem(), QStringLiteral("settingsColumn"));
    QVERIFY(rail);
    const QVariantList sections = rail->property("sections").toList();
    int video_index = -1;
    for (int i = 0; i < sections.size(); ++i) {
        const QVariantMap section = sections.at(i).toMap();
        if (section.value(QStringLiteral("key")).toString() == QStringLiteral("video")) {
            video_index = i;
            QCOMPARE(section.value(QStringLiteral("title")).toString(), QStringLiteral("Video"));
            QCOMPARE(section.value(QStringLiteral("symbol")).toString(),
                     QStringLiteral("play.rectangle"));
        }
    }
    QCOMPARE(video_index, 7);
    QCOMPARE(sections.at(6).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("wordAudio"));
    QCOMPARE(sections.at(8).toMap().value(QStringLiteral("key")).toString(),
             QStringLiteral("sync"));

    auto* word_audio = find_item(rail, QStringLiteral("settingsRow-wordAudio"));
    auto* video_row = find_item(rail, QStringLiteral("settingsRow-video"));
    QVERIFY(word_audio);
    QVERIFY(video_row);
    const auto scene_top = [](QQuickItem* item) { return item->mapToScene(QPointF()).y(); };
    const double word_top = scene_top(word_audio);
    const double video_top = scene_top(video_row);
    bool caption_found = false;
    for (QQuickItem* item : all_items(rail)) {
        if (!item->isVisible() || item->property("text").toString() != QStringLiteral("Video")) {
            continue;
        }
        const double top = scene_top(item);
        if (top > word_top && top < video_top) {
            caption_found = true;
            QVERIFY2(QString::fromLatin1(item->metaObject()->className())
                         .contains(QStringLiteral("SourceListCaption")),
                     "the Video caption is not a SourceListCaption");
        }
    }
    QVERIFY2(caption_found, "the Video caption is not between Word Audio and Video");

    click(video_row);
    QQuickItem* form = nullptr;
    QTRY_VERIFY((form = find_item(window->contentItem(), QStringLiteral("settings-video"))) !=
                nullptr);
    QTRY_VERIFY(form->isVisible());
    QTRY_COMPARE(find_item(window->contentItem(), QStringLiteral("settingsTitle"))
                     ->property("text")
                     .toString(),
                 QStringLiteral("Video"));
    QVERIFY(form->property("height").toDouble() > 0);
    auto* flick = form->property("flickable").value<QQuickItem*>();
    QVERIFY(flick != nullptr);
    QVERIFY(flick->property("contentHeight").toDouble() > flick->height());
}

// B8: an edit on the open Settings page reaches an open player without
// reopening the video, and a mining limit of 0 clears its saved records.
void SettingsVideoTest::video_edits_reach_an_open_player() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());
    const QString video = touch_video(data.path());

    auto store = niratan::player::MiningHistoryStore::at_data_root();
    QVERIFY(store.clear());
    niratan::player::MiningHistoryItem item;
    item.id = "saved";
    item.created_at = 100.0;
    item.subtitle_text = "saved cue";
    item.video_file_name = "Alpha.mp4";
    item.video_title = "Alpha.mp4";
    item.subtitle_source_name = "Alpha.mp4";
    item.cue_start = 1.0;
    item.cue_end = 2.0;
    QVERIFY(store.add(item, 25));

    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_main_window(engine, 800, window);
    QVERIFY2(object != nullptr, "MainWindow was not created");
    auto* video_window = window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    QVERIFY(video_window != nullptr);
    QVERIFY(QMetaObject::invokeMethod(
        video_window, "openVideo", Q_ARG(QVariant, QVariant(QUrl::fromLocalFile(video))),
        Q_ARG(QVariant, QVariant(QUrl())), Q_ARG(QVariant, QVariant(false))));
    QQuickItem* player = nullptr;
    QTRY_VERIFY((player = video_window->findChild<QQuickItem*>(
                     QStringLiteral("videoPlayerPage"))) != nullptr);
    QQuickItem* mpv = nullptr;
    QTRY_VERIFY((mpv = find_item(video_window->contentItem(), QStringLiteral("mpvItem"))) !=
                nullptr);
    auto* study = player->findChild<niratan::viewmodels::PlayerStudyViewModel*>();
    QVERIFY(study != nullptr);
    QCOMPARE(study->history().size(), 1);

    window->setProperty("sectionIndex", 3);
    QTRY_VERIFY(find_item(window->contentItem(), QStringLiteral("settingsRow-video")) != nullptr);
    click(find_item(window->contentItem(), QStringLiteral("settingsRow-video")));
    QQuickItem* form = nullptr;
    QTRY_VERIFY((form = find_item(window->contentItem(), QStringLiteral("settings-video"))) !=
                nullptr);
    QQuickItem* limit = nullptr;
    QTRY_VERIFY((limit = find_item(form, QStringLiteral("videoMiningLimit"))) != nullptr);

    QVERIFY(QMetaObject::invokeMethod(limit, "stepped", Q_ARG(double, 0)));
    QTRY_COMPARE(stored_settings().value(QStringLiteral("videoMiningHistoryLimit")).toInt(), 0);
    QTRY_COMPARE(mpv->property("videoSettings")
                     .toMap()
                     .value(QStringLiteral("videoMiningHistoryLimit"))
                     .toInt(),
                 0);
    QTRY_VERIFY(niratan::player::MiningHistoryStore::at_data_root().load_snapshot().items.empty());
    QTRY_COMPARE(study->history_limit(), 0);
    QVERIFY(study->history().isEmpty());

    QQuickItem* layout = find_item(form, QStringLiteral("videoControlBarLayout"));
    QVERIFY(layout);
    QVERIFY(QMetaObject::invokeMethod(layout, "activated", Q_ARG(int, 1)));
    QTRY_COMPARE(mpv->property("videoSettings")
                     .toMap()
                     .value(QStringLiteral("videoControlBarLayout"))
                     .toString(),
                 QStringLiteral("compactBottom"));
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    SettingsVideoTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_settings_video.moc"

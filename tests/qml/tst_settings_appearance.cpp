// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Popup card of Features/Settings/AppearanceView.swift
// (nativeAppearanceContent): it is the last card, after Display, with Width,
// Height, Scale, the four toggles and the conditional Swipe Threshold row
// with its separator. Rows follow NativeSettingsSliderRow and
// NativeSettingsToggle, and the values are the popup fields of
// Core/UserConfig.swift.
//
// Runs offscreen like tests/qml/tst_qml_load.cpp: URL schemes are registered
// and QtWebEngineQuick initialized before the application object, and the
// Niratan style is selected before any component is created. The popup and
// colour tests write settings, so each of those builds a fresh data root
// through TempDataRoot.
//
// The colour tests follow the Theme card colour rows of
// AppearanceView.nativeAppearanceContent: Background Color, Text Color and
// Info Color appear after the Interface picker, each behind its separator,
// only while the reader theme is Custom. Each row is a ColorPicker whose
// swatch shows the UserConfig colour; the rows persist the three
// Core/UserConfig.swift properties.
//
// The layout and display cases follow AppearanceView.nativeAppearanceContent:
// Mouse Wheel Page Turn (247) sits between Two-Column Horizontal Pages and
// Horizontal Padding in both modes, and the Display card shows Show Statistics
// Toggle, Show Reading Speed and Show Reading Time after Progress Position
// only while enableStatistics is on, before Show Sasayaki Toggle. All four
// values persist in the profile's reader settings.

#include "../viewmodels/data_root.h"
#include "services/storage/app_paths.h"
#include "viewmodels/sasayaki/sasayaki_settings_view_model.h"
#include "viewmodels/settings/settings_view_model.h"
#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"

#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>
#include <filesystem>
#include <memory>

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

QString take_warnings() {
    QTest::qWait(50);
    const QString report = g_warnings.join('\n');
    g_warnings.clear();
    return report;
}

QList<QQuickItem*> visual_items(QQuickItem* item) {
    QList<QQuickItem*> items{item};
    for (QQuickItem* child : item->childItems()) {
        items.append(visual_items(child));
    }
    return items;
}

QQuickItem* find_item(QQuickItem* root, const QString& name) {
    for (QQuickItem* item : visual_items(root)) {
        if (item->objectName() == name) {
            return item;
        }
    }
    return nullptr;
}

QQuickItem* visible_item(QQuickItem* root, const QString& name) {
    QQuickItem* item = find_item(root, name);
    return item != nullptr && item->isVisible() ? item : nullptr;
}

// Scene top of the first visible item carrying text, or -1 when absent.
double text_top(QQuickItem* root, const QString& text) {
    for (QQuickItem* item : visual_items(root)) {
        if (item->isVisible() && item->property("text").toString() == text) {
            return item->mapRectToScene(item->boundingRect()).top();
        }
    }
    return -1;
}

std::unique_ptr<QObject> create_main_window(QQmlApplicationEngine& engine, int height,
                                            QQuickWindow*& window) {
    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    if (component.isError()) {
        return {};
    }
    std::unique_ptr<QObject> object(
        component.createWithInitialProperties({{QStringLiteral("sectionIndex"), 3}}));
    window = qobject_cast<QQuickWindow*>(object.get());
    if (window == nullptr) {
        return {};
    }
    window->resize(960, height);
    if (!QTest::qWaitForWindowExposed(window)) {
        return {};
    }
    return object;
}

std::unique_ptr<QObject> create_settings_window(QQmlApplicationEngine& engine, int height,
                                                QQuickWindow*& window) {
    auto host = std::make_unique<QQuickWindow>();
    host->resize(720, height);
    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/settings/AppearanceSettings.qml")));
    std::unique_ptr<QObject> object(component.beginCreate(engine.rootContext()));
    auto* appearance = qobject_cast<QQuickItem*>(object.get());
    if (appearance) {
        appearance->setParentItem(host->contentItem());
        appearance->setSize(QSizeF(host->size()));
    }
    if (object) {
        component.completeCreate();
    }
    if (!appearance || component.isError()) {
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

QQuickItem* appearance_detail(QQuickWindow* window) {
    return window->findChild<QQuickItem*>(QStringLiteral("settings-appearance"));
}

// The Theme card's reader-theme picker, found by its Swift labels because the
// segmented control carries its name only as an accessibility property.
QQuickItem* theme_picker(QQuickItem* root) {
    for (QQuickItem* item : visual_items(root)) {
        const QVariant labels = item->property("model");
        if (labels.isValid() && labels.toStringList().contains(QStringLiteral("Custom"))) {
            return item;
        }
    }
    return nullptr;
}

QColor swatch_color(QQuickItem* root, const QString& row) {
    QQuickItem* swatch = find_item(root, row + QStringLiteral("Swatch"));
    return swatch != nullptr ? swatch->property("color").value<QColor>() : QColor();
}

// A segmented control found by its exact Swift labels.
QQuickItem* segments_with_labels(QQuickItem* root, const QStringList& labels) {
    for (QQuickItem* item : visual_items(root)) {
        if (item->property("model").toStringList() == labels) {
            return item;
        }
    }
    return nullptr;
}

}  // namespace

class SettingsAppearanceTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void popup_card_follows_swift_order();
    void popup_rows_write_their_setting();
    void popup_threshold_row_follows_swipe_toggle();
    void popup_card_fits_the_default_window();
    void theme_custom_color_rows_follow_theme();
    void theme_custom_color_rows_write_their_setting();
    void layout_wheel_page_turn_follows_swift_order();
    void display_statistics_rows_follow_statistics();
    void font_row_lists_and_deletes_imported_fonts();

   private:
    QQmlApplicationEngine engine_;
};

void SettingsAppearanceTest::initTestCase() {
    g_previous_handler = qInstallMessageHandler(collect_qml_messages);
}

// V1, V2, V11: the card is last, its rows follow the Swift order, and a fresh
// data root shows the UserConfig.init defaults.
void SettingsAppearanceTest::popup_card_follows_swift_order() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_settings_window(engine_, 1400, window);
    QVERIFY2(object != nullptr, "AppearanceSettings was not created");
    QQuickItem* appearance = appearance_detail(window);
    QVERIFY(appearance);
    QTest::qWait(50);

    const QStringList cards{QStringLiteral("Theme"), QStringLiteral("Text"),
                            QStringLiteral("Layout"), QStringLiteral("Display"),
                            QStringLiteral("Popup")};
    double previous = -1;
    for (const QString& card : cards) {
        const double top = text_top(appearance, card);
        QVERIFY2(top >= 0, qPrintable(card + QStringLiteral(" card title missing")));
        QVERIFY2(top > previous, qPrintable(card + QStringLiteral(" card is out of order")));
        previous = top;
    }

    const QStringList rows{QStringLiteral("Width"),
                           QStringLiteral("Height"),
                           QStringLiteral("Scale"),
                           QStringLiteral("Show Action Bar"),
                           QStringLiteral("Disable Transparency"),
                           QStringLiteral("Full-width"),
                           QStringLiteral("Swipe to Dismiss")};
    previous = -1;
    for (const QString& row : rows) {
        const double top = text_top(appearance, row);
        QVERIFY2(top >= 0, qPrintable(row + QStringLiteral(" row missing")));
        QVERIFY2(top > previous, qPrintable(row + QStringLiteral(" row is out of order")));
        previous = top;
    }
    QVERIFY2(text_top(appearance, QStringLiteral("Popup")) <
                 text_top(appearance, QStringLiteral("Width")),
             "Popup is not above its first row");
    QVERIFY2(text_top(appearance, QStringLiteral("Swipe Threshold")) < 0,
             "Swipe Threshold is shown while Swipe to Dismiss is off");

    auto* width = find_item(appearance, QStringLiteral("popupWidthRow"));
    auto* height = find_item(appearance, QStringLiteral("popupHeightRow"));
    auto* scale = find_item(appearance, QStringLiteral("popupScaleRow"));
    auto* actionBar = find_item(appearance, QStringLiteral("popupActionBarRow"));
    auto* transparency = find_item(appearance, QStringLiteral("popupDisableTransparencyRow"));
    auto* fullWidth = find_item(appearance, QStringLiteral("popupFullWidthRow"));
    auto* swipe = find_item(appearance, QStringLiteral("popupSwipeToDismissRow"));
    QVERIFY(width);
    QVERIFY(height);
    QVERIFY(scale);
    QVERIFY(actionBar);
    QVERIFY(transparency);
    QVERIFY(fullWidth);
    QVERIFY(swipe);
    auto* card = find_item(appearance, QStringLiteral("settings-popupCard"));
    QVERIFY(card);
    QVERIFY2(card->property("footer").toString().isEmpty(),
             "The Popup card shows a footer Swift does not");
    QCOMPARE(width->property("value").toReal(), 320.0);
    QCOMPARE(height->property("value").toReal(), 250.0);
    QCOMPARE(scale->property("value").toReal(), 1.0);
    QCOMPARE(actionBar->property("checked").toBool(), false);
    QCOMPARE(transparency->property("checked").toBool(), false);
    QCOMPARE(fullWidth->property("checked").toBool(), false);
    QCOMPARE(swipe->property("checked").toBool(), false);

    int separators = 0;
    for (QQuickItem* item : visual_items(appearance)) {
        if (item->objectName() == QStringLiteral("popupSeparator") && item->isVisible()) {
            ++separators;
        }
    }
    QCOMPARE(separators, 6);

    const QString warnings = take_warnings();
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// V3-V9: each row edits the setting it names and the value reaches a fresh
// model; the slider ranges, steps and decimals are Swift's.
void SettingsAppearanceTest::popup_rows_write_their_setting() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_settings_window(engine_, 1400, window);
    QVERIFY2(object != nullptr, "AppearanceSettings was not created");
    QQuickItem* appearance = appearance_detail(window);
    QVERIFY(appearance);
    QTest::qWait(50);

    auto* width = find_item(appearance, QStringLiteral("popupWidthRow"));
    auto* height = find_item(appearance, QStringLiteral("popupHeightRow"));
    auto* scale = find_item(appearance, QStringLiteral("popupScaleRow"));
    auto* actionBar = find_item(appearance, QStringLiteral("popupActionBarRow"));
    auto* transparency = find_item(appearance, QStringLiteral("popupDisableTransparencyRow"));
    auto* fullWidth = find_item(appearance, QStringLiteral("popupFullWidthRow"));
    auto* swipe = find_item(appearance, QStringLiteral("popupSwipeToDismissRow"));
    QVERIFY(width);
    QVERIFY(height);
    QVERIFY(scale);
    QVERIFY(actionBar);
    QVERIFY(transparency);
    QVERIFY(fullWidth);
    QVERIFY(swipe);

    QCOMPARE(width->property("from").toReal(), 100.0);
    QCOMPARE(width->property("to").toReal(), 1400.0);
    QCOMPARE(width->property("stepSize").toReal(), 10.0);
    QCOMPARE(width->property("decimals").toInt(), 0);
    QCOMPARE(height->property("from").toReal(), 100.0);
    QCOMPARE(height->property("to").toReal(), 800.0);
    QCOMPARE(height->property("stepSize").toReal(), 10.0);
    QCOMPARE(height->property("decimals").toInt(), 0);
    QCOMPARE(scale->property("from").toReal(), 0.8);
    QCOMPARE(scale->property("to").toReal(), 1.5);
    QCOMPARE(scale->property("stepSize").toReal(), 0.05);
    QCOMPARE(scale->property("decimals").toInt(), 2);

    QVERIFY(QMetaObject::invokeMethod(width, "moved", Q_ARG(double, 1390.0)));
    QVERIFY(QMetaObject::invokeMethod(height, "moved", Q_ARG(double, 790.0)));
    QVERIFY(QMetaObject::invokeMethod(scale, "moved", Q_ARG(double, 1.45)));
    QVERIFY(QMetaObject::invokeMethod(actionBar, "toggled", Q_ARG(bool, true)));
    QVERIFY(QMetaObject::invokeMethod(transparency, "toggled", Q_ARG(bool, true)));
    QVERIFY(QMetaObject::invokeMethod(fullWidth, "toggled", Q_ARG(bool, true)));
    QVERIFY(QMetaObject::invokeMethod(swipe, "toggled", Q_ARG(bool, true)));

    QCOMPARE(width->property("value").toReal(), 1390.0);
    QCOMPARE(height->property("value").toReal(), 790.0);
    QCOMPARE(scale->property("value").toReal(), 1.45);
    QCOMPARE(actionBar->property("checked").toBool(), true);
    QCOMPARE(transparency->property("checked").toBool(), true);
    QCOMPARE(fullWidth->property("checked").toBool(), true);
    QCOMPARE(swipe->property("checked").toBool(), true);

    niratan::viewmodels::SettingsViewModel fresh;
    QCOMPARE(fresh.popupWidth(), 1390);
    QCOMPARE(fresh.popupHeight(), 790);
    QCOMPARE(fresh.popupScale(), 1.45);
    QCOMPARE(fresh.popupActionBar(), true);
    QCOMPARE(fresh.popupDisableTransparency(), true);
    QCOMPARE(fresh.popupFullWidth(), true);
    QCOMPARE(fresh.popupSwipeToDismiss(), true);

    const QString warnings = take_warnings();
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// V10, V11: the threshold row and its separator appear only while Swipe to
// Dismiss is on, and turning it off keeps the stored threshold.
void SettingsAppearanceTest::popup_threshold_row_follows_swipe_toggle() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_settings_window(engine_, 1400, window);
    QVERIFY2(object != nullptr, "AppearanceSettings was not created");
    QQuickItem* appearance = appearance_detail(window);
    QVERIFY(appearance);
    QTest::qWait(50);

    auto* swipe = find_item(appearance, QStringLiteral("popupSwipeToDismissRow"));
    QVERIFY(swipe);
    QVERIFY(!visible_item(appearance, QStringLiteral("popupSwipeThresholdRow")));
    QVERIFY(!visible_item(appearance, QStringLiteral("popupSwipeThresholdSeparator")));

    QVERIFY(QMetaObject::invokeMethod(swipe, "toggled", Q_ARG(bool, true)));
    QTRY_VERIFY(visible_item(appearance, QStringLiteral("popupSwipeThresholdRow")));
    QTRY_VERIFY(visible_item(appearance, QStringLiteral("popupSwipeThresholdSeparator")));
    auto* threshold = visible_item(appearance, QStringLiteral("popupSwipeThresholdRow"));
    auto* separator = visible_item(appearance, QStringLiteral("popupSwipeThresholdSeparator"));
    QVERIFY(threshold);
    QVERIFY(separator);
    QCOMPARE(threshold->property("label").toString(), QStringLiteral("Swipe Threshold"));
    QCOMPARE(threshold->property("from").toReal(), 20.0);
    QCOMPARE(threshold->property("to").toReal(), 80.0);
    QCOMPARE(threshold->property("stepSize").toReal(), 5.0);
    QCOMPARE(threshold->property("decimals").toInt(), 0);
    QCOMPARE(threshold->property("value").toReal(), 40.0);

    QVERIFY(QMetaObject::invokeMethod(threshold, "moved", Q_ARG(double, 75.0)));
    QTRY_COMPARE(threshold->property("value").toReal(), 75.0);

    QVERIFY(QMetaObject::invokeMethod(swipe, "toggled", Q_ARG(bool, false)));
    QTRY_VERIFY(!visible_item(appearance, QStringLiteral("popupSwipeThresholdRow")));
    QTRY_VERIFY(!visible_item(appearance, QStringLiteral("popupSwipeThresholdSeparator")));
    niratan::viewmodels::SettingsViewModel fresh;
    QCOMPARE(fresh.popupSwipeToDismiss(), false);
    QCOMPARE(fresh.popupSwipeThreshold(), 75);

    const QString warnings = take_warnings();
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// V12: at 960x640 the form scrolls vertically and the card stays inside the
// window horizontally.
void SettingsAppearanceTest::popup_card_fits_the_default_window() {
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_main_window(engine_, 640, window);
    QVERIFY2(object != nullptr, "MainWindow was not created");
    QQuickItem* appearance = appearance_detail(window);
    QVERIFY(appearance);
    auto* card = find_item(appearance, QStringLiteral("settings-popupCard"));
    QVERIFY(card);
    QTest::qWait(50);

    QQuickItem* flickable = nullptr;
    for (QQuickItem* item : visual_items(appearance)) {
        if (item->inherits("QQuickFlickable") && item->property("contentHeight").isValid()) {
            flickable = item;
            break;
        }
    }
    QVERIFY(flickable);
    QVERIFY2(flickable->property("contentHeight").toReal() > flickable->height(),
             "The Appearance form does not scroll to the last card");

    for (QQuickItem* item : visual_items(card)) {
        const QRectF bounds = item->mapRectToScene(item->boundingRect());
        QVERIFY2(bounds.left() >= 0.0,
                 qPrintable(item->objectName() + QStringLiteral(" starts left of the window")));
        QVERIFY2(bounds.right() <= 960.0,
                 qPrintable(item->objectName() + QStringLiteral(" ends right of the window")));
    }

    const QString warnings = take_warnings();
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// C6, C8: the three colour rows and their separators appear only while the
// theme is Custom, after the Interface row, in Swift's order, and a fresh
// data root reads the UserConfig.init colours.
void SettingsAppearanceTest::theme_custom_color_rows_follow_theme() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_settings_window(engine_, 640, window);
    QVERIFY2(object != nullptr, "AppearanceSettings was not created");
    QQuickItem* appearance = appearance_detail(window);
    QVERIFY(appearance);
    QTest::qWait(50);

    const QStringList rows{QStringLiteral("customBackgroundColorRow"),
                           QStringLiteral("customTextColorRow"),
                           QStringLiteral("customInfoColorRow")};
    const QStringList separatorObjects{
        QStringLiteral("customBackgroundColorSeparator"),
        QStringLiteral("customTextColorSeparator"),
        QStringLiteral("customInfoColorSeparator"),
    };
    for (const QString& row : rows) {
        QVERIFY2(!visible_item(appearance, row),
                 qPrintable(row + QStringLiteral(" is shown outside the Custom theme")));
    }
    for (const QString& separator : separatorObjects) {
        QVERIFY2(!visible_item(appearance, separator),
                 qPrintable(separator + QStringLiteral(" is shown outside the Custom theme")));
    }

    QQuickItem* picker = theme_picker(appearance);
    QVERIFY(picker);
    QVERIFY(QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, 4)));
    QTRY_VERIFY(visible_item(appearance, QStringLiteral("customBackgroundColorRow")));

    const QStringList rowObjects{
        QStringLiteral("customBackgroundColorRow"),
        QStringLiteral("customTextColorRow"),
        QStringLiteral("customInfoColorRow"),
    };
    const QStringList rowLabels{
        QStringLiteral("Background Color"),
        QStringLiteral("Text Color"),
        QStringLiteral("Info Color"),
    };

    QString previousLabel = QStringLiteral("Interface");
    for (qsizetype index = 0; index < rowObjects.size(); ++index) {
        QVERIFY2(visible_item(appearance, rowObjects.at(index)),
                 qPrintable(rowObjects.at(index) + QStringLiteral(" is not visible")));
        QVERIFY2(visible_item(appearance, separatorObjects.at(index)),
                 qPrintable(separatorObjects.at(index) + QStringLiteral(" is not visible")));
        const QString& label = rowLabels.at(index);
        QTRY_VERIFY2(text_top(appearance, previousLabel) >= 0 &&
                         text_top(appearance, label) > text_top(appearance, previousLabel),
                     qPrintable(label + QStringLiteral(" is missing or out of order")));
        previousLabel = label;
    }

    QCOMPARE(swatch_color(appearance, QStringLiteral("customBackgroundColorRow")),
             QColor(255, 255, 255));
    QCOMPARE(swatch_color(appearance, QStringLiteral("customTextColorRow")), QColor(0, 0, 0));
    QCOMPARE(swatch_color(appearance, QStringLiteral("customInfoColorRow")), QColor(153, 153, 153));

    const QString warnings = take_warnings();
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// C7, C8: each colour row writes its own setting and its swatch follows the
// model colour.
void SettingsAppearanceTest::theme_custom_color_rows_write_their_setting() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_settings_window(engine_, 640, window);
    QVERIFY2(object != nullptr, "AppearanceSettings was not created");
    QQuickItem* appearance = appearance_detail(window);
    QVERIFY(appearance);
    QTest::qWait(50);

    QQuickItem* picker = theme_picker(appearance);
    QVERIFY(picker);
    QVERIFY(QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, 4)));
    QTRY_VERIFY(visible_item(appearance, QStringLiteral("customBackgroundColorRow")));

    auto* background = find_item(appearance, QStringLiteral("customBackgroundColorRow"));
    auto* text = find_item(appearance, QStringLiteral("customTextColorRow"));
    auto* info = find_item(appearance, QStringLiteral("customInfoColorRow"));
    QVERIFY(background);
    QVERIFY(text);
    QVERIFY(info);

    const QColor red(255, 0, 0);
    const QColor blue(0, 128, 255, 102);
    const QColor transparent(0, 0, 0, 0);
    QVERIFY(QMetaObject::invokeMethod(background, "picked", Q_ARG(QColor, red)));
    QVERIFY(QMetaObject::invokeMethod(text, "picked", Q_ARG(QColor, blue)));
    QVERIFY(QMetaObject::invokeMethod(info, "picked", Q_ARG(QColor, transparent)));

    QTRY_COMPARE(swatch_color(appearance, QStringLiteral("customBackgroundColorRow")), red);
    QTRY_COMPARE(swatch_color(appearance, QStringLiteral("customTextColorRow")), blue);
    QTRY_COMPARE(swatch_color(appearance, QStringLiteral("customInfoColorRow")), transparent);

    niratan::viewmodels::SettingsViewModel fresh;
    QCOMPARE(fresh.customBackgroundColor(), red);
    QCOMPARE(fresh.customTextColor(), blue);
    QCOMPARE(fresh.customInfoColor(), transparent);

    const QString warnings = take_warnings();
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// R3, R7: the wheel toggle sits between the Two-Column row and Horizontal
// Padding in both modes and writes wheelPageTurnEnabled.
void SettingsAppearanceTest::layout_wheel_page_turn_follows_swift_order() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());
    {
        // The Two-Column row needs horizontal pages; vertical writing is the
        // default.
        niratan::viewmodels::SettingsViewModel prep;
        prep.setVerticalWriting(false);
    }
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_settings_window(engine_, 640, window);
    QVERIFY2(object != nullptr, "AppearanceSettings was not created");
    QQuickItem* appearance = appearance_detail(window);
    QVERIFY(appearance);
    QTest::qWait(50);

    QQuickItem* wheel = find_item(appearance, QStringLiteral("wheelPageTurnRow"));
    QVERIFY(wheel);
    QCOMPARE(wheel->property("label").toString(), QStringLiteral("Mouse Wheel Page Turn"));
    QCOMPARE(wheel->property("checked").toBool(), true);

    const QString wheelLabel = QStringLiteral("Mouse Wheel Page Turn");
    const QString spreadsLabel = QStringLiteral("Two-Column Horizontal Pages");
    const QString paddingLabel = QStringLiteral("Horizontal Padding");
    QTRY_VERIFY2(text_top(appearance, spreadsLabel) >= 0 &&
                     text_top(appearance, wheelLabel) > text_top(appearance, spreadsLabel) &&
                     text_top(appearance, paddingLabel) > text_top(appearance, wheelLabel),
                 "Mouse Wheel Page Turn is not between Two-Column and Horizontal Padding");

    QVERIFY(QMetaObject::invokeMethod(wheel, "toggled", Q_ARG(bool, false)));
    QTRY_COMPARE(wheel->property("checked").toBool(), false);
    {
        niratan::viewmodels::SettingsViewModel fresh;
        QCOMPARE(fresh.wheelPageTurnEnabled(), false);
    }

    QQuickItem* mode = segments_with_labels(
        appearance, {QStringLiteral("Paginated"), QStringLiteral("Continuous")});
    QVERIFY(mode);
    QVERIFY(QMetaObject::invokeMethod(mode, "activated", Q_ARG(int, 1)));
    QTRY_VERIFY(visible_item(appearance, QStringLiteral("wheelPageTurnRow")));
    QCOMPARE(wheel->property("checked").toBool(), false);

    const QString warnings = take_warnings();
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// R5, R7: the three statistics rows follow Progress Position while statistics
// is on, before Show Sasayaki Toggle, and each writes its property.
void SettingsAppearanceTest::display_statistics_rows_follow_statistics() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());
    {
        niratan::viewmodels::SettingsViewModel prep;
        prep.setEnableStatistics(true);
        niratan::viewmodels::SasayakiSettingsViewModel sasayaki;
        sasayaki.setEnable(true);
    }
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_settings_window(engine_, 640, window);
    QVERIFY2(object != nullptr, "AppearanceSettings was not created");
    QQuickItem* appearance = appearance_detail(window);
    QVERIFY(appearance);
    QTest::qWait(50);

    const QStringList rows{QStringLiteral("showStatisticsToggleRow"),
                           QStringLiteral("showReadingSpeedRow"),
                           QStringLiteral("showReadingTimeRow")};
    const QStringList labels{QStringLiteral("Show Statistics Toggle"),
                             QStringLiteral("Show Reading Speed"),
                             QStringLiteral("Show Reading Time")};
    const QStringList separators{
        QStringLiteral("showStatisticsToggleSeparator"),
        QStringLiteral("showReadingSpeedSeparator"),
        QStringLiteral("showReadingTimeSeparator"),
    };
    for (qsizetype index = 0; index < rows.size(); ++index) {
        QVERIFY2(visible_item(appearance, rows.at(index)),
                 qPrintable(rows.at(index) + QStringLiteral(" is not visible")));
        QVERIFY2(visible_item(appearance, separators.at(index)),
                 qPrintable(separators.at(index) + QStringLiteral(" is not visible")));
    }

    const QString positionLabel = QStringLiteral("Progress Position");
    const QString sasayakiLabel = QStringLiteral("Show Sasayaki Toggle");
    QTRY_VERIFY2(text_top(appearance, positionLabel) >= 0 &&
                     text_top(appearance, labels.at(0)) > text_top(appearance, positionLabel) &&
                     text_top(appearance, labels.at(1)) > text_top(appearance, labels.at(0)) &&
                     text_top(appearance, labels.at(2)) > text_top(appearance, labels.at(1)) &&
                     text_top(appearance, sasayakiLabel) > text_top(appearance, labels.at(2)),
                 "The statistics rows are not between Progress Position and Sasayaki");

    for (qsizetype index = 0; index < rows.size(); ++index) {
        QQuickItem* row = find_item(appearance, rows.at(index));
        QVERIFY(row);
        QCOMPARE(row->property("label").toString(), labels.at(index));
        QVERIFY(QMetaObject::invokeMethod(row, "toggled", Q_ARG(bool, true)));
        QTRY_COMPARE(row->property("checked").toBool(), true);
    }
    {
        niratan::viewmodels::SettingsViewModel fresh;
        QCOMPARE(fresh.showStatisticsToggle(), true);
        QCOMPARE(fresh.showReadingSpeed(), true);
        QCOMPARE(fresh.showReadingTime(), true);
    }

    object.reset();
    window = nullptr;
    {
        niratan::viewmodels::SettingsViewModel off;
        off.setEnableStatistics(false);
    }
    std::unique_ptr<QObject> second = create_settings_window(engine_, 640, window);
    QVERIFY2(second != nullptr, "AppearanceSettings was not created");
    QQuickItem* hidden_appearance = appearance_detail(window);
    QVERIFY(hidden_appearance);
    QTest::qWait(50);
    for (qsizetype index = 0; index < rows.size(); ++index) {
        QVERIFY2(
            !visible_item(hidden_appearance, rows.at(index)),
            qPrintable(rows.at(index) + QStringLiteral(" is visible while statistics is off")));
        QVERIFY2(!visible_item(hidden_appearance, separators.at(index)),
                 qPrintable(separators.at(index) +
                            QStringLiteral(" is visible while statistics is off")));
    }

    const QString warnings = take_warnings();
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B7: the Appearance Font row lists the imported font after the installed
// families, shows the delete control only for it, confirms Delete "name"? and
// opens the picker for Import Font (AppearanceView.swift 127-179).
void SettingsAppearanceTest::font_row_lists_and_deletes_imported_fonts() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());
    const auto fonts = niratan::storage::data_root() / "Fonts";
    std::error_code error;
    std::filesystem::create_directories(fonts, error);
    const auto imported = fonts / "Imported.ttf";
    {
        QFile file(QString::fromStdString(imported.string()));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("font-bytes"), qint64(10));
    }
    QQuickWindow* window = nullptr;
    std::unique_ptr<QObject> object = create_settings_window(engine_, 1400, window);
    QVERIFY2(object != nullptr, "AppearanceSettings was not created");
    QQuickItem* appearance = appearance_detail(window);
    QVERIFY(appearance);
    QTest::qWait(50);

    QQuickItem* picker = find_item(appearance, QStringLiteral("appearanceFont"));
    QVERIFY(picker);
    QStringList fonts_model = picker->property("model").toStringList();
    QVERIFY(fonts_model.contains(QStringLiteral("Imported")));
    QVERIFY(fonts_model.indexOf(QStringLiteral("Imported")) > 0);

    auto* remove = find_item(appearance, QStringLiteral("appearanceFontDelete"));
    QVERIFY(remove);
    QVERIFY(!remove->isVisible());

    const int imported_index = fonts_model.indexOf(QStringLiteral("Imported"));
    QVERIFY(imported_index >= 0);
    QVERIFY(QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, imported_index)));
    {
        niratan::viewmodels::SettingsViewModel fresh;
        QTRY_COMPARE(fresh.selectedFont(), QStringLiteral("Imported"));
    }
    QTRY_VERIFY(remove->isVisible());

    QVERIFY(QMetaObject::invokeMethod(remove, "clicked"));
    auto* confirmation = window->findChild<QObject*>(QStringLiteral("appearanceFontDeleteDialog"));
    QVERIFY(confirmation);
    QTRY_VERIFY(confirmation->property("visible").toBool());
    QCOMPARE(confirmation->property("message").toString(), QStringLiteral("Delete \"Imported\"?"));
    QVERIFY(confirmation->property("acceptDestructive").toBool());
    QVERIFY(QMetaObject::invokeMethod(confirmation, "accepted"));
    QTRY_VERIFY(!std::filesystem::exists(imported));
    {
        niratan::viewmodels::SettingsViewModel fresh;
        QCOMPARE(fresh.selectedFont(), QStringLiteral("Hiragino Mincho ProN"));
        QVERIFY(fresh.importedFonts().isEmpty());
    }
    QTRY_VERIFY(!remove->isVisible());

    {
        QFile file(QString::fromStdString(imported.string()));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("font-bytes"), qint64(10));
    }
    auto* add = find_item(appearance, QStringLiteral("appearanceFontImport"));
    QVERIFY(add);
    QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
    auto* dialog = window->findChild<QObject*>(QStringLiteral("appearanceFontDialog"));
    QVERIFY(dialog);
    QTRY_VERIFY(dialog->property("visible").toBool());
    const QString filters = dialog->property("nameFilters").toStringList().join(QLatin1Char(' '));
    QVERIFY(filters.contains(QStringLiteral("ttf")));
    QVERIFY(filters.contains(QStringLiteral("otf")));
    QVERIFY(filters.contains(QStringLiteral("ttc")));

    const QString warnings = take_warnings();
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

int main(int argc, char* argv[]) {
    niratan::ui::register_popup_url_schemes();
    niratan::ui::register_reader_url_schemes();
    QtWebEngineQuick::initialize();

    QTemporaryDir data_root;
    if (data_root.isValid()) {
        const QByteArray base = data_root.path().toUtf8();
        qputenv("HOME", base);
        qputenv("XDG_DATA_HOME", base + "/data");
        qputenv("XDG_CACHE_HOME", base + "/cache");
    }

    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    SettingsAppearanceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_settings_appearance.moc"

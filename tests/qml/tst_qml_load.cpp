// SPDX-License-Identifier: GPL-3.0-or-later
//
// Loads the QML layer offscreen: the application window and every page from
// src/views/pages, each with the state it needs from an empty data root. Any
// QML warning, binding error or creation failure fails the test; a page that
// cannot load without real data is reported rather than silenced.
//
// The process mirrors src/main.cpp: URL schemes are registered and
// QtWebEngineQuick initialized before the application object, and the Niratan
// style is selected before a component is created. Pages are parented to a
// window before they complete, because Overlay.overlay and Window.window only
// resolve inside one, as in the application.

#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"
#include "models/profiles/dictionary_settings.h"
#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"
#include "viewmodels/anki/mining_view_model.h"
#include "viewmodels/dictionary/dictionary_popup_model.h"
#include "viewmodels/settings/settings_view_model.h"

#include "../dictionary/yomitan_fixture.h"
#include "../viewmodels/data_root.h"

#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlError>
#include <QPointer>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QUrl>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <algorithm>
#include <clocale>
#include <filesystem>
#include <memory>

Q_IMPORT_QML_PLUGIN(NiratanPlugin)
Q_IMPORT_QML_PLUGIN(NiratanStylePlugin)

namespace {

QStringList g_warnings;
QtMessageHandler g_previous_handler = nullptr;

QString error_text(const QQmlComponent& component) {
    if (!component.isError()) {
        return {};
    }
    QStringList lines;
    for (const QQmlError& error : component.errors()) {
        lines.append(error.toString());
    }
    return lines.join('\n');
}

// Every non-empty text string the page created, in declaration order. A row a
// conditional leaves out is missing here, as it is missing from the view.
QStringList page_texts(QQuickItem* page) {
    QStringList texts;
    for (QObject* child : page->findChildren<QObject*>()) {
        const QVariant text = child->property("text");
        if (text.isValid() && !text.toString().isEmpty()) {
            texts.append(text.toString());
        }
    }
    return texts;
}

// Every non-empty text in the visual tree. A Repeater's delegates are visual
// siblings of the Repeater and are not QObject children of the page, so
// QObject::findChildren misses them; childItems() reaches them.
void collect_visual_texts(QQuickItem* item, QStringList& texts) {
    const QVariant text = item->property("text");
    if (text.isValid() && !text.toString().isEmpty()) {
        texts.append(text.toString());
    }
    for (QQuickItem* child : item->childItems()) {
        collect_visual_texts(child, texts);
    }
}

QStringList visual_page_texts(QQuickItem* page) {
    QStringList texts;
    collect_visual_texts(page, texts);
    return texts;
}

QList<QQuickItem*> visual_items(QQuickItem* item) {
    QList<QQuickItem*> items{item};
    for (QQuickItem* child : item->childItems()) {
        items.append(visual_items(child));
    }
    return items;
}

bool fits_settings_viewport(const QQuickItem& item, const QRectF& window_bounds) {
    auto visible_bounds = item.mapRectToScene(item.boundingRect());
    if (visible_bounds.isEmpty()) {
        return false;
    }

    QList<QRectF> clips{window_bounds};
    for (auto* parent = item.parentItem(); parent; parent = parent->parentItem()) {
        if (!parent->clip()) {
            continue;
        }
        const auto viewport = parent->mapRectToScene(parent->boundingRect());
        if (parent->inherits("QQuickFlickable")) {
            // Only vertical scrolling can move a row out of view. Keep its
            // full horizontal extent for the viewport and window checks.
            visible_bounds.setTop(std::max(visible_bounds.top(), viewport.top()));
            visible_bounds.setBottom(std::min(visible_bounds.bottom(), viewport.bottom()));
        }
        clips.append(viewport);
    }
    const auto fits_horizontally = [&](const QRectF& clip) {
        return visible_bounds.left() >= clip.left() && visible_bounds.right() <= clip.right();
    };
    if (!std::ranges::all_of(clips, fits_horizontally)) {
        return false;
    }
    if (visible_bounds.height() <= 0) {
        return true;
    }
    return std::ranges::all_of(clips,
                               [&](const QRectF& clip) { return clip.contains(visible_bounds); });
}

// QML reports compile errors and binding failures through the message handler.
// Binding loops arrive with the default category and the source file in the
// text, so the file suffix is part of the match.
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

}  // namespace

class QmlLoadTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void main_window_loads();
    void page_components_load();
    void video_lookup_surface_loads();
    void anki_settings_page_connection_rows();
    void settings_column_switches_details();
    void settings_fit_allows_only_vertical_scrolling();
    void dictionary_settings_page_lists_the_management_controls();
    void dictionary_settings_page_shows_sort_and_text_rows();
    void profiles_page_lists_the_management_controls();
    void profile_editor_stays_open_when_save_fails();
    void icon_table_resolves();
    void icon_table_entries_have_files();
    void shared_components_load();
    void main_window_controls_fit();

   private:
    // Empty when the component loaded without a QML warning.
    QString warnings_after(const QString& label);

    // Creates a page from src/views/pages inside window; null with error set.
    std::unique_ptr<QObject> create_page(const QString& page, QQuickWindow& window, QString& error);

    QQmlApplicationEngine engine_;
};

void QmlLoadTest::initTestCase() {
    g_previous_handler = qInstallMessageHandler(collect_qml_messages);
}

QString QmlLoadTest::warnings_after(const QString& label) {
    QTest::qWait(50);
    QString report;
    if (!g_warnings.isEmpty()) {
        report = label + QStringLiteral(":\n") + g_warnings.join('\n');
    }
    g_warnings.clear();
    return report;
}

void QmlLoadTest::main_window_loads() {
    engine_.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    QVERIFY2(!engine_.rootObjects().isEmpty(), "MainWindow was not created");
    const QString warnings = warnings_after(QStringLiteral("MainWindow"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

void QmlLoadTest::page_components_load() {
    const QStringList pages{
        QStringLiteral("AnkiSettingsPage"),
        QStringLiteral("DictionaryPage"),
        QStringLiteral("DictionarySettingsPage"),
        QStringLiteral("LibraryPage"),
        QStringLiteral("PlayerPage"),
        QStringLiteral("ProfilesPage"),
        QStringLiteral("ReaderPage"),
        QStringLiteral("SettingsPage"),
        QStringLiteral("StatisticsPage"),
    };

    QQuickWindow window;
    window.resize(960, 640);

    for (const QString& page : pages) {
        QString error;
        std::unique_ptr<QObject> object = create_page(page, window, error);
        QVERIFY2(object != nullptr, qPrintable(error));

        const QString warnings = warnings_after(page);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
        object.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

std::unique_ptr<QObject> QmlLoadTest::create_page(const QString& page, QQuickWindow& window,
                                                  QString& error) {
    const QUrl url(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/%1.qml").arg(page));
    QQmlComponent component(&engine_, url);
    if (component.isError()) {
        error = page + QStringLiteral(": ") + error_text(component);
        return {};
    }

    std::unique_ptr<QObject> object(component.beginCreate(engine_.rootContext()));
    if (!object) {
        error = page + QStringLiteral(": ") + error_text(component);
        return {};
    }

    auto* item = qobject_cast<QQuickItem*>(object.get());
    if (item == nullptr) {
        error = page + QStringLiteral(" is not an Item");
        return {};
    }
    item->setParentItem(window.contentItem());
    item->setWidth(960);
    item->setHeight(640);
    component.completeCreate();
    return object;
}

// PopupView.swift popupContent and VideoPlayerScreen.swift popupView: the
// video lookup surface is its own item and loads with the two models the page
// supplies, before any lookup exists.
void QmlLoadTest::video_lookup_surface_loads() {
    QQuickWindow window;
    window.resize(960, 640);
    QQmlComponent component(
        &engine_,
        QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/player/VideoLookupSurface.qml")));
    QVERIFY2(!component.isError(), qPrintable(error_text(component)));

    niratan::viewmodels::DictionaryPopupModel popup;
    niratan::viewmodels::MiningViewModel mining;
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
        {{QStringLiteral("popupModel"), QVariant::fromValue(&popup)},
         {QStringLiteral("miningModel"), QVariant::fromValue(&mining)}},
        engine_.rootContext()));
    QVERIFY2(object != nullptr, qPrintable(error_text(component)));
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(item != nullptr, "VideoLookupSurface is not an Item");
    item->setParentItem(window.contentItem());
    item->setWidth(960);
    item->setHeight(640);
    QVERIFY2(!component.isError(), qPrintable(error_text(component)));

    const QString warnings = warnings_after(QStringLiteral("VideoLookupSurface"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// Features/Settings/AnkiView.swift body: the connection card exists before any
// connection, with Address, the masked API key from AnkiConnectView.swift, the
// status line and Connect. The sections behind AnkiManager.isConnected are not
// created while the page is not connected.
void QmlLoadTest::anki_settings_page_connection_rows() {
    QQuickWindow window;
    window.resize(960, 640);
    QString error;
    std::unique_ptr<QObject> object =
        create_page(QStringLiteral("AnkiSettingsPage"), window, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(item != nullptr, "AnkiSettingsPage is not an Item");

    // Read the disconnected state before the event loop delivers the page's
    // opening ping, so a locally running Anki cannot change this result.
    const QStringList texts = page_texts(item);

    const QStringList shown{
        QStringLiteral("AnkiConnect"),
        QStringLiteral("Address"),
        QStringLiteral("API Key (optional)"),
        QStringLiteral("Status: Not connected"),
        QStringLiteral("Connect"),
        QStringLiteral("Card creation uses AnkiConnect."),
    };
    for (const QString& text : shown) {
        QVERIFY2(
            texts.contains(text),
            qPrintable(text + QStringLiteral(" missing: ") + texts.join(QStringLiteral(" | "))));
    }

    // Refresh, AnkiConnect Settings, Config and Settings wait for the
    // connection; Fields also needs a selected note type. Deferred rows and the
    // AnkiMobile footer sentence are not part of the page.
    const QStringList hidden{
        QStringLiteral("Refresh"),
        QStringLiteral("Fetch decks and models from AnkiConnect"),
        QStringLiteral("AnkiConnect Settings"),
        QStringLiteral("Duplicate Scope"),
        QStringLiteral("Check All Models"),
        QStringLiteral("Config"),
        QStringLiteral("Deck"),
        QStringLiteral("Model"),
        QStringLiteral("Settings"),
        QStringLiteral("Allow Duplicates"),
        QStringLiteral("Compact Glossaries"),
        QStringLiteral("Compress Images"),
        QStringLiteral("Fields"),
        QStringLiteral("Apply Defaults"),
        QStringLiteral("Tags"),
        QStringLiteral("Force Sync on adding card"),
        QStringLiteral("Embed Media"),
        QStringLiteral("Audio Compression Format"),
        QStringLiteral("Image Format"),
        QStringLiteral("AnkiMobile callbacks are not used in the Mac app."),
    };
    for (const QString& text : hidden) {
        QVERIFY2(!texts.contains(text),
                 qPrintable(text + QStringLiteral(" shown while not connected")));
    }

    // AnkiConnectView.swift uses a SecureField: the key never shows in clear.
    bool masked = false;
    for (QObject* child : item->findChildren<QObject*>()) {
        if (child->property("placeholderText").toString() == QStringLiteral("API Key (optional)") &&
            child->property("echoMode").toInt() == 2) {
            masked = true;
        }
    }
    QVERIFY2(masked, "The API key row is not a masked input");

    const QString warnings = warnings_after(QStringLiteral("AnkiSettingsPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// NativeSettingsReuseView: source-list order, Appearance by default, and
// independently recreated details rather than pushed pages.
void QmlLoadTest::settings_column_switches_details() {
    QQmlComponent component(&engine_);
    component.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    std::unique_ptr<QObject> object(
        component.createWithInitialProperties({{QStringLiteral("sectionIndex"), 3}}));
    QVERIFY2(object != nullptr, qPrintable(error_text(component)));
    auto* window = qobject_cast<QQuickWindow*>(object.get());
    QVERIFY(window);
    window->resize(960, 640);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto* column = window->findChild<QQuickItem*>(QStringLiteral("settingsColumn"));
    QVERIFY(column);
    const auto column_items = visual_items(column);
    QList<QQuickItem*> rows;
    for (auto* item : column_items) {
        if (item->objectName().startsWith(QStringLiteral("settingsRow-"))) {
            rows.append(item);
        }
    }
    const QStringList keys{"profiles",   "appearance", "dictionaries", "anki",
                           "statistics", "sasayaki",   "report"};
    const QStringList titles{"Profiles",       "Appearance", "Dictionaries",
                             "Anki",           "Statistics", "Sasayaki (Audiobooks)",
                             "Report an Issue"};
    QCOMPARE(rows.size(), keys.size());
    auto* title = window->findChild<QQuickItem*>(QStringLiteral("settingsTitle"));
    QVERIFY(title);
    QCOMPARE(title->property("text").toString(), titles.at(1));
    QVERIFY(rows.at(1)->property("selected").toBool());
    auto* page = window->findChild<QQuickItem*>(QStringLiteral("settingsPage"));
    QVERIFY(page);
    QPointer<QQuickItem> detail =
        page->findChild<QQuickItem*>(QStringLiteral("settings-appearance"));
    QVERIFY(detail);

    for (qsizetype index = 0; index < rows.size(); ++index) {
        auto* row = rows.at(index);
        QCOMPARE(row->objectName(), QStringLiteral("settingsRow-") + keys.at(index));
        QCOMPARE(row->property("title").toString(), titles.at(index));
        if (keys.at(index) == QStringLiteral("report")) {
            continue;
        }
        const QString detail_name = QStringLiteral("settings-") + keys.at(index);
        if (index == 1) {
            QTest::keyClick(window, Qt::Key_Down);
        } else {
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                              row->mapToScene(row->boundingRect().center()).toPoint());
        }
        QTRY_VERIFY(page->findChild<QQuickItem*>(detail_name));
        QTRY_VERIFY(detail.isNull());
        detail = page->findChild<QQuickItem*>(detail_name);
        QCOMPARE(title->property("text").toString(), titles.at(index));
        QVERIFY(row->property("selected").toBool());

        for (auto* item : visual_items(window->contentItem())) {
            QVERIFY(!item->inherits("QQuickStackView"));
            QVERIFY(item->metaObject()->indexOfSignal("backRequested()") < 0);
            if (!item->isVisible() || !item->inherits("QQuickControl")) {
                continue;
            }
            const auto bounds = QRectF(0, 0, window->width(), window->height());
            QVERIFY2(fits_settings_viewport(*item, bounds),
                     qPrintable(QStringLiteral("%1: %2").arg(item->metaObject()->className(),
                                                             item->property("text").toString())));
        }
    }
    const qreal expanded_x = title->x();
    QVERIFY(window->setProperty("sidebarVisible", false));
    QTRY_VERIFY(title->x() > expanded_x);
    QCOMPARE(title->property("text").toString(), titles.at(5));
    QTest::keyClick(window, Qt::Key_Up);
    QTRY_COMPARE(title->property("text").toString(), titles.at(4));
    QQuickItem* statistics = nullptr;
    QTRY_VERIFY((statistics = page->findChild<QQuickItem*>(QStringLiteral("settings-statistics"))));
    const auto controls = visual_items(statistics);
    const auto enable = std::ranges::find_if(controls, [](const QQuickItem* item) {
        return item->isVisible() && item->inherits("QQuickSwitch");
    });
    QVERIFY(enable != controls.end());
    const bool statistics_enabled = (*enable)->property("checked").toBool();
    const auto autostart_visible = [&] {
        for (const auto* item : visual_items(statistics)) {
            if (item->isVisible() &&
                item->property("text").toString() == QStringLiteral("Autostart")) {
                return true;
            }
        }
        return false;
    };
    QCOMPARE(autostart_visible(), statistics_enabled);
    QVERIFY(QMetaObject::invokeMethod(*enable, "click"));
    QTRY_COMPARE(autostart_visible(), !statistics_enabled);
    QVERIFY(QMetaObject::invokeMethod(*enable, "click"));
    QTRY_COMPARE(autostart_visible(), statistics_enabled);
    const QString warnings = warnings_after(QStringLiteral("Settings in MainWindow"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

void QmlLoadTest::settings_fit_allows_only_vertical_scrolling() {
    QQmlComponent component(&engine_);
    component.setData(R"(
        import QtQuick
        Flickable {
            width: 100; height: 100
            contentWidth: width; contentHeight: 200
            clip: true
            Item { objectName: "row"; x: 10; y: 90; width: 80; height: 20 }
        }
    )",
                      QUrl());
    const std::unique_ptr<QObject> fixture(component.create());
    QVERIFY2(fixture, qPrintable(error_text(component)));
    auto* row = fixture->findChild<QQuickItem*>(QStringLiteral("row"));
    QVERIFY(row);
    const QRectF window_bounds(0, 0, 120, 100);
    QVERIFY(fits_settings_viewport(*row, window_bounds));
    QVERIFY(!fits_settings_viewport(*row, QRectF(0, 0, 85, 100)));
    row->setX(30);
    QVERIFY(!fits_settings_viewport(*row, window_bounds));
    row->setX(105);
    QVERIFY(!fits_settings_viewport(*row, window_bounds));
    row->setX(10);
    row->setY(110);
    QVERIFY(fits_settings_viewport(*row, window_bounds));
    QVERIFY(!fits_settings_viewport(*row, QRectF(0, 0, 85, 100)));
    row->setX(30);
    QVERIFY(!fits_settings_viewport(*row, window_bounds));
}

// Features/Settings/DictionaryView.swift body, dictionaryTypePicker and
// DictionaryCategoryPicker: the import button, the formats note, the four
// type segments and the recommended list. O3 keeps downloads out.
void QmlLoadTest::dictionary_settings_page_lists_the_management_controls() {
    QQuickWindow window;
    window.resize(960, 640);
    QString error;
    std::unique_ptr<QObject> object =
        create_page(QStringLiteral("DictionarySettingsPage"), window, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(item != nullptr, "DictionarySettingsPage is not an Item");

    const QStringList texts = visual_page_texts(item);
    const QStringList shown{
        QStringLiteral("Dictionaries"),
        QStringLiteral("Import"),
        QStringLiteral("Supported Formats"),
        QStringLiteral("Term"),
        QStringLiteral("Frequency"),
        QStringLiteral("Pitch"),
        QStringLiteral("Kanji"),
        QStringLiteral("Recommended Dictionaries"),
    };
    for (const QString& text : shown) {
        QVERIFY2(
            texts.contains(text),
            qPrintable(text + QStringLiteral(" missing: ") + texts.join(QStringLiteral(" | "))));
    }

    auto* css = item->findChild<QQuickItem*>(QStringLiteral("dictionaryCssButton"));
    QVERIFY(css);
    QVERIFY(QMetaObject::invokeMethod(css, "click"));
    QTRY_VERIFY(visual_page_texts(window.contentItem()).contains(QStringLiteral("Reset")));
    const auto dialog_items = visual_items(window.contentItem());
    const auto close = std::ranges::find_if(dialog_items, [](const QQuickItem* child) {
        return child->isVisible() &&
            child->property("text").toString() == QStringLiteral("Close") &&
            child->inherits("QQuickAbstractButton");
    });
    QVERIFY(close != dialog_items.end());
    QVERIFY(QMetaObject::invokeMethod(*close, "click"));
    QTRY_VERIFY(!visual_page_texts(window.contentItem()).contains(QStringLiteral("Reset")));

    auto* recommendations =
        item->findChild<QQuickItem*>(QStringLiteral("recommendedDictionariesButton"));
    QVERIFY(recommendations);
    QVERIFY(QMetaObject::invokeMethod(recommendations, "click"));
    QVERIFY(visual_page_texts(window.contentItem()).contains(QStringLiteral("JMdict")));

    const QString warnings = warnings_after(QStringLiteral("DictionarySettingsPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// Features/Settings/DictionaryView.swift DictionaryBehaviorSettingsSections:
// the Frequency Sorting picker writes the order, the conditional Frequency
// Dictionary picker offers the enabled titles with None first, and the Search
// Text card's Text Size row writes the size. A local data root keeps the
// page's SettingsViewModel out of the developer's settings.
void QmlLoadTest::dictionary_settings_page_shows_sort_and_text_rows() {
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());

    const auto zip = std::filesystem::path(data.path().toStdString()) / "probe.zip";
    niratan::test::write_stored_zip(
        zip,
        {{QStringLiteral("index.json"), niratan::test::minimal_index(QStringLiteral("Probe"))},
         {QStringLiteral("term_bank_1.json"), niratan::test::minimal_term_bank()},
         {QStringLiteral("term_meta_bank_1.json"),
          QByteArrayLiteral(R"([["decide","freq",100]])")}});
    QVERIFY(niratan::dictionary::DictionaryStore(niratan::storage::dictionaries_root())
                .import_zip(zip)
                .ok);
    niratan::profiles::ProfileRepository profiles{niratan::storage::data_root()};
    const auto directory = profiles.profile_directory(std::string("default-ja"));
    QVERIFY(directory.has_value());
    QFile config(QString::fromStdString((*directory / "dictionary_config.json").string()));
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray config_bytes =
        R"({"termDictionaries":[],"frequencyDictionaries":[{"fileName":"Probe","isEnabled":true,"order":0}],"pitchDictionaries":[]})";
    QCOMPARE(config.write(config_bytes), config_bytes.size());
    config.close();

    QQuickWindow window;
    window.resize(960, 640);
    QString error;
    std::unique_ptr<QObject> object =
        create_page(QStringLiteral("DictionarySettingsPage"), window, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(item != nullptr, "DictionarySettingsPage is not an Item");

    const QString footer = QStringLiteral("Results with the same match are ranked by the selected "
                                          "frequency dictionary. Results without a value are "
                                          "listed last.");
    const auto footer_shown = [item, &footer] {
        const auto items = visual_items(item);
        return std::ranges::any_of(items, [&footer](const QQuickItem* child) {
            return child->isVisible() && child->property("text").toString() == footer;
        });
    };

    auto* sort = item->findChild<QQuickItem*>(QStringLiteral("frequencySortPicker"));
    QVERIFY2(sort != nullptr, "DictionarySettingsPage has no Frequency Sorting picker");
    QVERIFY2(visual_page_texts(item).contains(QStringLiteral("Frequency Sorting")),
             "the Frequency Sorting label is missing");
    QCOMPARE(sort->property("model").toStringList(),
             (QStringList{QStringLiteral("Auto"), QStringLiteral("Ascending"),
                          QStringLiteral("Descending")}));
    QVERIFY(!footer_shown());

    auto* text_size = item->findChild<QQuickItem*>(QStringLiteral("searchTextSizeRow"));
    QVERIFY2(text_size != nullptr, "DictionarySettingsPage has no Text Size row");
    QCOMPARE(text_size->property("from").toInt(), 12);
    QCOMPARE(text_size->property("to").toInt(), 48);

    QVERIFY2(item->findChild<QObject*>(QStringLiteral("frequencyDictionaryPicker")) == nullptr,
             "the Frequency Dictionary row shows while the order is Auto");

    QVERIFY(QMetaObject::invokeMethod(sort, "activated", Q_ARG(int, 2)));
    QTRY_VERIFY2(item->findChild<QObject*>(QStringLiteral("frequencyDictionaryPicker")) != nullptr,
                 "the Frequency Dictionary row did not appear");
    auto* dictionary = item->findChild<QQuickItem*>(QStringLiteral("frequencyDictionaryPicker"));
    QVERIFY2(dictionary != nullptr, "the Frequency Dictionary row has no picker");
    QTRY_COMPARE(dictionary->property("model").toStringList(),
                 (QStringList{QStringLiteral("None"), QStringLiteral("Probe")}));
    QVERIFY2(visual_page_texts(item).contains(QStringLiteral("Frequency Dictionary")),
             "the Frequency Dictionary label is missing");
    QTRY_VERIFY(footer_shown());

    const QRectF bounds(0, 0, window.width(), window.height());
    QVERIFY(fits_settings_viewport(*sort, bounds));
    QVERIFY(fits_settings_viewport(*text_size, bounds));
    QVERIFY(fits_settings_viewport(*dictionary, bounds));

    QVERIFY(QMetaObject::invokeMethod(dictionary, "activated", Q_ARG(int, 1)));
    niratan::viewmodels::SettingsViewModel chosen;
    QCOMPARE(chosen.frequencySortDictionary(), QStringLiteral("Probe"));
    QTRY_COMPARE(dictionary->property("model").toStringList(),
                 QStringList{QStringLiteral("Probe")});

    QVERIFY2(visual_page_texts(item).contains(QStringLiteral("Search Text")),
             "the Search Text card is missing");
    QVERIFY2(visual_page_texts(item).contains(QStringLiteral("Text Size")),
             "the Text Size label is missing");

    niratan::viewmodels::SettingsViewModel sorted;
    QCOMPARE(sorted.frequencySortOrder(), QStringLiteral("Descending"));

    QVERIFY(QMetaObject::invokeMethod(text_size, "edited", Q_ARG(int, 30)));
    niratan::viewmodels::SettingsViewModel sized;
    QCOMPARE(sized.searchTextSize(), 30);

    const QString warnings = warnings_after(QStringLiteral("DictionarySettingsPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// Features/Settings/ProfilesView.swift body and profileRow: the Create row,
// the Built-in marker, the language caption and the
// footer sentence about per-profile settings.
void QmlLoadTest::profiles_page_lists_the_management_controls() {
    QQuickWindow window;
    window.resize(960, 640);
    QString error;
    std::unique_ptr<QObject> object = create_page(QStringLiteral("ProfilesPage"), window, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(item != nullptr, "ProfilesPage is not an Item");

    const QStringList texts = visual_page_texts(item);
    const QStringList shown{
        QStringLiteral("Active Profile"),
        QStringLiteral("Create Profile"),
        QStringLiteral("Built-in"),
        QStringLiteral("Japanese"),
        QStringLiteral("Dictionary, reader appearance and Anki mining settings follow the active "
                       "profile. Open pages keep the settings they already loaded until they load "
                       "them again."),
    };
    for (const QString& text : shown) {
        QVERIFY2(
            texts.contains(text),
            qPrintable(text + QStringLiteral(" missing: ") + texts.join(QStringLiteral(" | "))));
    }

    const QString warnings = warnings_after(QStringLiteral("ProfilesPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// ProfilesView.profileEditor closes only after the repository accepts the change.
void QmlLoadTest::profile_editor_stays_open_when_save_fails() {
    QTemporaryDir profile_root;
    QVERIFY(profile_root.isValid());
    const auto previous_root = qgetenv("XDG_DATA_HOME");
    const auto restore_root = qScopeGuard([&] { qputenv("XDG_DATA_HOME", previous_root); });
    QVERIFY(qputenv("XDG_DATA_HOME", profile_root.path().toUtf8()));

    QQuickWindow window;
    window.resize(960, 640);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QString error;
    const auto page = create_page(QStringLiteral("ProfilesPage"), window, error);
    QVERIFY2(page, qPrintable(error));

    const auto index = niratan::storage::data_root() / "Profiles" / "profiles.json";
    QVERIFY(std::filesystem::remove(index));
    QVERIFY(std::filesystem::create_directory(index));

    auto* create = page->findChild<QObject*>(QStringLiteral("createProfileButton"));
    auto* editor = page->findChild<QObject*>(QStringLiteral("profileEditor"));
    auto* name = page->findChild<QObject*>(QStringLiteral("profileNameField"));
    auto* save = page->findChild<QObject*>(QStringLiteral("saveProfileButton"));
    QVERIFY(create && editor && name && save);
    QVERIFY(QMetaObject::invokeMethod(create, "click"));
    QTRY_VERIFY(editor->property("opened").toBool());
    auto* save_button = qobject_cast<QQuickItem*>(save);
    QVERIFY(save_button);
    const auto dialog_items = visual_items(window.contentItem());
    const auto cancel = std::ranges::find_if(dialog_items, [](const QQuickItem* item) {
        return item->isVisible() && item->property("text").toString() == QStringLiteral("Cancel");
    });
    QVERIFY(cancel != dialog_items.end());
    QVERIFY((*cancel)->mapToScene((*cancel)->boundingRect().center()).x() <
            save_button->mapToScene(save_button->boundingRect().center()).x());
    QVERIFY(name->setProperty("text", QStringLiteral("Demo Reader")));
    QVERIFY(save->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(save, "click"));
    QTRY_VERIFY(visual_page_texts(window.contentItem())
                    .contains(QStringLiteral("Could not write the profile index.")));
    QVERIFY(editor->property("opened").toBool());
    QCOMPARE(name->property("text").toString(), QStringLiteral("Demo Reader"));

    QVERIFY(std::filesystem::remove(index));
    QVERIFY(QMetaObject::invokeMethod(save, "click"));
    QTRY_VERIFY(!editor->property("visible").toBool());
    QVERIFY(visual_page_texts(window.contentItem()).contains(QStringLiteral("Demo Reader")));
    const QString warnings = warnings_after(QStringLiteral("Profile editor"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// Every row of src/views/icons/icon_map.js names a bundled SVG, so a symbol a
// view adds never draws nothing for want of its asset.
void QmlLoadTest::icon_table_entries_have_files() {
    QFile table(QStringLiteral(":/qt/qml/Niratan/src/views/icons/icon_map.js"));
    QVERIFY2(table.open(QIODevice::ReadOnly), "icon_map.js is not bundled");
    const QString text = QString::fromUtf8(table.readAll());
    static const QRegularExpression row(
        QStringLiteral(R"re("([^"]+)": \{ weight: "(\w+)", name: "([^"]+)" \})re"));
    int rows = 0;
    for (const QRegularExpressionMatch& match : row.globalMatch(text)) {
        ++rows;
        const QString weight = match.captured(2);
        const QString suffix =
            weight == QStringLiteral("regular") ? QString() : QStringLiteral("-") + weight;
        const QString path = QStringLiteral(":/qt/qml/Niratan/src/views/icons/phosphor/%1/%2%3.svg")
                                 .arg(weight, match.captured(3), suffix);
        QVERIFY2(QFile::exists(path),
                 qPrintable(match.captured(1) + QStringLiteral(" names a missing file: ") + path));
    }
    QVERIFY2(rows > 20, "the icon table was not parsed");
}

// B2-B7 of the views-icons module: every SF Symbol name the shared components
// draw resolves through src/views/icons/icon_map.js to an existing Phosphor
// SVG, and a name the table does not know reports itself as unknown with no
// source. The mapping table itself is not duplicated here.
void QmlLoadTest::icon_table_resolves() {
    QQuickWindow window;
    window.resize(960, 640);
    QQmlComponent component(
        &engine_, QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/components/Icon.qml")));
    QVERIFY2(!component.isError(), qPrintable(error_text(component)));

    const QStringList symbols{
        QStringLiteral("books.vertical"),  QStringLiteral("character.book.closed"),
        QStringLiteral("play.rectangle"),  QStringLiteral("gearshape"),
        QStringLiteral("magnifyingglass"), QStringLiteral("xmark.circle.fill"),
    };
    for (const QString& symbol : symbols) {
        std::unique_ptr<QObject> object(component.createWithInitialProperties(
            {{QStringLiteral("name"), symbol}, {QStringLiteral("size"), 24.0}},
            engine_.rootContext()));
        QVERIFY2(object != nullptr,
                 qPrintable(symbol + QStringLiteral(": ") + error_text(component)));
        auto* icon = qobject_cast<QQuickItem*>(object.get());
        QVERIFY2(icon != nullptr, qPrintable(symbol + QStringLiteral(" is not an Item")));
        icon->setParentItem(window.contentItem());

        QVERIFY2(icon->property("known").toBool(),
                 qPrintable(symbol + QStringLiteral(" is not a known icon")));
        const QUrl source = icon->property("source").toUrl();
        QVERIFY2(!source.isEmpty(), qPrintable(symbol + QStringLiteral(" has no source")));
        QVERIFY2(
            QFile(QStringLiteral(":") + source.path()).exists(),
            qPrintable(symbol + QStringLiteral(" names a missing resource: ") + source.toString()));
        QCOMPARE(icon->implicitWidth(), 24.0);
        QCOMPARE(icon->implicitHeight(), 24.0);
    }

    std::unique_ptr<QObject> unknown(component.createWithInitialProperties(
        {{QStringLiteral("name"), QStringLiteral("not.a.symbol")}, {QStringLiteral("size"), 24.0}},
        engine_.rootContext()));
    QVERIFY2(unknown != nullptr, qPrintable(error_text(component)));
    auto* unknown_icon = qobject_cast<QQuickItem*>(unknown.get());
    QVERIFY2(unknown_icon != nullptr, "Icon is not an Item");
    QVERIFY(!unknown_icon->property("known").toBool());
    QVERIFY(unknown_icon->property("source").toUrl().isEmpty());

    const QString warnings = warnings_after(QStringLiteral("Icon"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B1-B4 of the views-components module: the shared surfaces carry the
// geometry and behaviour the pages rely on, and the search bar clears its text
// and returns focus to the field. B6.
void QmlLoadTest::shared_components_load() {
    QQuickWindow window;
    window.resize(960, 640);

    auto create = [this, &window](const QString& component_name,
                                  QString& error) -> std::unique_ptr<QObject> {
        const QUrl url(
            QStringLiteral("qrc:/qt/qml/Niratan/src/views/components/%1.qml").arg(component_name));
        QQmlComponent component(&engine_, url);
        if (component.isError()) {
            error = component_name + QStringLiteral(": ") + error_text(component);
            return {};
        }
        std::unique_ptr<QObject> object(component.beginCreate(engine_.rootContext()));
        if (!object) {
            error = component_name + QStringLiteral(": ") + error_text(component);
            return {};
        }
        auto* item = qobject_cast<QQuickItem*>(object.get());
        if (item == nullptr) {
            error = component_name + QStringLiteral(" is not an Item");
            return {};
        }
        item->setParentItem(window.contentItem());
        component.completeCreate();
        return object;
    };

    QString error;
    std::unique_ptr<QObject> surface = create(QStringLiteral("GlassSurface"), error);
    QVERIFY2(surface != nullptr, qPrintable(error));
    auto* surface_item = qobject_cast<QQuickItem*>(surface.get());
    QVERIFY2(surface_item != nullptr, "GlassSurface is not an Item");
    surface_item->setWidth(240);
    surface_item->setHeight(120);
    QCOMPARE(surface_item->property("cornerRadius").toInt(), 18);
    QCOMPARE(surface_item->property("borderWidth").toInt(), 1);

    std::unique_ptr<QObject> capsule = create(QStringLiteral("GlassCapsule"), error);
    QVERIFY2(capsule != nullptr, qPrintable(error));
    auto* capsule_item = qobject_cast<QQuickItem*>(capsule.get());
    QVERIFY2(capsule_item != nullptr, "GlassCapsule is not an Item");
    capsule_item->setWidth(240);
    capsule_item->setHeight(44);
    QCOMPARE(capsule_item->property("radius").toReal(), 22.0);
    QCOMPARE(capsule_item->property("borderWidth").toInt(), 1);

    std::unique_ptr<QObject> icon_button = create(QStringLiteral("IconButton"), error);
    QVERIFY2(icon_button != nullptr, qPrintable(error));
    auto* icon_button_item = qobject_cast<QQuickItem*>(icon_button.get());
    QVERIFY2(icon_button_item != nullptr, "IconButton is not an Item");
    icon_button_item->setProperty("icon", QStringLiteral("xmark.circle.fill"));
    QCOMPARE(icon_button_item->property("diameter").toReal(), 38.0);
    QCOMPARE(icon_button_item->implicitWidth(), 38.0);
    QCOMPARE(icon_button_item->implicitHeight(), 38.0);
    icon_button_item->setProperty("diameter", 44);
    QCOMPARE(icon_button_item->implicitWidth(), 44.0);
    QCOMPARE(icon_button_item->implicitHeight(), 44.0);

    std::unique_ptr<QObject> search_bar = create(QStringLiteral("SearchBar"), error);
    QVERIFY2(search_bar != nullptr, qPrintable(error));
    auto* search_bar_item = qobject_cast<QQuickItem*>(search_bar.get());
    QVERIFY2(search_bar_item != nullptr, "SearchBar is not an Item");
    search_bar_item->setWidth(620);
    search_bar_item->setHeight(48);

    QQuickItem* clear = search_bar_item->findChild<QQuickItem*>(QStringLiteral("clearButton"));
    QVERIFY2(clear != nullptr, "SearchBar has no clear control");
    QQuickItem* field = search_bar_item->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY2(field != nullptr, "SearchBar has no field");
    QVERIFY(field->property("placeholderText").toString().isEmpty());

    QCOMPARE(search_bar_item->property("text").toString(), QString());
    QVERIFY(!clear->isVisible());

    search_bar_item->setProperty("text", QStringLiteral("星"));
    QVERIFY(clear->isVisible());

    QVERIFY(QMetaObject::invokeMethod(clear, "clicked"));
    QCOMPARE(search_bar_item->property("text").toString(), QString());
    QVERIFY(field->hasFocus());

    const QString warnings = warnings_after(QStringLiteral("shared components"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B8 of the views-shell module: every visible control of the default window
// lies inside the 960x640 frame. The failure names each clipped control by its
// text, or its class when it has none, so a control pushed out by the toolbar
// is identifiable.
void QmlLoadTest::main_window_controls_fit() {
    QQmlApplicationEngine engine;
    engine.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    QVERIFY2(!engine.rootObjects().isEmpty(), "MainWindow was not created");
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
    QVERIFY2(window != nullptr, "MainWindow is not a Window");
    window->resize(960, 640);
    QTest::qWait(100);

    QStringList offenders;
    auto visit = [&](auto&& self, QQuickItem* parent) -> void {
        for (QQuickItem* item : parent->childItems()) {
            if (item->isVisible() && item->width() > 0 && item->height() > 0 &&
                (item->inherits("QQuickControl") || item->inherits("QQuickTextInput"))) {
                const QPointF top_left = item->mapToItem(window->contentItem(), QPointF(0, 0));
                const qreal right = top_left.x() + item->width();
                const qreal bottom = top_left.y() + item->height();
                if (top_left.x() < 0 || top_left.y() < 0 || right > window->width() ||
                    bottom > window->height()) {
                    QString name = item->property("text").toString();
                    if (name.isEmpty())
                        name = QString::fromLatin1(item->metaObject()->className());
                    offenders.append(QStringLiteral("%1 at (%2,%3) size %4x%5")
                                         .arg(name)
                                         .arg(top_left.x())
                                         .arg(top_left.y())
                                         .arg(item->width())
                                         .arg(item->height()));
                }
            }
            self(self, item);
        }
    };
    visit(visit, window->contentItem());
    QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QLatin1Char('\n'))));
}

int main(int argc, char* argv[]) {
    niratan::ui::register_popup_url_schemes();
    niratan::ui::register_reader_url_schemes();
    QtWebEngineQuick::initialize();

    // Keep profile, cache and dictionary paths out of the developer's home.
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

    QmlLoadTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_qml_load.moc"

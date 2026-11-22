// SPDX-License-Identifier: GPL-3.0-or-later
//
// Loads the QML layer offscreen: the application window and every page from
// src/views/pages, each with the state it needs from an empty data root. Any
// QML warning, binding error or creation failure fails the test; a page that
// cannot load without real data is reported rather than silenced.
//
// The process mirrors src/main.cpp: URL schemes are registered and
// QtWebEngineQuick initialized before the application object, and the Basic
// style is selected before a component is created. Pages are parented to a
// window before they complete, because Overlay.overlay and Window.window only
// resolve inside one, as in the application.

#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"
#include "models/profiles/dictionary_settings.h"
#include "services/storage/app_paths.h"
#include "viewmodels/anki/mining_view_model.h"
#include "viewmodels/dictionary/dictionary_popup_model.h"

#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QUrl>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

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
    void settings_page_offers_anki_section();
    void settings_page_offers_dictionaries_section();
    void settings_page_offers_profiles_section();
    void dictionary_settings_page_lists_the_management_controls();
    void profiles_page_lists_the_management_controls();
    void icon_table_resolves();
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
        QStringLiteral("Anki"),
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

// SettingsHomeView.swift lists Anki in the settings sidebar; the repo's
// SettingsPage rail reaches the Anki page through a signal instead.
void QmlLoadTest::settings_page_offers_anki_section() {
    QQuickWindow window;
    window.resize(960, 640);
    QString error;
    std::unique_ptr<QObject> object = create_page(QStringLiteral("SettingsPage"), window, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(item != nullptr, "SettingsPage is not an Item");

    QVERIFY2(item->metaObject()->indexOfSignal("ankiRequested()") >= 0,
             "SettingsPage has no ankiRequested signal");

    const QStringList texts = page_texts(item);
    QVERIFY2(texts.contains(QStringLiteral("Anki")), qPrintable(texts.join(QStringLiteral(" | "))));

    const QString warnings = warnings_after(QStringLiteral("SettingsPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// NativeMac/NativeReuseViews.swift settingsSidebar lists Dictionary and, after
// it, the Dictionaries destination; MainWindow loads the page from its signal.
void QmlLoadTest::settings_page_offers_dictionaries_section() {
    QQuickWindow window;
    window.resize(960, 640);
    QString error;
    std::unique_ptr<QObject> object = create_page(QStringLiteral("SettingsPage"), window, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(item != nullptr, "SettingsPage is not an Item");

    QVERIFY2(item->metaObject()->indexOfSignal("dictionariesRequested()") >= 0,
             "SettingsPage has no dictionariesRequested signal");

    const QStringList texts = page_texts(item);
    QVERIFY2(texts.contains(QStringLiteral("Dictionary")),
             qPrintable(texts.join(QStringLiteral(" | "))));
    QVERIFY2(texts.contains(QStringLiteral("Dictionaries")),
             qPrintable(texts.join(QStringLiteral(" | "))));

    const QString warnings = warnings_after(QStringLiteral("SettingsPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B5: NativeMac/NativeReuseViews.swift settingsSidebar puts Profiles first in
// the Library group; the port's Settings rail requests the page (O1).
void QmlLoadTest::settings_page_offers_profiles_section() {
    QQuickWindow window;
    window.resize(960, 640);
    QString error;
    std::unique_ptr<QObject> object = create_page(QStringLiteral("SettingsPage"), window, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(item != nullptr, "SettingsPage is not an Item");

    QVERIFY2(item->metaObject()->indexOfSignal("profilesRequested()") >= 0,
             "SettingsPage has no profilesRequested signal");

    const QStringList texts = page_texts(item);
    QVERIFY2(texts.contains(QStringLiteral("Profiles")),
             qPrintable(texts.join(QStringLiteral(" | "))));

    const QString warnings = warnings_after(QStringLiteral("SettingsPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
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

    QVERIFY2(item->metaObject()->indexOfSignal("backRequested()") >= 0,
             "DictionarySettingsPage has no backRequested signal");

    const QStringList texts = page_texts(item);
    const QStringList shown{
        QStringLiteral("Dictionaries"),
        QStringLiteral("Import"),
        QStringLiteral("Supported Formats"),
        QStringLiteral("Term"),
        QStringLiteral("Frequency"),
        QStringLiteral("Pitch"),
        QStringLiteral("Kanji"),
        QStringLiteral("Recommended for this profile"),
        QStringLiteral("JMdict"),
    };
    for (const QString& text : shown) {
        QVERIFY2(
            texts.contains(text),
            qPrintable(text + QStringLiteral(" missing: ") + texts.join(QStringLiteral(" | "))));
    }

    const QString warnings = warnings_after(QStringLiteral("DictionarySettingsPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B5: Features/Settings/ProfilesView.swift body and profileRow: the back
// signal, the Create row, the Built-in marker, the language caption and the
// footer sentence about per-profile settings.
void QmlLoadTest::profiles_page_lists_the_management_controls() {
    QQuickWindow window;
    window.resize(960, 640);
    QString error;
    std::unique_ptr<QObject> object = create_page(QStringLiteral("ProfilesPage"), window, error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* item = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(item != nullptr, "ProfilesPage is not an Item");

    QVERIFY2(item->metaObject()->indexOfSignal("backRequested()") >= 0,
             "ProfilesPage has no backRequested signal");

    const QStringList texts = visual_page_texts(item);
    const QStringList shown{
        QStringLiteral("Profiles"),
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

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary detail page as Features/Dictionary/DictionarySearchView.swift
// lays it out: `DictionarySearchBar` is the page's first content at the top,
// the popup document fills the rest, and there is no title band, back control
// or page toolbar (`NativeMac/NativeMacDetailView.swift` dictionary case,
// `NativeMac/NativeReuseViews.swift` `NativeDictionaryReuseView.body`).
//
// The search field is the shared SearchBar component, so the clear control and
// the Return path are the ones the component provides
// (`DictionarySearchBar.body`, `CustomSearchField.body`,
// `DictionarySearchTextFieldBridge.swift`). Return runs one lookup; the model
// trims the text and an empty or whitespace-only query clears the results
// (`DictionarySearchView.runLookup`).
//
// Pages are created inside a shown, active offscreen window because the focus
// behaviours are the page's own (`DictionarySearchView.onAppear` with
// `initialAutofocus: false`). The data root points at a temporary directory
// with the Probe dictionary installed, so the lookup tests never read the
// developer's dictionaries or profiles.
//
// Besides the visible layout, the tests drive the WebChannel bridge the way
// the popup script does (its inline lookup must answer from the current
// entries) and switch the active profile while a query is displayed, because
// the Mac page reruns a searched query after a profile change
// (`DictionarySearchView.onChange(profileRepository.index.globalActiveProfileId)`).

#include "../viewmodels/data_root.h"

#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"
#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"
#include "yomitan_fixture.h"

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
#include <QSize>
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

void collect_named_items(QQuickItem* item, const QString& name, QList<QQuickItem*>& found) {
    if (item->objectName() == name) {
        found.append(item);
    }
    for (QQuickItem* child : item->childItems()) {
        collect_named_items(child, name, found);
    }
}

QQuickItem* item_named_in(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> found;
    if (root != nullptr) {
        collect_named_items(root, name, found);
    }
    return found.isEmpty() ? nullptr : found.first();
}

// The trailing slot of QML reports no type for a JS function's `data`
// parameter, so the bridge is located by the method it must answer.
QObject* dictionary_bridge(QQuickItem* page) {
    QObject* bridge = nullptr;
    for (QObject* child : page->findChildren<QObject*>()) {
        if (child->metaObject()->indexOfMethod("getEntries(int,QVariant)") >= 0) {
            if (bridge != nullptr) {
                return nullptr;
            }
            bridge = child;
        }
    }
    return bridge;
}

// The active profile's dictionary list with the imported Probe term dictionary
// enabled at order 0, as the fixture dictionaries are addressed by file name.
bool write_probe_config(const std::filesystem::path& directory) {
    QFile file(QString::fromStdString((directory / "dictionary_config.json").string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray bytes =
        R"({"termDictionaries":[{"fileName":"Probe","isEnabled":true,"order":0}],"frequencyDictionaries":[],"pitchDictionaries":[]})";
    return file.write(bytes) == bytes.size();
}

}  // namespace

class DictionaryViewTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void page_has_no_title_band();
    void search_bar_sits_at_the_page_top();
    void clear_control_clears_and_refocuses();
    void submit_looks_up_the_query();
    void dictionary_page_shows_the_query_heading();
    void bridge_answers_its_inline_lookups();
    void profile_switch_reloads_the_searched_page();
    void page_opens_without_focusing_the_field();
    void shell_places_the_dictionary_at_the_content_top();
    void collapsed_sidebar_insets_the_search_bar();
    void aligned_page_checks_after_the_shell_change();

   private:
    // Creates the page at size inside a shown, active window; null with error
    // set when the component is missing or cannot be created.
    std::unique_ptr<QObject> create_page(QQuickWindow& window, const QSize& size, QString& error);

    // Empty when the component loaded without a QML warning.
    QString warnings_after(const QString& label);

    std::unique_ptr<niratan::test::TempDataRoot> root_;
    QQmlApplicationEngine engine_;
};

void DictionaryViewTest::initTestCase() {
    g_previous_handler = qInstallMessageHandler(collect_qml_messages);

    root_ = std::make_unique<niratan::test::TempDataRoot>();
    QVERIFY(root_->isValid());

    const auto zip = std::filesystem::path(root_->path().toStdString()) / "dictionary.zip";
    niratan::test::write_stored_zip(
        zip,
        {{QStringLiteral("index.json"), niratan::test::minimal_index(QStringLiteral("Probe"))},
         {QStringLiteral("term_bank_1.json"), niratan::test::minimal_term_bank()}});

    const auto dictionaries = niratan::storage::dictionaries_root();
    QVERIFY(!dictionaries.empty());
    const auto imported = niratan::dictionary::DictionaryStore(dictionaries).import_zip(zip);
    QVERIFY2(imported.ok, qPrintable(QString::fromStdString(imported.error)));

    // The active profile's dictionary list enables the imported Probe term
    // dictionary, so the page's first lookup finds the fixture entry.
    niratan::profiles::ProfileRepository profiles{niratan::storage::data_root()};
    const auto directory = profiles.profile_directory(std::string("default-ja"));
    QVERIFY(directory.has_value());
    QVERIFY2(write_probe_config(*directory), "cannot write the Probe dictionary config");
}

QString DictionaryViewTest::warnings_after(const QString& label) {
    QTest::qWait(50);
    QString report;
    if (!g_warnings.isEmpty()) {
        report = label + QStringLiteral(":\n") + g_warnings.join('\n');
    }
    g_warnings.clear();
    return report;
}

std::unique_ptr<QObject> DictionaryViewTest::create_page(QQuickWindow& window, const QSize& size,
                                                         QString& error) {
    window.resize(size);
    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) {
        error = QStringLiteral("the test window was not exposed");
        return {};
    }
    window.requestActivate();
    if (!QTest::qWaitForWindowActive(&window)) {
        error = QStringLiteral("the test window was not activated");
        return {};
    }

    const QUrl url(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/DictionaryPage.qml"));
    QQmlComponent component(&engine_, url);
    if (component.isError()) {
        error = QStringLiteral("DictionaryPage: ") + error_text(component);
        return {};
    }

    std::unique_ptr<QObject> object(component.beginCreate(engine_.rootContext()));
    if (!object) {
        error = QStringLiteral("DictionaryPage: ") + error_text(component);
        return {};
    }

    auto* item = qobject_cast<QQuickItem*>(object.get());
    if (item == nullptr) {
        error = QStringLiteral("DictionaryPage is not an Item");
        return {};
    }
    item->setParentItem(window.contentItem());
    item->setWidth(size.width());
    item->setHeight(size.height());
    component.completeCreate();
    return object;
}

// B1, B3: the bar is the page's first content; no "Dictionary" heading text
// and no PageHeader band exist, and the field is the shared one with an empty
// placeholder.
void DictionaryViewTest::page_has_no_title_band() {
    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 600), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(page != nullptr, "DictionaryPage is not an Item");

    const QString warnings = warnings_after(QStringLiteral("DictionaryPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));

    const QStringList texts = visual_page_texts(page);
    QVERIFY2(
        !texts.contains(QStringLiteral("Dictionary")),
        qPrintable(QStringLiteral("a title band is drawn: ") + texts.join(QStringLiteral(" | "))));

    for (QObject* child : page->findChildren<QObject*>()) {
        QVERIFY2(child->metaObject()->indexOfProperty("showBack") < 0,
                 qPrintable(QStringLiteral("a page header band is present: ") +
                            QString::fromLatin1(child->metaObject()->className())));
    }

    auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY2(field != nullptr, "DictionaryPage has no search field");
    QVERIFY2(field->property("placeholderText").toString().isEmpty(),
             "the search field has a placeholder");
}

// B2, B9: the capsule bar spans the page at y = 0 with its 20 px outer inset,
// and the field stays inside that inset at the default and minimum sizes.
void DictionaryViewTest::search_bar_sits_at_the_page_top() {
    const auto verify = [](QQuickItem* page) {
        auto* bar = page->findChild<QQuickItem*>(QStringLiteral("searchBar"));
        QVERIFY2(bar != nullptr, "DictionaryPage has no search bar");
        auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
        QVERIFY2(field != nullptr, "DictionaryPage has no search field");

        QCOMPARE(bar->x(), 0.0);
        QCOMPARE(bar->y(), 0.0);
        QCOMPARE(bar->width(), page->width());

        const QPointF field_top_left = field->mapToItem(page, QPointF(0, 0));
        QVERIFY2(field_top_left.x() >= 20.0,
                 qPrintable(QStringLiteral("the field starts left of the 20 px inset: %1")
                                .arg(field_top_left.x())));
        QVERIFY2(field_top_left.x() + field->width() <= page->width() - 20.0,
                 qPrintable(QStringLiteral("the field ends right of the 20 px inset: %1")
                                .arg(field_top_left.x() + field->width())));
    };

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(page != nullptr, "DictionaryPage is not an Item");
    verify(page);
    QString warnings = warnings_after(QStringLiteral("DictionaryPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));

    object.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    error.clear();
    std::unique_ptr<QObject> minimum = create_page(window, QSize(900, 620), error);
    QVERIFY2(minimum != nullptr, qPrintable(error));
    auto* minimum_page = qobject_cast<QQuickItem*>(minimum.get());
    QVERIFY2(minimum_page != nullptr, "DictionaryPage is not an Item");
    verify(minimum_page);
    warnings = warnings_after(QStringLiteral("DictionaryPage at the minimum size"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B3: the clear control follows the text, clears it and returns focus to the
// field.
void DictionaryViewTest::clear_control_clears_and_refocuses() {
    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(page != nullptr, "DictionaryPage is not an Item");

    auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY2(field != nullptr, "DictionaryPage has no search field");
    auto* clear = page->findChild<QQuickItem*>(QStringLiteral("clearButton"));
    QVERIFY2(clear != nullptr, "DictionaryPage has no clear control");

    QVERIFY2(!clear->isVisible(), "the clear control shows while the field is empty");

    field->setProperty("text", QStringLiteral("星"));
    QTRY_VERIFY2(clear->isVisible(), "the clear control stays hidden with text in the field");

    QVERIFY(QMetaObject::invokeMethod(clear, "clicked"));
    QCOMPARE(field->property("text").toString(), QString());
    QTRY_VERIFY2(!clear->isVisible(), "the clear control stays visible after clearing");
    QTRY_VERIFY2(field->hasActiveFocus(), "clearing did not return focus to the field");
}

// B4: Return submits the query once and the results surface follows the model;
// a whitespace-only query clears it.
void DictionaryViewTest::submit_looks_up_the_query() {
    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(page != nullptr, "DictionaryPage is not an Item");

    auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY2(field != nullptr, "DictionaryPage has no search field");
    auto* model = page->findChild<QObject*>(QStringLiteral("popupModel"));
    QVERIFY2(model != nullptr, "DictionaryPage has no popup model");
    auto* results = page->findChild<QQuickItem*>(QStringLiteral("results"));
    QVERIFY2(results != nullptr, "DictionaryPage has no results surface");
    QVERIFY2(!results->isVisible(), "the results surface shows before the first lookup");

    field->forceActiveFocus();
    QSignalSpy html_spy(model, SIGNAL(htmlChanged()));
    field->setProperty("text", QStringLiteral("decide"));
    QTest::keyClick(&window, Qt::Key_Return);

    QTRY_VERIFY2(model->property("hasResults").toBool(), "Return did not run the lookup");
    QCOMPARE(html_spy.count(), 1);
    QVERIFY2(!field->hasActiveFocus(), "Return kept focus in the field");
    QVERIFY2(results->isVisible(), "the results surface stays hidden with results");
    QVERIFY2(model->property("html").toString().contains(QStringLiteral("decide")),
             "the rendered document does not contain the query");

    field->forceActiveFocus();
    field->setProperty("text", QStringLiteral("   "));
    QTest::keyClick(&window, Qt::Key_Return);

    QTRY_VERIFY2(!model->property("hasResults").toBool(),
                 "a whitespace-only query kept the results");
    QVERIFY2(!results->isVisible(), "the results surface stays visible without results");

    const QString warnings = warnings_after(QStringLiteral("DictionaryPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B4: DictionarySearchView.body's PopupWebView passes the query heading; the
// page's model is the only host that enables it.
void DictionaryViewTest::dictionary_page_shows_the_query_heading() {
    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(page != nullptr, "DictionaryPage is not an Item");

    auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY2(field != nullptr, "DictionaryPage has no search field");
    auto* model = page->findChild<QObject*>(QStringLiteral("popupModel"));
    QVERIFY2(model != nullptr, "DictionaryPage has no popup model");
    QVERIFY2(model->property("page").toBool(), "the dictionary page host is not enabled");

    field->forceActiveFocus();
    field->setProperty("text", QStringLiteral("decide"));
    QTest::keyClick(&window, Qt::Key_Return);
    QTRY_VERIFY2(model->property("hasResults").toBool(), "Return did not run the lookup");

    const QString html = model->property("html").toString();
    QVERIFY2(html.contains(QStringLiteral("id=\"dictionary-query-source\"")),
             "the document has no query heading");
    QVERIFY2(html.contains(QStringLiteral("window.dictionaryQuerySource = \"decide\";")),
             "the document does not carry the query");

    const QString warnings = warnings_after(QStringLiteral("DictionaryPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B7: the page's WebChannel bridge answers the document's inline lookup from
// the current entry maps (PopupWebView.Coordinator.userContentController and
// DictionarySearchView.body's PopupWebView closures); the test drives the
// bridge exactly as the script does.
void DictionaryViewTest::bridge_answers_its_inline_lookups() {
    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(page != nullptr, "DictionaryPage is not an Item");

    auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY2(field != nullptr, "DictionaryPage has no search field");
    auto* model = page->findChild<QObject*>(QStringLiteral("popupModel"));
    QVERIFY2(model != nullptr, "DictionaryPage has no popup model");

    field->forceActiveFocus();
    field->setProperty("text", QStringLiteral("decide"));
    QTest::keyClick(&window, Qt::Key_Return);
    QTRY_VERIFY2(model->property("hasResults").toBool(), "Return did not run the lookup");

    QObject* bridge = dictionary_bridge(page);
    QVERIFY2(bridge != nullptr, "DictionaryPage has no single dictionary bridge");
    QSignalSpy spy(bridge, SIGNAL(replyReady(int, QVariant)));

    const QVariantMap request{{"start", 0}, {"count", 1}};
    QVERIFY(
        QMetaObject::invokeMethod(bridge, "getEntries", Q_ARG(int, 7), Q_ARG(QVariant, request)));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 7);
    const QVariantList entries = spy.at(0).at(1).toList();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.at(0).toMap().value("expression").toString(), QStringLiteral("decide"));
}

// B8: a searched page follows an active-profile switch without another
// Return: DictionarySettingsWatch reruns the saved query with the new
// profile's configuration (DictionarySearchView.onChange(globalActiveProfileId)).
void DictionaryViewTest::profile_switch_reloads_the_searched_page() {
    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(page != nullptr, "DictionaryPage is not an Item");

    auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY2(field != nullptr, "DictionaryPage has no search field");
    auto* model = page->findChild<QObject*>(QStringLiteral("popupModel"));
    QVERIFY2(model != nullptr, "DictionaryPage has no popup model");

    field->forceActiveFocus();
    field->setProperty("text", QStringLiteral("decide"));
    QTest::keyClick(&window, Qt::Key_Return);
    QTRY_VERIFY2(model->property("hasResults").toBool(), "Return did not run the lookup");

    niratan::profiles::ProfileRepository profiles{niratan::storage::data_root()};
    const auto english =
        profiles.create_profile(std::string("English"), niratan::profiles::ContentLanguage::english,
                                std::string("default-ja"));
    QVERIFY(english.profile.has_value());
    const auto directory = profiles.profile_directory(english.profile->id);
    QVERIFY(directory.has_value());
    QVERIFY2(write_probe_config(*directory), "cannot write the Probe dictionary config");
    QVERIFY(!profiles.set_active_profile(english.profile->id).has_value());

    QTRY_VERIFY_WITH_TIMEOUT(model->property("html").toString().contains(
                                 QStringLiteral("window.hoshiSelection.language = 'en';")),
                             5000);

    QVERIFY(!profiles.set_active_profile(std::string("default-ja")).has_value());
}

// B5: the section does not autofocus the field (NativeDictionaryReuseView
// passes initialAutofocus: false); a click focuses it.
void DictionaryViewTest::page_opens_without_focusing_the_field() {
    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(page != nullptr, "DictionaryPage is not an Item");

    auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY2(field != nullptr, "DictionaryPage has no search field");
    QTest::qWait(50);
    QVERIFY2(!field->hasActiveFocus(), "the page focused the field when it opened");

    const QPointF center =
        field->mapToItem(window.contentItem(), QPointF(field->width() / 2, field->height() / 2));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
    QTRY_VERIFY2(field->hasActiveFocus(), "clicking the field did not focus it");
}

// B1: MainWindow treats the Dictionary section as owning the toolbar row, so
// the page and its bar start at the window content top with no band above it.
void DictionaryViewTest::shell_places_the_dictionary_at_the_content_top() {
    QQmlApplicationEngine engine;
    engine.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    QVERIFY2(!engine.rootObjects().isEmpty(), "MainWindow was not created");
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    window->setProperty("sectionIndex", 1);
    QQuickItem* bar = nullptr;
    QTRY_VERIFY((bar = item_named_in(window->contentItem(), QStringLiteral("searchBar"))) !=
                nullptr);
    auto* page = bar->parentItem();
    QVERIFY2(page != nullptr, "the search bar has no page parent");

    QTRY_COMPARE(page->mapToItem(window->contentItem(), QPointF()).y(), 0.0);
    QTRY_COMPARE(bar->mapToItem(window->contentItem(), QPointF()).y(), 0.0);
    QTRY_COMPARE(page->mapToItem(window->contentItem(), QPointF(0, page->height())).y(),
                 window->contentItem()->height());

    auto* results = item_named_in(page, QStringLiteral("results"));
    QVERIFY2(results != nullptr, "DictionaryPage has no results surface");
    QVERIFY2(results->mapToItem(page, QPointF()).y() >= bar->height(),
             "the results surface starts above the search bar's bottom");

    for (QObject* child : page->findChildren<QObject*>()) {
        QVERIFY2(child->metaObject()->indexOfProperty("showBack") < 0,
                 qPrintable(QStringLiteral("a page header band is present: ") +
                            QString::fromLatin1(child->metaObject()->className())));
    }

    const QString warnings = warnings_after(QStringLiteral("MainWindow"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B2: the collapsed sidebar passes toolbarInset 44 and the bar leaves exactly
// that room; restoring the sidebar returns the full-page span.
void DictionaryViewTest::collapsed_sidebar_insets_the_search_bar() {
    QQmlApplicationEngine engine;
    engine.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    QVERIFY2(!engine.rootObjects().isEmpty(), "MainWindow was not created");
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    window->setProperty("sectionIndex", 1);
    QQuickItem* bar = nullptr;
    QTRY_VERIFY((bar = item_named_in(window->contentItem(), QStringLiteral("searchBar"))) !=
                nullptr);
    auto* page = bar->parentItem();
    QVERIFY2(page != nullptr, "the search bar has no page parent");

    QTRY_COMPARE(page->property("toolbarInset").toReal(), 0.0);
    QTRY_COMPARE(bar->x(), 0.0);
    QTRY_VERIFY(qAbs(bar->width() - page->width()) <= 0.5);

    window->setProperty("sidebarVisible", false);
    QTRY_COMPARE(page->property("toolbarInset").toReal(), 44.0);
    QTRY_COMPARE(bar->x(), 44.0);
    QTRY_VERIFY(qAbs(bar->width() - (page->width() - 44.0)) <= 0.5);

    window->setProperty("sidebarVisible", true);
    QTRY_COMPARE(page->property("toolbarInset").toReal(), 0.0);
    QTRY_COMPARE(bar->x(), 0.0);
    QTRY_VERIFY(qAbs(bar->width() - page->width()) <= 0.5);

    const QString warnings = warnings_after(QStringLiteral("MainWindow"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B3, B4: the aligned page keeps its first-content bar, empty placeholder and
// results below the bar at the default and minimum window sizes.
void DictionaryViewTest::aligned_page_checks_after_the_shell_change() {
    for (const QSize& size : {QSize(960, 640), QSize(900, 620)}) {
        QQuickWindow window;
        QString error;
        std::unique_ptr<QObject> object = create_page(window, size, error);
        QVERIFY2(object != nullptr, qPrintable(error));
        auto* page = qobject_cast<QQuickItem*>(object.get());
        QVERIFY2(page != nullptr, "DictionaryPage is not an Item");

        auto* bar = page->findChild<QQuickItem*>(QStringLiteral("searchBar"));
        QVERIFY2(bar != nullptr, "DictionaryPage has no search bar");
        auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
        QVERIFY2(field != nullptr, "DictionaryPage has no search field");
        auto* results = page->findChild<QQuickItem*>(QStringLiteral("results"));
        QVERIFY2(results != nullptr, "DictionaryPage has no results surface");

        QCOMPARE(bar->y(), 0.0);
        QCOMPARE(bar->x(), 0.0);
        QVERIFY2(field->property("placeholderText").toString().isEmpty(),
                 "the search field has a placeholder");
        QVERIFY2(results->mapToItem(page, QPointF()).y() >= bar->height(),
                 "the results surface starts above the search bar's bottom");

        const QString warnings = warnings_after(QStringLiteral("DictionaryPage"));
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
    }
}

int main(int argc, char* argv[]) {
    niratan::ui::register_popup_url_schemes();
    niratan::ui::register_reader_url_schemes();
    QtWebEngineQuick::initialize();

    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    DictionaryViewTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_dictionary_view.moc"

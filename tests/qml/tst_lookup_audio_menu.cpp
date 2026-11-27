// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Popup/PopupWebView.swift (showAudioSourceMenu, audioSourceSelected)
// serves the popup in the reader, the video player and the dictionary page
// alike, so a right click on a word's audio button opens the source menu in
// all three. Each lookup surface must route the page's audioSourceMenu message
// through its bridge to DictionaryPopupModel, open the menu the model then
// requests, and close it when the lookup is hidden.

#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"
#include "services/dictionary/dictionary_store.h"
#include "services/storage/app_paths.h"
#include "viewmodels/anki/mining_view_model.h"
#include "viewmodels/dictionary/dictionary_popup_model.h"
#include "viewmodels/sasayaki/sasayaki_view_model.h"

#include "../dictionary/yomitan_fixture.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QUrl>
#include <QTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>
#include <filesystem>
#include <memory>

using niratan::viewmodels::DictionaryPopupModel;

class LookupAudioMenuTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void video_lookup_opens_the_audio_source_menu();
    void reader_lookup_opens_the_audio_source_menu();
    void dictionary_page_opens_the_audio_source_menu();

   private:
    QQmlApplicationEngine engine_;
};

namespace {

QString error_text(const QQmlComponent& component) {
    QStringList lines;
    for (const QQmlError& error : component.errors()) {
        lines.append(error.toString());
    }
    return lines.join('\n');
}

// Creates a view in an offscreen window with the given initial properties.
std::unique_ptr<QObject> create(QQmlEngine& engine, const QString& path,
                                const QVariantMap& properties, QQuickWindow& window) {
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qt/qml/Niratan/") + path));
    if (component.isError()) {
        qWarning().noquote() << error_text(component);
        return {};
    }
    // The parent is set before the bindings run, as a page declares it.
    std::unique_ptr<QObject> object(component.beginCreate(engine.rootContext()));
    if (!object) {
        qWarning().noquote() << error_text(component);
        return {};
    }
    component.setInitialProperties(object.get(), properties);
    if (auto* item = qobject_cast<QQuickItem*>(object.get())) {
        item->setParentItem(window.contentItem());
        item->setWidth(window.width());
        item->setHeight(window.height());
    }
    component.completeCreate();
    return object;
}

// The page posts audioSourceMenu through the bridge for the looked-up entry;
// the surface opens its menu with the names and closes it once hidden.
void check_audio_menu(QObject* root, DictionaryPopupModel* model) {
    QVERIFY(root != nullptr);
    QVERIFY(model != nullptr);
    model->lookup(QStringLiteral("decide"));
    QVERIFY(model->has_results());

    QObject* bridge = root->findChild<QObject*>(QStringLiteral("dictionaryBridge"));
    QVERIFY2(bridge != nullptr, "the lookup has no dictionaryBridge");
    QObject* menu = root->findChild<QObject*>(QStringLiteral("audioSourceMenu"));
    QVERIFY2(menu != nullptr, "the lookup has no audio source menu");
    QObject* view = menu->property("webView").value<QObject*>();
    QVERIFY(view != nullptr);
    QTRY_VERIFY(!view->property("loading").toBool());
    QVERIFY(!menu->property("visible").toBool());

    const QStringList names{QStringLiteral("Preferred: NHK16"),
                            QStringLiteral("Preferred: Forvo (skent)")};
    const QVariantMap payload{{QStringLiteral("entryIndex"), 0},
                              {QStringLiteral("names"), names},
                              {QStringLiteral("selected"), 1},
                              {QStringLiteral("x"), 4.0},
                              {QStringLiteral("y"), 4.0}};
    QVERIFY2(QMetaObject::invokeMethod(bridge, "showAudioSourceMenu",
                                       Q_ARG(QVariant, QVariant(payload))),
             "the bridge does not route audioSourceMenu to the popup model");
    QTRY_VERIFY(menu->property("visible").toBool());
    QCOMPARE(menu->property("sourceNames").toStringList(), names);
    QCOMPARE(menu->property("selectedSource").toInt(), 1);

    view->setProperty("visible", false);
    QTRY_VERIFY(!menu->property("visible").toBool());
}

}  // namespace

void LookupAudioMenuTest::initTestCase() {
    const auto zip = std::filesystem::path(QDir::tempPath().toStdString()) /
        ("niratan-audio-menu-" + std::to_string(QCoreApplication::applicationPid()) + ".zip");
    niratan::test::write_stored_zip(
        zip,
        {{"index.json", niratan::test::minimal_index(QStringLiteral("Probe"))},
         {"term_bank_1.json", niratan::test::minimal_term_bank()}});
    const auto imported =
        niratan::dictionary::DictionaryStore(niratan::storage::dictionaries_root()).import_zip(zip);
    std::filesystem::remove(zip);
    QVERIFY2(imported.ok, imported.error.c_str());
}

void LookupAudioMenuTest::video_lookup_opens_the_audio_source_menu() {
    QQuickWindow window;
    window.resize(960, 640);
    DictionaryPopupModel popup;
    niratan::viewmodels::MiningViewModel mining;
    const std::unique_ptr<QObject> surface =
        create(engine_, QStringLiteral("src/views/player/VideoLookupSurface.qml"),
               {{QStringLiteral("popupModel"), QVariant::fromValue(&popup)},
                {QStringLiteral("miningModel"), QVariant::fromValue(&mining)}},
               window);
    check_audio_menu(surface.get(), &popup);
}

void LookupAudioMenuTest::reader_lookup_opens_the_audio_source_menu() {
    QQuickWindow window;
    window.resize(960, 640);
    DictionaryPopupModel popup;
    niratan::viewmodels::MiningViewModel mining;
    niratan::viewmodels::SasayakiViewModel player;
    const std::unique_ptr<QObject> surface =
        create(engine_, QStringLiteral("src/views/reader/ReaderLookupSurface.qml"),
               {{QStringLiteral("popupModel"), QVariant::fromValue(&popup)},
                {QStringLiteral("miningModel"), QVariant::fromValue(&mining)},
                {QStringLiteral("player"), QVariant::fromValue(&player)}},
               window);
    check_audio_menu(surface.get(), &popup);
}

void LookupAudioMenuTest::dictionary_page_opens_the_audio_source_menu() {
    QQuickWindow window;
    window.resize(960, 640);
    const std::unique_ptr<QObject> page =
        create(engine_, QStringLiteral("src/views/pages/DictionaryPage.qml"), {}, window);
    QVERIFY(page != nullptr);
    check_audio_menu(page.get(), page->findChild<DictionaryPopupModel*>());
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

    LookupAudioMenuTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_lookup_audio_menu.moc"

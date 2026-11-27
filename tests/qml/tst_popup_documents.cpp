// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Popup/PopupWebView.swift loads every generated lookup document with
// loadHTMLString, whatever its size, so a new lookup always replaces the
// popup. A word found in many dictionaries yields a document of several
// megabytes; the popup must show it rather than keep the previous lookup.
// PopupDocuments hands each view only its own latest document.

#include "services/dictionary/dictionary_store.h"
#include "services/storage/app_paths.h"
#include "views/dictionary/audio_scheme_handler.h"
#include "views/dictionary/popup_documents.h"

#include "../dictionary/yomitan_fixture.h"
#include "data_root.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTest>
#include <QUrl>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>
#include <filesystem>

using niratan::ui::PopupDocuments;

class PopupDocumentsTest : public QObject {
    Q_OBJECT

   private slots:
    void a_view_accepts_only_its_latest_document();
    void a_lookup_over_two_megabytes_replaces_the_popup();
};

namespace {

QQuickItem* find_type(QQuickItem* item, QLatin1StringView type) {
    if (QLatin1StringView(item->metaObject()->className()).startsWith(type)) {
        return item;
    }
    for (QQuickItem* child : item->childItems()) {
        if (QQuickItem* found = find_type(child, type)) {
            return found;
        }
    }
    return nullptr;
}

QQuickItem* web_view_in(QQuickItem* item) {
    if (item->inherits("QQuickWebEngineView")) {
        return item;
    }
    for (QQuickItem* child : item->childItems()) {
        if (QQuickItem* found = web_view_in(child)) {
            return found;
        }
    }
    return nullptr;
}

// 星 has a short entry; 見る has one glossary of three megabytes, which
// as a data URL passes Chromium's two megabyte limit.
QByteArray term_bank() {
    QByteArray huge("BIGMARK");
    huge.append(QByteArray(3 * 1024 * 1024, 'x'));
    return QByteArray(R"([["星","ほし","","n",0,["star"],1,""],["見る","みる","","v1",0,[")") +
        huge + QByteArray(R"("],2,""]])");
}

}  // namespace

void PopupDocumentsTest::a_view_accepts_only_its_latest_document() {
    QObject first;
    QObject second;
    PopupDocuments* documents = PopupDocuments::instance();

    const QUrl old_url = documents->publish(&first, QStringLiteral("<p>old</p>"));
    const QUrl url = documents->publish(&first, QStringLiteral("<p>new</p>"));
    const QUrl other = documents->publish(&second, QStringLiteral("<p>other</p>"));
    QCOMPARE(url.scheme(), QStringLiteral("popup"));

    QVERIFY(documents->isDocument(&first, url));
    QVERIFY(!documents->isDocument(&first, old_url));
    QVERIFY(!documents->isDocument(&first, other));
    QVERIFY(!documents->isDocument(&second, url));
    QVERIFY(!documents->isDocument(&first, QUrl(QStringLiteral("data:text/html,x"))));
    QCOMPARE(documents->document(url), std::optional<QByteArray>("<p>new</p>"));
    QCOMPARE(documents->document(old_url), std::nullopt);
}

void PopupDocumentsTest::a_lookup_over_two_megabytes_replaces_the_popup() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const auto zip = std::filesystem::path(root.path().toStdString()) / "dictionary.zip";
    niratan::test::write_stored_zip(
        zip,
        {{QStringLiteral("index.json"), niratan::test::minimal_index(QStringLiteral("Probe"))},
         {QStringLiteral("term_bank_1.json"), term_bank()}});
    QVERIFY(niratan::dictionary::DictionaryStore(niratan::storage::dictionaries_root())
                .import_zip(zip)
                .ok);
    const QString video = QDir(root.path()).filePath(QStringLiteral("Alpha.mkv"));
    QFile file(video);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/shell/VideoWindow.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(QMetaObject::invokeMethod(
        window, "openVideo", Q_ARG(QVariant, QVariant(QUrl::fromLocalFile(video))),
        Q_ARG(QVariant, QVariant(QUrl())), Q_ARG(QVariant, QVariant(false))));
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QObject* lookup = nullptr;
    for (QObject* object : window->findChildren<QObject*>()) {
        if (object->inherits("niratan::viewmodels::PlayerLookupViewModel")) {
            lookup = object;
        }
    }
    QVERIFY(lookup != nullptr);
    QVERIFY(QMetaObject::invokeMethod(
        lookup, "setSubtitleDocument",
        Q_ARG(QUrl, QUrl::fromLocalFile(QDir(root.path()).filePath(QStringLiteral("a.srt")))),
        Q_ARG(QByteArray, QByteArray("1\n00:00:00,000 --> 00:00:10,000\n星を見る\n")),
        Q_ARG(bool, true)));
    QVERIFY(QMetaObject::invokeMethod(lookup, "updatePlayback", Q_ARG(double, 1.0),
                                      Q_ARG(double, 0.0)));

    QQuickItem* surface = find_type(window->contentItem(), QLatin1StringView("VideoLookupSurface"));
    QVERIFY(surface != nullptr);
    QQuickItem* view = web_view_in(surface);
    QVERIFY(view != nullptr);
    // The page reports whether its source holds the marker of the big entry.
    const auto shows_big_entry = [view] {
        QMetaObject::invokeMethod(
            view, "runJavaScript",
            Q_ARG(QString,
                  QStringLiteral("document.title = document.documentElement.outerHTML"
                                 ".includes('BIGMARK') ? 'big' : 'small'")));
        return view->property("title").toString();
    };

    QVariantMap result;
    QVERIFY(QMetaObject::invokeMethod(lookup, "lookupAt", Q_RETURN_ARG(QVariantMap, result),
                                      Q_ARG(QString, QStringLiteral("1")), Q_ARG(int, 0)));
    QVERIFY(!result.isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(shows_big_entry(), QStringLiteral("small"), 10000);

    QVERIFY(QMetaObject::invokeMethod(lookup, "lookupAt", Q_RETURN_ARG(QVariantMap, result),
                                      Q_ARG(QString, QStringLiteral("1")), Q_ARG(int, 2)));
    QCOMPARE(result.value(QStringLiteral("matchedText")).toString(), QStringLiteral("見る"));
    QTRY_COMPARE_WITH_TIMEOUT(shows_big_entry(), QStringLiteral("big"), 10000);
}

int main(int argc, char* argv[]) {
    niratan::ui::register_popup_url_schemes();
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    PopupDocumentsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_popup_documents.moc"

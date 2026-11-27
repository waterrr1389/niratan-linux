// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Video/VideoPlayerScreen+Shortcuts.swift: Space and T reach the
// player whenever no text field is editing. A click in the subtitle lookup
// popup gives its web view the keyboard, so closing the popup must hand the
// keyboard back to the player page; otherwise the shortcuts stop working.

#include "services/dictionary/dictionary_store.h"
#include "services/storage/app_paths.h"
#include "viewmodels/player/player_lookup_view_model.h"

#include "../dictionary/yomitan_fixture.h"
#include "data_root.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrl>
#include <QTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>
#include <filesystem>

using niratan::viewmodels::PlayerLookupViewModel;

class VideoLookupFocusTest : public QObject {
    Q_OBJECT

   private slots:
    void closing_the_lookup_returns_the_keyboard_to_the_player();
    void closing_the_lookup_leaves_focus_elsewhere_alone();
};

namespace {

QQuickItem* web_view_in(QQuickItem* item) {
    if (item == nullptr) {
        return nullptr;
    }
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

bool import_dictionary(const QString& folder) {
    const auto zip = std::filesystem::path(folder.toStdString()) / "dictionary.zip";
    niratan::test::write_stored_zip(
        zip,
        {{QStringLiteral("index.json"), niratan::test::minimal_index(QStringLiteral("Probe"))},
         {QStringLiteral("term_bank_1.json"),
          QByteArray(R"([["星","ほし","","n",0,["star"],1,""]])")}});
    return niratan::dictionary::DictionaryStore(niratan::storage::dictionaries_root())
        .import_zip(zip)
        .ok;
}

// A video window with a subtitle lookup popup open over a placeholder file.
struct OpenLookup {
    niratan::test::TempDataRoot root;
    QQmlApplicationEngine engine;
    QQuickWindow* window = nullptr;
    QQuickItem* player = nullptr;
    PlayerLookupViewModel* lookup = nullptr;
    QQuickItem* view = nullptr;

    bool open() {
        if (!root.isValid() || !import_dictionary(root.path())) {
            return false;
        }
        const QString video = QDir(root.path()).filePath(QStringLiteral("Alpha.mkv"));
        QFile file(video);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write("x");
        file.close();

        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/shell/VideoWindow.qml")));
        if (engine.rootObjects().isEmpty()) {
            return false;
        }
        window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        if (window == nullptr ||
            !QMetaObject::invokeMethod(
                window, "openVideo", Q_ARG(QVariant, QVariant(QUrl::fromLocalFile(video))),
                Q_ARG(QVariant, QVariant(QUrl())), Q_ARG(QVariant, QVariant(false))) ||
            !QTest::qWaitForWindowExposed(window)) {
            return false;
        }
        player = window->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"));
        lookup = window->findChild<PlayerLookupViewModel*>();
        if (player == nullptr || lookup == nullptr) {
            return false;
        }
        lookup->setSubtitleDocument(QUrl::fromLocalFile(QDir(root.path()).filePath("a.srt")),
                                    QByteArray("1\n00:00:00,000 --> 00:00:10,000\n星\n"), true);
        lookup->updatePlayback(1.0, 0.0);
        if (lookup->lookupAt(QStringLiteral("1"), 0).isEmpty() ||
            !QTest::qWaitFor([this] { return lookup->has_popup(); })) {
            return false;
        }
        view = web_view_in(window->contentItem());
        return view != nullptr && QTest::qWaitFor([this] { return view->isVisible(); });
    }
};

}  // namespace

void VideoLookupFocusTest::closing_the_lookup_returns_the_keyboard_to_the_player() {
    OpenLookup scene;
    QVERIFY2(scene.open(), "the lookup popup did not open");

    // The click on the popup's audio button focuses its web view.
    scene.view->forceActiveFocus();
    QTRY_VERIFY(scene.view->hasActiveFocus());

    scene.lookup->closeLookup();
    QTRY_VERIFY(!scene.lookup->has_popup());
    QTRY_VERIFY2(scene.player->hasActiveFocus(), "the player did not get the keyboard back");
}

// Focus outside the popup, such as an Inspector field, stays where it is.
void VideoLookupFocusTest::closing_the_lookup_leaves_focus_elsewhere_alone() {
    OpenLookup scene;
    QVERIFY2(scene.open(), "the lookup popup did not open");

    scene.view->forceActiveFocus();
    QTRY_VERIFY(scene.view->hasActiveFocus());
    QQuickItem field(scene.window->contentItem());
    field.forceActiveFocus();
    QTRY_VERIFY(field.hasActiveFocus());

    scene.lookup->closeLookup();
    QTRY_VERIFY(!scene.lookup->has_popup());
    QTest::qWait(50);
    QVERIFY2(field.hasActiveFocus(), "closing the lookup took the keyboard from the field");
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    VideoLookupFocusTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_video_lookup_focus.moc"

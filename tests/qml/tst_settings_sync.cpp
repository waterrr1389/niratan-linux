// SPDX-License-Identifier: GPL-3.0-or-later
// SyncView.swift: TTU connection, behaviour and data rows.

#include "../viewmodels/data_root.h"
#include "models/books/book.h"
#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"
#include "services/sync/oauth_credentials.h"

#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QPointer>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>

namespace {

QStringList warnings;
QtMessageHandler previous_handler = nullptr;

void collect_qml_messages(QtMsgType type, const QMessageLogContext& context,
                          const QString& message) {
    if ((type == QtWarningMsg || type == QtCriticalMsg) &&
        (QString::fromUtf8(context.category).startsWith("qml") || message.contains(".qml"))) {
        warnings.append(message);
    }
    if (previous_handler) {
        previous_handler(type, context, message);
    }
}

QString take_warnings() {
    QCoreApplication::processEvents();
    const auto report = warnings.join('\n');
    warnings.clear();
    return report;
}

QList<QQuickItem*> visual_items(QQuickItem* root) {
    QList<QQuickItem*> items{root};
    for (auto* child : root->childItems()) {
        items.append(visual_items(child));
    }
    return items;
}

}  // namespace

class SettingsSyncTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        QVERIFY(root_.isValid());
        qunsetenv("NIRATAN_GDRIVE_CLIENT_ID");
        previous_handler = qInstallMessageHandler(collect_qml_messages);
        const auto root = niratan::storage::data_root();
        QVERIFY(niratan::sync::CredentialsStore(root / "sync/credentials.json")
                    .save({"access", "refresh", "123-fixture.apps.googleusercontent.com", {}}));
        QFile settings(QString::fromStdString((root / "settings.json").string()));
        QVERIFY(settings.open(QIODevice::WriteOnly));
        const QByteArray data =
            "{\"googleClientId\":\"123-fixture.apps.googleusercontent.com\",\"enableSync\":true,"
            "\"enableStatistics\":true,\"enableSasayaki\":true,\"enableAutoSync\":true,"
            "\"statisticsEnableSync\":true,\"sasayakiEnableSync\":true}";
        QCOMPARE(settings.write(data), data.size());
    }

    void cleanupTestCase() { qInstallMessageHandler(previous_handler); }

    void init() { QVERIFY(show_page("SyncSettings")); }

    void cleanup() {
        window_.reset();
        const auto report = take_warnings();
        QVERIFY2(report.isEmpty(), qPrintable(report));
    }

    // B8: StatisticsSettingsView's ッツ Sync card, shown only while sync is on.
    void statistics_sync_card() {
        QVERIFY(show_page("StatisticsSettings"));
        auto* section = find("statisticsSyncSection");
        QVERIFY(section);
        QVERIFY(section->property("visible").toBool());
        QCOMPARE(
            section->property("footer").toString(),
            QString::fromUtf8("Determines if statistics will be merged entry by entry or replaced "
                              "completely on a sync."));
        auto* toggle = find("statisticsSyncToggle");
        QVERIFY(toggle);
        QCOMPARE(toggle->property("label").toString(), QStringLiteral("ッツ Sync"));
        QVERIFY(toggle->property("checked").toBool());
        QVERIFY(QMetaObject::invokeMethod(toggle, "toggled", false));
        QCOMPARE(settings().value("statisticsEnableSync"), QJsonValue(false));
        QVERIFY(QMetaObject::invokeMethod(toggle, "toggled", true));
        QCOMPARE(settings().value("statisticsEnableSync"), QJsonValue(true));

        auto* mode = find("statisticsSyncMode");
        QVERIFY(mode);
        QCOMPARE(mode->property("label").toString(), QStringLiteral("Sync Behaviour"));
        QCOMPARE(mode->property("model").toStringList(),
                 (QStringList{QStringLiteral("Merge"), QStringLiteral("Replace")}));
        QCOMPARE(mode->property("currentIndex").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(mode, "activated", 1));
        QCOMPARE(settings().value("statisticsSyncMode").toString(), QStringLiteral("Replace"));
        QCOMPARE(mode->property("currentIndex").toInt(), 1);

        auto* sync = engine_.singletonInstance<QObject*>("Niratan", "SyncSettingsViewModel");
        QVERIFY(sync);
        QVERIFY(sync->setProperty("enabled", false));
        QTRY_VERIFY(!section->property("visible").toBool());
        QVERIFY(sync->setProperty("enabled", true));
        QTRY_VERIFY(section->property("visible").toBool());
    }

    void loads_sync_controls_data() {
        QTest::addColumn<QString>("name");
        for (const auto* name :
             {"clientIdField", "syncDirection", "syncAuto", "syncUploadBooks", "syncStats",
              "syncAudioBook", "syncClearCache", "syncSignOut", "syncEnable"}) {
            QTest::newRow(name) << QString::fromLatin1(name);
        }
    }

    void loads_sync_controls() {
        QFETCH(QString, name);
        auto* control = window_->findChild<QQuickItem*>(name);
        QVERIFY2(control, qPrintable(name));
        QVERIFY(control->isVisible());
        if (name == "clientIdField") {
            QCOMPARE(control->property("text").toString(),
                     QStringLiteral("123-fixture.apps.googleusercontent.com"));
            QVERIFY(!control->isEnabled());
            QCOMPARE(control->property("placeholderText").toString(), QStringLiteral("Required"));
            QCOMPARE(control->opacity(), 0.6);
        }
    }

    void load_has_swift_texts() {
        QStringList texts;
        for (auto* item : visual_items(window_->contentItem())) {
            if (item->isVisible()) {
                texts.append(item->property("text").toString());
            }
        }
        for (const auto* value :
             {"Syncing", "Connection", "Behaviour", "Data", "Client ID", "Status", "Connected",
              "Direction", "Auto Sync", "Upload Books",
              "Uploads books on first sync if no bookdata is stored on Google Drive.", "Sync Stats",
              "Sync Audiobook Progress", "Clear Cache", "Sign out"}) {
            QVERIFY2(texts.contains(QString::fromUtf8(value)), value);
        }
        auto* section = find("syncSection");
        QVERIFY(section);
        QCOMPARE(
            section->property("footer").toString(),
            QString::fromUtf8(
                "Sync bookmarks and statistics with ッツ Reader per book via Google Drive.<br/>"
                "A <b><a "
                "href=\"https://github.com/ttu-ttu/"
                "ebook-reader?tab=readme-ov-file#storage-sources\">Google Cloud project</a></b> is "
                "necessary for syncing.<br/>"
                "1. After the initial setup, create another <b>OAuth client ID</b> in the same "
                "project.<br/>"
                "2. Select <b>Desktop app</b> as the <b>Application type</b>.<br/>"
                "3. Paste the <b>Client ID</b> in the textbox below and press '<b>Connect Google "
                "Drive</b>'.<br/>"
                "4. You can sync individual books by long-pressing and selecting "
                "'<b>Sync</b>'.<br/>"
                "<b><a "
                "href=\"https://github.com/Manhhao/Hoshi-Reader/blob/develop/TTUSYNC.md\">More...</"
                "a></b>"));
        auto* connect = find("syncConnect");
        QVERIFY(connect);
        QVERIFY(!connect->property("visible").toBool());
    }

    void enable_off_hides_sections() {
        auto* enable = find("syncEnable");
        QVERIFY(enable);
        QVERIFY(enable->property("checked").toBool());
        for (bool enabled : {false, true}) {
            QVERIFY(QMetaObject::invokeMethod(enable, "toggled", enabled));
            QCOMPARE(settings().value("enableSync"), QJsonValue(enabled));
            for (const auto* name :
                 {"syncConnectionSection", "syncBehaviourSection", "syncDataSection"}) {
                auto* section = find(name);
                QVERIFY(section);
                QCOMPARE(section->property("visible").toBool(), enabled);
            }
        }
    }

    void writes_direction_and_data_settings() {
        auto* direction = find("syncDirection");
        QVERIFY(direction);
        QVERIFY(QMetaObject::invokeMethod(direction, "activated", 1));
        QCOMPARE(settings().value("syncMode").toString(), QStringLiteral("Manual"));
        QVERIFY(QMetaObject::invokeMethod(direction, "activated", 0));
        QCOMPARE(settings().value("syncMode").toString(), QStringLiteral("Auto"));
        for (const auto& [name, key] : {std::pair{"syncAuto", "enableAutoSync"},
                                        {"syncUploadBooks", "syncUploadBooks"},
                                        {"syncStats", "statisticsEnableSync"},
                                        {"syncAudioBook", "sasayakiEnableSync"}}) {
            auto* row = find(name);
            QVERIFY(row);
            QVERIFY(QMetaObject::invokeMethod(row, "toggled", false));
            QCOMPARE(settings().value(QLatin1StringView(key)), QJsonValue(false));
        }
    }

    // BookCell's contextMenu: Sync follows Move, as an item in Auto mode and
    // an Import/Export submenu in Manual mode; reopening keeps it intact.
    void book_menu_sync_entry() {
        auto* sync = engine_.singletonInstance<QObject*>("Niratan", "SyncSettingsViewModel");
        QVERIFY(sync);
        QCOMPARE(sync->property("provider").toString(), QStringLiteral("ttu"));
        QQmlComponent component(&engine_);
        component.setData("import Niratan\nLibraryDialogs { model: LibraryViewModel {} }",
                          QUrl(QStringLiteral("qrc:/book_menu_test.qml")));
        QTRY_VERIFY(!component.isLoading());
        std::unique_ptr<QObject> dialogs(component.create());
        QVERIFY2(dialogs, qPrintable(component.errorString()));
        qobject_cast<QQuickItem*>(dialogs.get())->setParentItem(window_->contentItem());
        auto* menu = dialogs->findChild<QObject*>("bookMenu");
        QVERIFY(menu);
        const auto row_at = [menu](int index) {
            QQuickItem* row = nullptr;
            QMetaObject::invokeMethod(menu, "itemAt", qReturnArg(row), index);
            return row;
        };
        const auto open = [&] {
            QVERIFY(QMetaObject::invokeMethod(dialogs.get(), "showBookMenu",
                                              QVariant(QStringLiteral("book")),
                                              QVariant(QStringLiteral("Title"))));
            QTRY_VERIFY(menu->property("opened").toBool());
        };
        for (const auto& mode :
             {QStringLiteral("Auto"), QStringLiteral("Manual"), QStringLiteral("Manual"),
              QStringLiteral("Auto"), QStringLiteral("Auto")}) {
            QVERIFY(sync->setProperty("syncMode", mode));
            open();
            QCOMPARE(menu->property("count").toInt(), 5);
            auto* row = row_at(1);
            QVERIFY(row);
            QCOMPARE(row->property("text").toString(), QStringLiteral("Sync"));
            auto* sub = row->property("subMenu").value<QObject*>();
            QCOMPARE(sub != nullptr, mode == QStringLiteral("Manual"));
            if (sub) {
                QCOMPARE(sub->property("count").toInt(), 2);
            }
            QVERIFY(QMetaObject::invokeMethod(menu, "close"));
            QTRY_VERIFY(!menu->property("visible").toBool());
        }
        // Reopening the menu, as for another book, keeps the Sync row live:
        // the pointer passing down from Move highlights it, and Down from
        // Move selects it and then the next action.
        const auto hover = [&](QQuickItem* row) {
            QTest::mouseMove(
                window_.get(),
                row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
            QTest::qWait(100);
        };
        const auto key_down = [&] {
            QTest::keyClick(window_.get(), Qt::Key_Down);
            return row_at(menu->property("currentIndex").toInt());
        };
        for (const auto& mode : {QStringLiteral("Auto"), QStringLiteral("Auto"),
                                 QStringLiteral("Manual"), QStringLiteral("Auto")}) {
            QVERIFY(sync->setProperty("syncMode", mode));
            open();
            auto* move = row_at(0);
            auto* row = row_at(1);
            auto* next = row_at(2);
            QVERIFY(move && row && next);
            QVERIFY(row->y() > move->y() && next->y() > row->y());
            hover(move);
            hover(row);
            QVERIFY2(row->property("hovered").toBool(), qPrintable(mode));
            hover(next);
            QVERIFY(next->property("hovered").toBool());
            menu->setProperty("currentIndex", 0);
            QCOMPARE(key_down(), row);
            // The next enabled action after Sync.
            QQuickItem* after = nullptr;
            for (int i = 2; i < menu->property("count").toInt() && !after; ++i) {
                auto* candidate = row_at(i);
                if (candidate && candidate->property("enabled").toBool()) {
                    after = candidate;
                }
            }
            QVERIFY(after);
            QCOMPARE(key_down(), after);
            QVERIFY(QMetaObject::invokeMethod(menu, "close"));
            QTRY_VERIFY(!menu->property("visible").toBool());
        }
        // Reopening for another book while the Manual submenu is open closes
        // that submenu and shows the new entry.
        QVERIFY(sync->setProperty("syncMode", QStringLiteral("Manual")));
        open();
        menu->setProperty("currentIndex", 0);
        QVERIFY(key_down());
        QPointer<QObject> previous = row_at(1)->property("subMenu").value<QObject*>();
        QVERIFY(previous);
        QTest::keyClick(window_.get(), Qt::Key_Right);
        QTRY_VERIFY(previous->property("visible").toBool());
        QVERIFY(sync->setProperty("syncMode", QStringLiteral("Auto")));
        QVERIFY(QMetaObject::invokeMethod(dialogs.get(), "showBookMenu",
                                          QVariant(QStringLiteral("other")),
                                          QVariant(QStringLiteral("Other"))));
        QTRY_VERIFY(previous.isNull() || !previous->property("visible").toBool());
        QTRY_VERIFY(menu->property("opened").toBool());
        QVERIFY(row_at(1));
        QCOMPARE(row_at(1)->property("text").toString(), QStringLiteral("Sync"));
        QVERIFY(row_at(1)->property("subMenu").value<QObject*>() == nullptr);
        QVERIFY(QMetaObject::invokeMethod(menu, "close"));
        QTRY_VERIFY(!menu->property("visible").toBool());
        QVERIFY(sync->setProperty("enabled", false));
        open();
        QCOMPARE(menu->property("count").toInt(), 4);
        QVERIFY(QMetaObject::invokeMethod(menu, "close"));
        QVERIFY(sync->setProperty("enabled", true));
    }

    // The Move submenu checks and disables the book's shelf, None for a book
    // on no shelf, and follows a move on the next popup.
    void book_menu_marks_current_shelf() {
        const niratan::books::BookStorage storage(niratan::storage::data_root());
        for (const std::string folder : {"shelf-a", "shelf-b"}) {
            niratan::books::BookMetadata book;
            book.id = "ID-" + folder;
            book.title = folder;
            book.folder = folder;
            QVERIFY(storage.save_metadata(book));
        }
        QVERIFY(
            storage.save_shelves({niratan::books::BookShelf{"Novel", {"ID-shelf-a", "ID-shelf-b"}},
                                  niratan::books::BookShelf{"Other", {}}}));
        QQmlComponent component(&engine_);
        component.setData("import Niratan\nLibraryDialogs { model: LibraryViewModel {} }",
                          QUrl(QStringLiteral("qrc:/shelf_menu_test.qml")));
        QTRY_VERIFY(!component.isLoading());
        std::unique_ptr<QObject> dialogs(component.create());
        QVERIFY2(dialogs, qPrintable(component.errorString()));
        qobject_cast<QQuickItem*>(dialogs.get())->setParentItem(window_->contentItem());
        auto* menu = dialogs->findChild<QObject*>("bookMenu");
        QVERIFY(menu);
        // Names of the checked rows of the Move submenu; each checked row is
        // also disabled.
        const auto checked_rows = [&](const QString& folder) {
            QStringList names;
            [&] {
                QVERIFY(QMetaObject::invokeMethod(dialogs.get(), "showBookMenu", QVariant(folder),
                                                  QVariant(folder)));
                QTRY_VERIFY(menu->property("opened").toBool());
                QQuickItem* first = nullptr;
                QMetaObject::invokeMethod(menu, "itemAt", qReturnArg(first), 0);
                QVERIFY(first);
                auto* move = first->property("subMenu").value<QObject*>();
                QVERIFY(move);
                for (int i = 0; i < move->property("count").toInt(); ++i) {
                    QQuickItem* row = nullptr;
                    QMetaObject::invokeMethod(move, "itemAt", qReturnArg(row), i);
                    QVERIFY(row);
                    if (row->property("checked").toBool()) {
                        QVERIFY(!row->property("enabled").toBool());
                        names.append(row->property("text").toString());
                    } else {
                        QVERIFY(row->property("enabled").toBool());
                    }
                }
                QVERIFY(QMetaObject::invokeMethod(menu, "close"));
                QTRY_VERIFY(!menu->property("visible").toBool());
            }();
            return names;
        };
        // Chooses a destination from the Move submenu, as the user does.
        const auto choose = [&](const QString& folder, const QString& name) {
            QVERIFY(QMetaObject::invokeMethod(dialogs.get(), "showBookMenu", QVariant(folder),
                                              QVariant(folder)));
            QTRY_VERIFY(menu->property("opened").toBool());
            QQuickItem* first = nullptr;
            QMetaObject::invokeMethod(menu, "itemAt", qReturnArg(first), 0);
            QVERIFY(first);
            auto* move = first->property("subMenu").value<QObject*>();
            QVERIFY(move);
            QQuickItem* target = nullptr;
            for (int i = 0; i < move->property("count").toInt() && !target; ++i) {
                QQuickItem* row = nullptr;
                QMetaObject::invokeMethod(move, "itemAt", qReturnArg(row), i);
                if (row && row->property("text").toString() == name) {
                    target = row;
                }
            }
            QVERIFY(target);
            QVERIFY(target->property("enabled").toBool());
            QVERIFY(QMetaObject::invokeMethod(target, "triggered"));
            QVERIFY(QMetaObject::invokeMethod(menu, "close"));
            QTRY_VERIFY(!menu->property("visible").toBool());
        };
        QCOMPARE(checked_rows(QStringLiteral("shelf-b")), QStringList{QStringLiteral("Novel")});
        QCOMPARE(checked_rows(QStringLiteral("shelf-a")), QStringList{QStringLiteral("Novel")});
        choose(QStringLiteral("shelf-a"), QStringLiteral("Other"));
        QCOMPARE(checked_rows(QStringLiteral("shelf-a")), QStringList{QStringLiteral("Other")});
        choose(QStringLiteral("shelf-a"), QStringLiteral("None"));
        QCOMPARE(checked_rows(QStringLiteral("shelf-a")), QStringList{QStringLiteral("None")});
        QCOMPARE(checked_rows(QStringLiteral("shelf-b")), QStringList{QStringLiteral("Novel")});
    }

    void fits_default_window() {
        QVERIFY(find("syncDataSection"));
        for (auto* item : visual_items(window_->contentItem())) {
            if (!item->isVisible() || !item->inherits("QQuickControl") ||
                item->boundingRect().isEmpty()) {
                continue;
            }
            const auto rect = item->mapRectToScene(item->boundingRect());
            QVERIFY2(rect.left() >= 0 && rect.right() <= window_->width(),
                     item->metaObject()->className());
        }
    }

    void alerts_and_client_edits() {
        const auto before = settings();
        auto* clear = find("syncClearCache");
        QVERIFY(clear);
        QVERIFY(QMetaObject::invokeMethod(clear, "clicked"));
        auto* alert = find("syncClearCacheAlert");
        QVERIFY(alert);
        QCOMPARE(alert->property("acceptText").toString(), QStringLiteral("Clear"));
        QCOMPARE(alert->property("rejectText").toString(), QStringLiteral("Cancel"));
        QTRY_VERIFY(alert->property("visible").toBool());
        QCOMPARE(alert->property("title").toString(), QStringLiteral("Clear Cache?"));
        QCOMPARE(alert->property("message").toString(),
                 QStringLiteral("This will clear cached folder ids and book covers."));
        QVERIFY(alert->property("acceptDestructive").toBool());
        QVERIFY(QMetaObject::invokeMethod(alert, "accept"));
        const auto credentials = niratan::storage::data_root() / "sync/credentials.json";
        QVERIFY(std::filesystem::exists(credentials));
        QCOMPARE(settings(), before);
        auto* sign_out = find("syncSignOut");
        QVERIFY(sign_out);
        QVERIFY(QMetaObject::invokeMethod(sign_out, "clicked"));
        alert = find("syncSignOutAlert");
        QVERIFY(alert);
        QCOMPARE(alert->property("acceptText").toString(), QStringLiteral("Confirm"));
        QCOMPARE(alert->property("rejectText").toString(), QStringLiteral("Cancel"));
        QTRY_VERIFY(alert->property("visible").toBool());
        QCOMPARE(alert->property("title").toString(), QStringLiteral("Sign out?"));
        QCOMPARE(
            alert->property("message").toString(),
            QStringLiteral(
                "Signing out will clear authorization tokens, cached folder ids and book covers."));
        QVERIFY(alert->property("acceptDestructive").toBool());
        QVERIFY(QMetaObject::invokeMethod(alert, "accept"));
        QVERIFY(!std::filesystem::exists(credentials));
        auto* connect = find("syncConnect");
        QVERIFY(connect && connect->property("visible").toBool());
        auto* status = find("syncStatus");
        QVERIFY(status);
        QCOMPARE(status->property("text").toString(), QStringLiteral("Not connected"));
        auto* field = find("clientIdField");
        QVERIFY(field && field->property("enabled").toBool());
        for (const auto& text : {QStringLiteral("456-other.apps.googleusercontent.com"),
                                 QStringLiteral(" invalid ")}) {
            QVERIFY(field->setProperty("text", text));
            QVERIFY(QMetaObject::invokeMethod(field, "textEdited"));
            QCOMPARE(settings().value("googleClientId").toString(), text);
        }
        QVERIFY(QMetaObject::invokeMethod(connect, "clicked"));
        alert = find("syncErrorAlert");
        QVERIFY(alert);
        QCOMPARE(alert->property("title").toString(), QStringLiteral("Error"));
        QCOMPARE(alert->property("acceptText").toString(), QStringLiteral("OK"));
        QTRY_VERIFY(alert->property("visible").toBool());
        QCOMPARE(alert->property("message").toString(), QStringLiteral("Invalid Client ID format"));
        QVERIFY(QMetaObject::invokeMethod(alert, "accept"));
        QCOMPARE(alert->property("message").toString(), QString{});
    }

    void hides_disabled_feature_rows() {
        auto object = settings();
        object.insert("enableStatistics", false);
        object.insert("enableSasayaki", false);
        QFile file(
            QString::fromStdString((niratan::storage::data_root() / "settings.json").string()));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const auto data = QJsonDocument(object).toJson();
        QCOMPARE(file.write(data), data.size());
        file.close();
        window_.reset();
        init();
        for (const auto* name : {"syncStats", "syncAudioBook"}) {
            auto* row = find(name);
            QVERIFY(!row || !row->property("visible").toBool());
        }
        QVERIFY(find("syncDataSection"));
    }

   private:
    QObject* find(const QString& name) const { return window_->findChild<QObject*>(name); }

    bool show_page(const char* module) {
        window_ = std::make_unique<QQuickWindow>();
        window_->resize(960, 640);
        QQmlComponent component(&engine_);
        component.loadFromModule("Niratan", module);
        for (int waited = 0; waited < 200 && component.isLoading(); waited += 10) {
            QTest::qWait(10);
        }
        if (component.isLoading()) {
            return false;
        }
        auto* page = component.beginCreate(engine_.rootContext());
        if (page == nullptr) {
            return false;
        }
        page->setParent(window_.get());
        auto* item = qobject_cast<QQuickItem*>(page);
        if (item == nullptr) {
            return false;
        }
        item->setParentItem(window_->contentItem());
        item->setSize(QSizeF(window_->size()));
        component.completeCreate();
        if (component.isError()) {
            return false;
        }
        window_->show();
        return QTest::qWaitForWindowExposed(window_.get());
    }
    QJsonObject settings() const {
        QFile file(
            QString::fromStdString((niratan::storage::data_root() / "settings.json").string()));
        return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object()
                                              : QJsonObject{};
    }
    niratan::test::TempDataRoot root_;
    QQmlEngine engine_;
    std::unique_ptr<QQuickWindow> window_;
};

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));
    SettingsSyncTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_settings_sync.moc"

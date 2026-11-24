// SPDX-License-Identifier: GPL-3.0-or-later
// SyncView.swift: TTU connection, behaviour and data rows.

#include "../viewmodels/data_root.h"
#include "services/storage/app_paths.h"
#include "services/sync/oauth_credentials.h"

#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>

Q_IMPORT_QML_PLUGIN(NiratanPlugin)
Q_IMPORT_QML_PLUGIN(NiratanStylePlugin)

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

    void init() {
        window_ = std::make_unique<QQuickWindow>();
        window_->resize(960, 640);
        QQmlComponent component(&engine_);
        component.loadFromModule("Niratan", "SyncSettings");
        QTRY_VERIFY(!component.isLoading());
        auto* page = component.beginCreate(engine_.rootContext());
        QVERIFY2(page, qPrintable(component.errorString()));
        page->setParent(window_.get());
        auto* item = qobject_cast<QQuickItem*>(page);
        QVERIFY(item);
        item->setParentItem(window_->contentItem());
        item->setSize(QSizeF(window_->size()));
        component.completeCreate();
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_.get()));
    }

    void cleanup() {
        window_.reset();
        const auto report = take_warnings();
        QVERIFY2(report.isEmpty(), qPrintable(report));
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

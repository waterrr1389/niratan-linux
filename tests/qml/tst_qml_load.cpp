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

#include <QGuiApplication>
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

Q_IMPORT_QML_PLUGIN(NiratanPlugin)

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

}  // namespace

class QmlLoadTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void main_window_loads();
    void page_components_load();

   private:
    // Empty when the component loaded without a QML warning.
    QString warnings_after(const QString& label);

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
        QStringLiteral("DictionaryPage"), QStringLiteral("LibraryPage"),
        QStringLiteral("PlayerPage"),     QStringLiteral("ProfilesPage"),
        QStringLiteral("ReaderPage"),     QStringLiteral("SettingsPage"),
        QStringLiteral("StatisticsPage"),
    };

    QQuickWindow window;
    window.resize(960, 640);

    for (const QString& page : pages) {
        const QUrl url(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/%1.qml").arg(page));
        QQmlComponent component(&engine_, url);
        QVERIFY2(!component.isError(),
                 qPrintable(page + QStringLiteral(": ") + error_text(component)));

        QScopedPointer<QObject> object(component.beginCreate(engine_.rootContext()));
        QVERIFY2(object != nullptr,
                 qPrintable(page + QStringLiteral(": ") + error_text(component)));

        auto* item = qobject_cast<QQuickItem*>(object.data());
        QVERIFY2(item != nullptr, qPrintable(page + QStringLiteral(" is not an Item")));
        item->setParentItem(window.contentItem());
        item->setWidth(960);
        item->setHeight(640);
        component.completeCreate();

        const QString warnings = warnings_after(page);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
        object.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
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
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QmlLoadTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_qml_load.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The interface appearance follows Core/UserConfig.swift
// `preferredColorScheme`, which NativeMac/HoshiNativeMacApp.swift applies to
// the main window with the system appearance as the fallback: the Light and
// Dark reader themes force their scheme, Custom takes the Interface picker's
// (`uiTheme`), and System and Sepia follow the desktop.
//
// The main window binds ThemeState to the reader model, which watches the
// active profile's settings file, so the settings are written through a
// separate SettingsViewModel the way the Settings page writes them. The
// offscreen platform reports no colour scheme, which resolves to light.

#include "../viewmodels/data_root.h"
#include "viewmodels/settings/settings_view_model.h"
#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>
#include <memory>

class InterfaceThemeTest : public QObject {
    Q_OBJECT

   private slots:
    void reader_theme_drives_the_interface();
};

void InterfaceThemeTest::reader_theme_drives_the_interface() {
    qunsetenv("NIRATAN_THEME");
    niratan::test::TempDataRoot data;
    QVERIFY(data.isValid());

    QQmlApplicationEngine engine;
    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    auto* window = qobject_cast<QQuickWindow*>(object.get());
    QVERIFY2(window != nullptr, "MainWindow was not created");

    auto* theme =
        engine.singletonInstance<QObject*>(QStringLiteral("Niratan"), QStringLiteral("ThemeState"));
    QVERIFY2(theme != nullptr, "ThemeState is not reachable");
    QVERIFY2(!theme->property("dark").toBool(), "the default System theme is not light");

    niratan::viewmodels::SettingsViewModel settings;
    settings.setTheme(QStringLiteral("Dark"));
    QTRY_VERIFY2(theme->property("dark").toBool(),
                 "the Dark reader theme left the interface light");

    settings.setTheme(QStringLiteral("Light"));
    QTRY_VERIFY2(!theme->property("dark").toBool(), "the Light reader theme left it dark");

    settings.setTheme(QStringLiteral("Sepia"));
    settings.setUiTheme(QStringLiteral("Dark"));
    QTest::qWait(300);
    QVERIFY2(!theme->property("dark").toBool(), "Sepia did not follow the system appearance");

    settings.setTheme(QStringLiteral("Custom"));
    QTRY_VERIFY2(theme->property("dark").toBool(), "Custom did not take the Interface picker");

    settings.setUiTheme(QStringLiteral("Light"));
    QTRY_VERIFY2(!theme->property("dark").toBool(), "Custom with a Light interface stayed dark");
}

int main(int argc, char** argv) {
    niratan::ui::register_popup_url_schemes();
    niratan::ui::register_reader_url_schemes();
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));
    InterfaceThemeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_interface_theme.moc"

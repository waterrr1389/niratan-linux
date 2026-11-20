// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>

namespace {

// Debug stand-in for the desktop appearance, mirroring Niratan's
// HOSHI_DEBUG_SYSTEM_APPEARANCE. It is not a stored setting.
void apply_debug_color_scheme() {
    const QByteArray requested = qgetenv("NIRATAN_THEME");
    if (requested == "light") {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    } else if (requested == "dark") {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Niratan");
    QGuiApplication::setOrganizationName("niratan");

    // Basic keeps the platform style out of later specs; M1 replaces it
    // with theme-drawn controls.
    QQuickStyle::setStyle("Basic");
    apply_debug_color_scheme();

    const bool theme_preview = qEnvironmentVariableIsSet("NIRATAN_THEME_PREVIEW");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Niratan", theme_preview ? "ThemePreview" : "MainWindow");

    return app.exec();
}

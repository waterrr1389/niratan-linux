// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>

#include <clocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStyleHints>
#include <QWebEngineProfile>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include "viewmodels/reader/reader_view_model.h"
#include "views/dictionary/audio_scheme_handler.h"
#include "views/dictionary/dictionary_media_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"

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
    // The video surface renders libmpv into a QQuickFramebufferObject, which
    // needs the OpenGL RHI backend. Headless platforms have no GL context, so
    // the choice only applies when a real windowing system is in use, and the
    // user may override it.
    const QByteArray platform = qgetenv("QT_QPA_PLATFORM");
    const bool headless = platform == "offscreen" || platform == "minimal"
        || platform == "vnc";
    if (!headless && !qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        qputenv("QSG_RHI_BACKEND", "opengl");
    }
    niratan::ui::register_popup_url_schemes();
    niratan::ui::register_reader_url_schemes();
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    // libmpv rejects a non-C numeric locale; Qt keeps its own locale for
    // formatting, so pinning the C library's numeric locale is safe.
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName("Niratan");
    QGuiApplication::setOrganizationName("niratan");

    // Basic keeps the platform style out of later specs; M1 replaces it
    // with theme-drawn controls.
    QQuickStyle::setStyle("Basic");
    apply_debug_color_scheme();

    const bool theme_preview = qEnvironmentVariableIsSet("NIRATAN_THEME_PREVIEW");

    QWebEngineProfile* profile = QWebEngineProfile::defaultProfile();
    profile->installUrlSchemeHandler(
        QByteArrayLiteral("audio"),
        new niratan::ui::AudioSchemeHandler(profile));
    profile->installUrlSchemeHandler(QByteArrayLiteral("dict"),
                                    new niratan::ui::DictionaryMediaSchemeHandler(profile));
    auto* reader_model = niratan::viewmodels::ReaderViewModel::instance();
    auto* reader_handler = new niratan::ui::ReaderSchemeHandler(profile);
    reader_handler->set_view_model(reader_model);
    profile->installUrlSchemeHandler(QByteArrayLiteral("reader"), reader_handler);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Niratan", theme_preview ? "ThemePreview" : "MainWindow");

    return app.exec();
}

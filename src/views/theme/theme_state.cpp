// SPDX-License-Identifier: GPL-3.0-or-later

#include "theme_state.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

namespace niratan::ui {

ThemeState::ThemeState(QObject* parent) : QObject(parent) {
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) {
                emit systemDarkChanged();
                if (ui_theme_ != QStringLiteral("Dark") && ui_theme_ != QStringLiteral("Light")) {
                    emit darkChanged();
                }
            });
    // Qt 6.12 asks for QEvent::ApplicationPaletteChange instead of the
    // deprecated paletteChanged signal.
    QGuiApplication::instance()->installEventFilter(this);
}

bool ThemeState::isDark() const {
    if (ui_theme_ == QStringLiteral("Dark")) {
        return true;
    }
    if (ui_theme_ == QStringLiteral("Light")) {
        return false;
    }
    return isSystemDark();
}

bool ThemeState::isSystemDark() const {
    // NIRATAN_THEME stands in for the desktop appearance, like Niratan's
    // HOSHI_DEBUG_SYSTEM_APPEARANCE. It is read here rather than only applied
    // to QStyleHints, because platforms such as offscreen and vnc ignore a
    // requested color scheme. An explicit profile theme still wins in isDark.
    const QByteArray requested = qgetenv("NIRATAN_THEME");
    if (requested == "dark") {
        return true;
    }
    if (requested == "light") {
        return false;
    }
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

QString ThemeState::ui_theme() const { return ui_theme_; }

void ThemeState::setUiTheme(const QString& theme) {
    if (ui_theme_ != theme) {
        const bool was_dark = isDark();
        ui_theme_ = theme;
        emit uiThemeChanged();
        if (was_dark != isDark()) {
            emit darkChanged();
        }
    }
}

QColor ThemeState::accent() const {
    return QGuiApplication::palette().color(QPalette::Highlight);
}

bool ThemeState::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::ApplicationPaletteChange) {
        emit accentChanged();
    }
    return QObject::eventFilter(watched, event);
}

}  // namespace niratan::ui

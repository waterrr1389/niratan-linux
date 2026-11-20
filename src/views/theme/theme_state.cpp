// SPDX-License-Identifier: GPL-3.0-or-later

#include "theme_state.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

namespace niratan::ui {

ThemeState::ThemeState(QObject* parent) : QObject(parent) {
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { emit darkChanged(); });
    // Qt 6.12 asks for QEvent::ApplicationPaletteChange instead of the
    // deprecated paletteChanged signal.
    QGuiApplication::instance()->installEventFilter(this);
}

bool ThemeState::isDark() const {
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
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

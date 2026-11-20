// SPDX-License-Identifier: GPL-3.0-or-later

#include "theme_state.h"

#include <QGuiApplication>
#include <QStyleHints>

namespace niratan::ui {

ThemeState::ThemeState(QObject* parent) : QObject(parent) {
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { emit darkChanged(); });
}

bool ThemeState::isDark() const {
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

}  // namespace niratan::ui

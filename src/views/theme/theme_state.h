// SPDX-License-Identifier: GPL-3.0-or-later
//
// The appearance the theme tokens switch on, following
// Core/UserConfig.swift (preferredColorScheme) and
// NativeMac/HoshiNativeMacApp.swift (the system appearance fallback). It only
// reports what QStyleHints says; the palette lives in Theme.qml.

#pragma once

#include <QColor>
#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace niratan::ui {

class ThemeState : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool dark READ isDark NOTIFY darkChanged)
    // The platform accent, from the system palette's Highlight role.
    Q_PROPERTY(QColor accent READ accent NOTIFY accentChanged)

   public:
    explicit ThemeState(QObject* parent = nullptr);

    bool isDark() const;
    QColor accent() const;

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

   signals:
    void darkChanged();
    void accentChanged();
};

}  // namespace niratan::ui

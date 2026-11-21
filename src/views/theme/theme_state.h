// SPDX-License-Identifier: GPL-3.0-or-later
//
// The appearance the theme tokens switch on, following
// Core/UserConfig.swift (preferredColorScheme) and
// NativeMac/HoshiNativeMacApp.swift (the system appearance fallback). The
// palette lives in Theme.qml.

#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace niratan::ui {

class ThemeState : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool dark READ isDark NOTIFY darkChanged)
    Q_PROPERTY(bool systemDark READ isSystemDark NOTIFY systemDarkChanged)
    Q_PROPERTY(QString uiTheme READ ui_theme WRITE setUiTheme NOTIFY uiThemeChanged)
    // The platform accent, from the system palette's Highlight role.
    Q_PROPERTY(QColor accent READ accent NOTIFY accentChanged)

   public:
    explicit ThemeState(QObject* parent = nullptr);

    bool isDark() const;
    bool isSystemDark() const;
    QString ui_theme() const;
    void setUiTheme(const QString& theme);
    QColor accent() const;

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

   signals:
    void darkChanged();
    void systemDarkChanged();
    void uiThemeChanged();
    void accentChanged();

   private:
    QString ui_theme_ = QStringLiteral("System");
};

}  // namespace niratan::ui

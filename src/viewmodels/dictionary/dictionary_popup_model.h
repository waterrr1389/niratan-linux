// SPDX-License-Identifier: GPL-3.0-or-later
//
// State and commands for the dictionary page, following
// Features/Dictionary/DictionarySearchView.swift (performLookup and
// constructHtml): trim the query, look it up with the profile's result and
// scan limits, drop an empty result, and otherwise build the popup shell from
// the entries, the dictionary styles, the audio sources and the theme.
//
// The model never reads files, opens sockets or calls hoshidicts itself. The
// installed dictionary list, the lookup, the media-independent styles and the
// popup assets all come from services. Lookup runs synchronously: the library
// query is in-memory and the page has a single user.
//
// The theme properties are pushed from QML's Theme singleton, because colors
// outside src/views/theme are not allowed.

#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::viewmodels {

class DictionaryPopupModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString html READ html NOTIFY htmlChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool hasResults READ has_results NOTIFY htmlChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY themeChanged)
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY themeChanged)

   public:
    explicit DictionaryPopupModel(QObject* parent = nullptr);
    ~DictionaryPopupModel() override;

    QString html() const;
    QString error() const;
    bool has_results() const;

    bool dark() const;
    void setDark(bool dark);
    QColor background() const;
    void setBackground(QColor background);
    QColor textColor() const;
    void setTextColor(QColor text_color);

    // Empty or whitespace-only input clears the view.
    Q_INVOKABLE void lookup(const QString& text);
    Q_INVOKABLE void clear();

   signals:
    void htmlChanged();
    void errorChanged();
    void themeChanged();

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::viewmodels

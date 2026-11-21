// SPDX-License-Identifier: GPL-3.0-or-later
//
// State and commands for the dictionary page, following
// Features/Dictionary/DictionarySearchView.swift (performLookup and
// constructHtml): trim the query, look it up with the profile's result and
// scan limits, drop an empty result, and otherwise build the popup shell from
// the entries, the dictionary styles, the audio sources and the theme.
//
// Core/UserConfig.swift (apply(dictionaryProfileSettings:)) supplies the
// profile's resolved defaults. Content language, maxResults and scanLength
// reach both the lookup and the popup HTML. collapseMode,
// expandFirstDictionary, twoColumnLayout, compactGlossaries, showExpressionTags,
// harmonicFrequency, deduplicatePitchAccents, compactPitchAccents and customCSS
// affect only the popup HTML. Audio and mining retain their existing defaults.
// scanNonJapaneseText has no selection bridge on this page and is not a
// typed-lookup parameter. dictionaryTabDefault, frequencySortOrder,
// frequencySortDictionary and searchTextSize have no runtime consumer here.
//
// settings::DictionarySettingsWatch supplies the active profile's language
// and settings and reports edits from other windows, which rerun the last
// lookup. An invalid live edit retains the last good settings for the same
// profile; a missing file or an invalid initial file uses defaults.
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
    void run_lookup();
    void clear_results();

    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::viewmodels

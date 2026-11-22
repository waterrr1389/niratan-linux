// SPDX-License-Identifier: GPL-3.0-or-later
//
// State and commands for the dictionary page, following
// Features/Dictionary/DictionarySearchView.swift (performLookup and
// constructHtml): trim the query, look it up with the profile's result and
// scan limits, drop an empty result, and otherwise build the popup shell from
// the entries, the dictionary styles, the audio sources and the theme.
//
// Core/UserConfig.swift (apply(dictionaryProfileSettings:)) supplies the
// profile's resolved defaults. Content language, maxResults, scanLength,
// scanNonJapaneseText and searchTextSize reach the popup page and shell.
// collapseMode, expandFirstDictionary, twoColumnLayout, compactGlossaries,
// showExpressionTags, harmonicFrequency, deduplicatePitchAccents,
// compactPitchAccents and customCSS affect only the popup HTML. Audio and
// mining retain their existing defaults.
//
// Core/DictionaryManager.swift supplies the two dictionary sets the shell
// carries: collapsedDictionaries (only while collapseMode is Custom, read
// through services/dictionary load_collapsed_dictionaries) and
// excludedDictionaries (the active profile's resolved term dictionaries whose
// category is exclude, in list order, disabled entries included). Both are
// resolved again for every render.
//
// The query is the active profile's LookupConfiguration
// (services/dictionary active_lookup_configuration), rebuilt whenever the
// resolved configuration differs from the one the current query was built
// with. A profile switch, a dictionary_config.json edit and a frequency sort
// setting therefore reach the next lookup on the same model.
//
// settings::DictionarySettingsWatch supplies the active profile's language
// and settings and reports edits to its dictionary_settings.json,
// dictionary_config.json and collapsed.json from other windows, which rerun
// the last lookup. An invalid live edit retains the last good settings for
// the same profile; a missing file or an invalid initial file uses defaults.
//
// popup.js answers its own inline lookups through four further commands
// (PopupWebView.Coordinator.userContentController and
// DictionarySearchView.onRedirect/onQueryTextSelected): lookupRedirect,
// queryTextSelected, getEntries and textSelected. The first three run the same
// lookup pipeline and replace the stored entry maps while leaving the
// document, the matched text and the last query alone, because the script
// renders the returned entries in place and fetches them with getEntries.
// textSelected runs a normal lookup instead: the port has no child popups, so
// the popup document is replaced. matchedCharacterCount is the Unicode scalar
// count of the first entry's matched text (Swift's matchedText.count, which
// counts grapheme clusters).
//
// Features/Popup/WordAudioPlayer.swift (play) replaces the current clip with
// the resolved URL. The web script resolves the configured sources; the
// model accepts only HTTP(S) clip URLs and reports player errors to the view.
// PopupWebView.swift (showAudioSourceMenu, audioSourceSelected) keeps the
// selection in the current web document; the model relays names and indices.
// stopWordAudio stops the current clip, which the popup's Sasayaki controls do
// before playing an audiobook cue (WordAudioPlayer.shared.stop()).
//
// The theme properties are pushed from QML's Theme singleton, because colors
// outside src/views/theme are not allowed.

#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::viewmodels {

class DictionaryPopupModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString html READ html NOTIFY htmlChanged)
    Q_PROPERTY(QString bridgeScript READ bridge_script CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString audioError READ audio_error NOTIFY audioErrorChanged)
    Q_PROPERTY(QString matchedText READ matched_text NOTIFY htmlChanged)
    Q_PROPERTY(bool hasResults READ has_results NOTIFY htmlChanged)
    Q_PROPERTY(int searchTextSize READ search_text_size NOTIFY searchTextSizeChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY themeChanged)
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY themeChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY themeChanged)

   public:
    explicit DictionaryPopupModel(QObject* parent = nullptr);
    ~DictionaryPopupModel() override;

    QString html() const;
    QString bridge_script() const;
    QString error() const;
    QString audio_error() const;
    QString matched_text() const;
    bool has_results() const;
    // The profile's resolved searchTextSize, 26 when absent.
    int search_text_size() const;

    bool dark() const;
    void setDark(bool dark);
    QColor background() const;
    void setBackground(QColor background);
    QColor textColor() const;
    void setTextColor(QColor text_color);

    // Empty or whitespace-only input clears the view.
    Q_INVOKABLE void lookup(const QString& text);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void playWordAudio(const QString& audioUrl);
    Q_INVOKABLE void stopWordAudio();
    Q_INVOKABLE void showAudioSourceMenu(const QVariantMap& data);
    Q_INVOKABLE void selectAudioSource(int entryIndex, int sourceIndex);
    // The popup's inline lookups; see the header comment for how they differ.
    Q_INVOKABLE int lookupRedirect(const QString& query);
    Q_INVOKABLE QVariantMap queryTextSelected(const QString& text);
    Q_INVOKABLE QVariantList getEntries(int start, int count);
    Q_INVOKABLE int textSelected(const QString& text);

   signals:
    void htmlChanged();
    void errorChanged();
    void audioErrorChanged();
    void searchTextSizeChanged();
    void audioSourceMenuRequested(const QStringList& names, int selected, double x, double y,
                                  int entryIndex);
    void audioSourceSelected(int entryIndex, int sourceIndex);
    void themeChanged();

   private:
    void run_lookup();
    void clear_results();
    void set_audio_error(const QString& error);

    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::viewmodels

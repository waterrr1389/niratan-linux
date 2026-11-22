// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows AppearanceView.nativeAppearanceContent, DictionaryBehaviorSettingsSections
// and StatisticsSettingsView.resetTimeBinding in Features/Settings, and
// UserConfig.init/apply/profile snapshots in Core/UserConfig.swift. Reader and
// dictionary settings belong to the profile active when the page opens; the
// three statistics keys are global. Older optional reader fields use Swift's
// legacy furigana and count flags or the corresponding defaults. Enum values
// are the untranslated Swift raw strings; only the UI labels are translated.
// The text reveal speed defaults to 35. Bounded reader setters clamp to the
// range and round to the step of their AppearanceView control, so a stored
// value is always one the control can show; a value read from a file is not
// re-bounded.
// Changes are saved on edit and do not reload any running consumer. Consumers
// should load the reader preferences on book open and the reset minute on
// dashboard refresh rather than subscribing to settings-change signals.
//
// The Popup card of AppearanceView.nativeAppearanceContent writes the eight
// popup preferences: Width 100...1400 step 10, Height 100...800 step 10,
// Scale 0.8...1.5 step 0.05, Swipe Threshold 20...80 step 5, and the four
// toggles of UserConfig.popupActionBar, popupDisableTransparency,
// popupFullWidth and popupSwipeToDismiss. Defaults are 320, 250, 1.0, 40 and
// false, matching UserConfig.init. The four sliders clamp to their range and
// round to their step with std::round, ties away from zero; the threshold also
// keeps its value while Swipe to Dismiss is off. All eight stay in
// the active profile's reader_settings.json under the Swift keys. Full-width
// reaches the reader popup only; the video popup passes false in Swift.

#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>

#include "services/settings/settings_status.h"

namespace niratan::viewmodels {

class SettingsViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int section READ section WRITE setSection NOTIFY sectionChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY readerChanged)
    Q_PROPERTY(QString uiTheme READ uiTheme WRITE setUiTheme NOTIFY readerChanged)
    Q_PROPERTY(bool sepiaInvertInDark READ sepiaInvertInDark WRITE setSepiaInvertInDark NOTIFY readerChanged)
    Q_PROPERTY(bool systemLightSepia READ systemLightSepia WRITE setSystemLightSepia NOTIFY readerChanged)
    Q_PROPERTY(bool verticalWriting READ verticalWriting WRITE setVerticalWriting NOTIFY readerChanged)
    Q_PROPERTY(QString selectedFont READ selectedFont WRITE setSelectedFont NOTIFY readerChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY readerChanged)
    Q_PROPERTY(QString furiganaMode READ furiganaMode WRITE setFuriganaMode NOTIFY readerChanged)
    Q_PROPERTY(bool continuousMode READ continuousMode WRITE setContinuousMode NOTIFY readerChanged)
    Q_PROPERTY(bool paragraphMode READ paragraphMode WRITE setParagraphMode NOTIFY readerChanged)
    Q_PROPERTY(bool textAnimation READ textAnimation WRITE setTextAnimation NOTIFY readerChanged)
    Q_PROPERTY(int textSpeed READ textSpeed WRITE setTextSpeed NOTIFY readerChanged)
    Q_PROPERTY(bool twoColumnHorizontalPages READ twoColumnHorizontalPages WRITE setTwoColumnHorizontalPages NOTIFY readerChanged)
    Q_PROPERTY(int horizontalPadding READ horizontalPadding WRITE setHorizontalPadding NOTIFY readerChanged)
    Q_PROPERTY(int verticalPadding READ verticalPadding WRITE setVerticalPadding NOTIFY readerChanged)
    Q_PROPERTY(bool avoidPageBreak READ avoidPageBreak WRITE setAvoidPageBreak NOTIFY readerChanged)
    Q_PROPERTY(bool justifyText READ justifyText WRITE setJustifyText NOTIFY readerChanged)
    Q_PROPERTY(bool blurImages READ blurImages WRITE setBlurImages NOTIFY readerChanged)
    Q_PROPERTY(bool layoutAdvanced READ layoutAdvanced WRITE setLayoutAdvanced NOTIFY readerChanged)
    Q_PROPERTY(double lineHeight READ lineHeight WRITE setLineHeight NOTIFY readerChanged)
    Q_PROPERTY(double characterSpacing READ characterSpacing WRITE setCharacterSpacing NOTIFY readerChanged)
    Q_PROPERTY(double paragraphSpacing READ paragraphSpacing WRITE setParagraphSpacing NOTIFY readerChanged)
    Q_PROPERTY(bool showTitle READ showTitle WRITE setShowTitle NOTIFY readerChanged)
    Q_PROPERTY(bool showProgress READ showProgress WRITE setShowProgress NOTIFY readerChanged)
    Q_PROPERTY(bool showChapterProgress READ showChapterProgress WRITE setShowChapterProgress NOTIFY readerChanged)
    Q_PROPERTY(QString progressCount READ progressCount WRITE setProgressCount NOTIFY readerChanged)
    Q_PROPERTY(bool showPercentage READ showPercentage WRITE setShowPercentage NOTIFY readerChanged)
    Q_PROPERTY(bool showProgressTop READ showProgressTop WRITE setShowProgressTop NOTIFY readerChanged)
    Q_PROPERTY(int popupWidth READ popupWidth WRITE setPopupWidth NOTIFY readerChanged)
    Q_PROPERTY(int popupHeight READ popupHeight WRITE setPopupHeight NOTIFY readerChanged)
    Q_PROPERTY(double popupScale READ popupScale WRITE setPopupScale NOTIFY readerChanged)
    Q_PROPERTY(bool popupActionBar READ popupActionBar WRITE setPopupActionBar NOTIFY readerChanged)
    Q_PROPERTY(bool popupDisableTransparency READ popupDisableTransparency WRITE
                   setPopupDisableTransparency NOTIFY readerChanged)
    Q_PROPERTY(bool popupFullWidth READ popupFullWidth WRITE setPopupFullWidth NOTIFY readerChanged)
    Q_PROPERTY(bool popupSwipeToDismiss READ popupSwipeToDismiss WRITE setPopupSwipeToDismiss NOTIFY
                   readerChanged)
    Q_PROPERTY(int popupSwipeThreshold READ popupSwipeThreshold WRITE setPopupSwipeThreshold NOTIFY
                   readerChanged)

    Q_PROPERTY(bool dictionaryTabDefault READ dictionaryTabDefault WRITE setDictionaryTabDefault NOTIFY dictionaryChanged)
    Q_PROPERTY(bool scanNonJapaneseText READ scanNonJapaneseText WRITE setScanNonJapaneseText NOTIFY dictionaryChanged)
    Q_PROPERTY(int maxResults READ maxResults WRITE setMaxResults NOTIFY dictionaryChanged)
    Q_PROPERTY(int scanLength READ scanLength WRITE setScanLength NOTIFY dictionaryChanged)
    Q_PROPERTY(QString collapseMode READ collapseMode WRITE setCollapseMode NOTIFY dictionaryChanged)
    Q_PROPERTY(bool twoColumnLayout READ twoColumnLayout WRITE setTwoColumnLayout NOTIFY dictionaryChanged)
    Q_PROPERTY(bool compactGlossaries READ compactGlossaries WRITE setCompactGlossaries NOTIFY dictionaryChanged)
    Q_PROPERTY(QString customCSS READ customCSS WRITE setCustomCSS NOTIFY dictionaryChanged)

    Q_PROPERTY(bool enableStatistics READ enableStatistics WRITE setEnableStatistics NOTIFY statisticsChanged)
    Q_PROPERTY(QString statisticsAutostartMode READ statisticsAutostartMode WRITE setStatisticsAutostartMode NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsResetTime READ statisticsResetTime WRITE setStatisticsResetTime NOTIFY statisticsChanged)

   public:
    explicit SettingsViewModel(QObject* parent = nullptr);
    ~SettingsViewModel() override;

    int section() const;
    void setSection(int value);
    QString error() const;

    QString theme() const;
    void setTheme(const QString& value);
    QString uiTheme() const;
    void setUiTheme(const QString& value);
    bool sepiaInvertInDark() const;
    void setSepiaInvertInDark(bool value);
    bool systemLightSepia() const;
    void setSystemLightSepia(bool value);
    bool verticalWriting() const;
    void setVerticalWriting(bool value);
    QString selectedFont() const;
    void setSelectedFont(const QString& value);
    int fontSize() const;
    void setFontSize(int value);
    QString furiganaMode() const;
    void setFuriganaMode(const QString& value);
    bool continuousMode() const;
    void setContinuousMode(bool value);
    bool paragraphMode() const;
    void setParagraphMode(bool value);
    bool textAnimation() const;
    void setTextAnimation(bool value);
    int textSpeed() const;
    void setTextSpeed(int value);
    bool twoColumnHorizontalPages() const;
    void setTwoColumnHorizontalPages(bool value);
    int horizontalPadding() const;
    void setHorizontalPadding(int value);
    int verticalPadding() const;
    void setVerticalPadding(int value);
    bool avoidPageBreak() const;
    void setAvoidPageBreak(bool value);
    bool justifyText() const;
    void setJustifyText(bool value);
    bool blurImages() const;
    void setBlurImages(bool value);
    bool layoutAdvanced() const;
    void setLayoutAdvanced(bool value);
    double lineHeight() const;
    void setLineHeight(double value);
    double characterSpacing() const;
    void setCharacterSpacing(double value);
    double paragraphSpacing() const;
    void setParagraphSpacing(double value);
    bool showTitle() const;
    void setShowTitle(bool value);
    bool showProgress() const;
    void setShowProgress(bool value);
    bool showChapterProgress() const;
    void setShowChapterProgress(bool value);
    QString progressCount() const;
    void setProgressCount(const QString& value);
    bool showPercentage() const;
    void setShowPercentage(bool value);
    bool showProgressTop() const;
    void setShowProgressTop(bool value);

    int popupWidth() const;
    void setPopupWidth(int value);
    int popupHeight() const;
    void setPopupHeight(int value);
    double popupScale() const;
    void setPopupScale(double value);
    bool popupActionBar() const;
    void setPopupActionBar(bool value);
    bool popupDisableTransparency() const;
    void setPopupDisableTransparency(bool value);
    bool popupFullWidth() const;
    void setPopupFullWidth(bool value);
    bool popupSwipeToDismiss() const;
    void setPopupSwipeToDismiss(bool value);
    int popupSwipeThreshold() const;
    void setPopupSwipeThreshold(int value);

    bool dictionaryTabDefault() const;
    void setDictionaryTabDefault(bool value);
    bool scanNonJapaneseText() const;
    void setScanNonJapaneseText(bool value);
    int maxResults() const;
    void setMaxResults(int value);
    int scanLength() const;
    void setScanLength(int value);
    QString collapseMode() const;
    void setCollapseMode(const QString& value);
    bool twoColumnLayout() const;
    void setTwoColumnLayout(bool value);
    bool compactGlossaries() const;
    void setCompactGlossaries(bool value);
    QString customCSS() const;
    void setCustomCSS(const QString& value);

    bool enableStatistics() const;
    void setEnableStatistics(bool value);
    QString statisticsAutostartMode() const;
    void setStatisticsAutostartMode(const QString& value);
    int statisticsResetTime() const;
    void setStatisticsResetTime(int value);

   signals:
    void sectionChanged();
    void errorChanged();
    void readerChanged();
    void dictionaryChanged();
    void statisticsChanged();

   private:
    struct State;
    std::unique_ptr<State> state_;
    int section_ = 0;

    template <typename Field, typename Value>
    void update_reader(Field& field, Value value);
    template <typename Field, typename Value>
    void update_dictionary(Field& field, Value value);
    void save_reader();
    void save_dictionary();
    void report_statistics_save(settings::StoreError error);
    void set_error(QString& slot, QString error);
};

}  // namespace niratan::viewmodels

// SPDX-License-Identifier: GPL-3.0-or-later
//
// State and commands for the reader page, following
// NativeMac/NativeReaderView.swift (the reading model, page turning, chapter
// changes, bookmark persistence and the web view commands) and
// Features/Reader/ReaderStatisticsPersistencePolicy.swift.
//
// The model owns one opened book and one chapter at a time. It never touches
// the web view directly: the page asks for a script, runs it, and reports the
// result back, so the model stays free of QtWebEngine. The chapter itself is
// served through the reader:// scheme by document_for()/resource().
//
// Positions are chapter index plus progress inside the chapter, exactly as the
// bookmark stores them. The overall progress is the bookmark's character
// offset over the book's character total; a chapter without a recorded count
// maps to its start.
//
// Page turning follows the web view's protocol: paginate("forward") returns
// "limit" at the end of a chapter, which advances to the next chapter; any
// other result leaves the chapter in place. Continuous reading reports a
// progress number instead. Chapter changes save the bookmark of the chapter
// that is left, and the bookmark is also saved when the reader closes.
// NativeReaderView.navigate and textAnimationScript reveal text only after a
// successful forward turn in paginated paragraph mode with animation enabled.
//
// Appearance settings from the active profile's reader_settings.json apply
// live. The chapter reloads at its recorded progress because layout can
// change which scripts are injected. This restore neither saves the bookmark
// nor credits statistics. Dictionary settings still require reopening.
// TOC fragment jumps use the document's anchor; a missing anchor leaves the
// reader at the start of that spine item. Page numbers are local to that item
// and unavailable in continuous mode, as in NativeReaderView.showsPageCount.
//
// Reading statistics
// ------------------
// Following the statistics section of NativeReaderView.swift (configure,
// loadBook's On autostart, updateStats, handleManualNavigation, flushStats,
// saveStats, prepareForReaderLifecycleClose) and
// Features/Reader/ReaderStatisticsPersistencePolicy.swift, the model owns one
// reader::BookStatisticsStore and one reader::ReadingTracker for the open book.
// They are created in openBook after the session is open and destroyed in
// closeBook. openBook reads the statistics preferences (enabled, autostart
// mode, reset minute) through settings::GlobalSettings; a missing or damaged
// settings.json gives the defaults. The reader never writes those keys.
// The tracker is configured with the local time zone, and On mode starts
// tracking at the open instant. Page Turn mode starts on the first page turn.
//
// Time and characters are credited by updateChapterProgress(): it maps the
// reported chapter progress to BookSession::character_offset and passes the
// global offset to the tracker. A chapter change credits the jump to the new
// chapter's boundary before saving. The tracker gets no position from loading
// or restoring a chapter, so the restore target and the chapter's initial
// position never count as read; a position that equals the current progress is
// ignored before the tracker is called. closeBook stops and saves the live
// session, and leaving the chapter saves it so the stored bookmark the
// persistence policy compares against is the position the session ended at.
// Page turns do not save. setReaderActive() feeds the tracker's context
// predicate (isStatisticsContextActive): a hidden page pauses time, while the
// statistics sheet keeps the reader page visible.
//
// The totals below are cached when the tracker changes, so
// a QML binding does no work until statisticsChanged() fires.
//
// statisticsInspector is the reader sheet's presentation, rebuilt from the
// tracker, the current position and the profile's content language whenever
// the totals, the position or the open book change. models/statistics/
// reader_statistics.h owns the number, duration and "—" decisions; this class
// only supplies the values.
//
// bookFolder is the open book's folder name under Books/, exposed so a model
// that reads sidecar files (Sasayaki) can resolve them without matching the
// display title. It is empty when no book is open.

#pragma once

#include <QJSEngine>
#include <QObject>
#include <QColor>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QRectF>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>
#include <string>

#include "services/reader/book_session.h"

namespace niratan::viewmodels {

class ReaderViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString title READ title NOTIFY bookChanged)
    Q_PROPERTY(QString chapterTitle READ chapter_title NOTIFY chapterChanged)
    Q_PROPERTY(int chapterIndex READ chapter_index NOTIFY chapterChanged)
    Q_PROPERTY(int chapterCount READ chapter_count NOTIFY bookChanged)
    Q_PROPERTY(QString bookFolder READ book_folder NOTIFY bookChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(double chapterProgress READ chapter_progress NOTIFY progressChanged)
    Q_PROPERTY(double logicalChapterProgress READ logical_chapter_progress NOTIFY progressChanged)
    Q_PROPERTY(int characterOffset READ character_offset NOTIFY progressChanged)
    Q_PROPERTY(int chapterCharactersRemaining READ chapter_characters_remaining NOTIFY progressChanged)
    Q_PROPERTY(int pageCount READ page_count NOTIFY paginationChanged)
    Q_PROPERTY(int pageIndex READ page_index NOTIFY paginationChanged)
    Q_PROPERTY(int chapterPageCount READ page_count NOTIFY paginationChanged)
    Q_PROPERTY(bool showsPageCount READ shows_page_count NOTIFY settingsChanged)
    Q_PROPERTY(QString progressCountMode READ progress_count_mode NOTIFY settingsChanged)
    Q_PROPERTY(bool showTitle READ show_title NOTIFY settingsChanged)
    Q_PROPERTY(bool showProgress READ show_progress NOTIFY settingsChanged)
    Q_PROPERTY(bool showPercentage READ show_percentage NOTIFY settingsChanged)
    Q_PROPERTY(bool showProgressTop READ show_progress_top NOTIFY settingsChanged)
    Q_PROPERTY(bool showChapterProgress READ show_chapter_progress NOTIFY settingsChanged)
    Q_PROPERTY(bool showReadingSpeed READ show_reading_speed NOTIFY settingsChanged)
    Q_PROPERTY(bool showReadingTime READ show_reading_time NOTIFY settingsChanged)
    Q_PROPERTY(bool showSasayakiToggle READ show_sasayaki_toggle NOTIFY settingsChanged)
    Q_PROPERTY(bool showStatisticsToggle READ show_statistics_toggle NOTIFY settingsChanged)
    Q_PROPERTY(QString uiTheme READ ui_theme NOTIFY settingsChanged)
    Q_PROPERTY(bool englishContent READ english_content NOTIFY settingsChanged)
    Q_PROPERTY(QString chapterUrl READ chapter_url NOTIFY chapterUrlChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY bookChanged)
    Q_PROPERTY(QVariantList searchResults READ search_results NOTIFY searchChanged)
    Q_PROPERTY(int searchState READ search_state NOTIFY searchChanged)
    Q_PROPERTY(QVariantList highlights READ highlights NOTIFY highlightsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool hasBook READ has_book NOTIFY bookChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY paletteChanged)
    Q_PROPERTY(QColor pageBackground READ page_background NOTIFY paletteChanged)
    Q_PROPERTY(QColor infoColor READ info_color NOTIFY paletteChanged)
    Q_PROPERTY(bool usesCustomInfoColor READ uses_custom_info_color NOTIFY paletteChanged)
    Q_PROPERTY(bool restoring READ restoring NOTIFY restoreStateChanged)
    Q_PROPERTY(bool verticalWriting READ vertical_writing WRITE setVerticalWriting NOTIFY
                   settingsChanged)
    Q_PROPERTY(int fontSize READ font_size WRITE setFontSize NOTIFY settingsChanged)
    Q_PROPERTY(int horizontalPadding READ horizontal_padding WRITE setHorizontalPadding NOTIFY
                   settingsChanged)
    Q_PROPERTY(int verticalPadding READ vertical_padding WRITE setVerticalPadding NOTIFY
                   settingsChanged)
    Q_PROPERTY(int statisticsTodayCharacters READ statistics_today_characters NOTIFY
                   statisticsChanged)
    Q_PROPERTY(int statisticsTodaySeconds READ statistics_today_seconds NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsSessionSpeed READ statistics_session_speed NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsSessionMinutes READ statistics_session_minutes NOTIFY statisticsChanged)
    Q_PROPERTY(bool statisticsEnabled READ statistics_enabled NOTIFY statisticsChanged)
    Q_PROPERTY(bool tracking READ tracking NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantMap statisticsInspector READ statistics_inspector NOTIFY
                   statisticsInspectorChanged)

   public:
    ~ReaderViewModel() override;

    // The process-wide reader. The page and the reader:// scheme handler must
    // share one open book, so QML reaches the same instance main() created.
    static ReaderViewModel* instance();
    static ReaderViewModel* create(QQmlEngine* engine, QJSEngine* js_engine);

    QString title() const;
    QString chapter_title() const;
    int chapter_index() const;
    int chapter_count() const;
    QString book_folder() const;
    double progress() const;
    double chapter_progress() const;
    double logical_chapter_progress() const;
    int character_offset() const;
    int chapter_characters_remaining() const;
    int page_count() const;
    int page_index() const;
    bool shows_page_count() const;
    QString progress_count_mode() const;
    bool show_title() const;
    bool show_progress() const;
    bool show_percentage() const;
    bool show_progress_top() const;
    bool show_chapter_progress() const;
    bool show_reading_speed() const;
    bool show_reading_time() const;
    bool show_sasayaki_toggle() const;
    bool show_statistics_toggle() const;
    QString ui_theme() const;
    bool english_content() const;
    QString chapter_url() const;
    QVariantList chapters() const;
    QVariantList search_results() const;
    int search_state() const;
    Q_INVOKABLE bool hasSearchableText(const QString& query) const;
    Q_INVOKABLE void searchBook(const QString& query);
    Q_INVOKABLE void clearBookSearch();
    Q_INVOKABLE void jumpToCharacter(int character);
    QVariantList highlights() const;
    Q_INVOKABLE QString highlightScript(const QString& color) const;
    Q_INVOKABLE void addHighlight(const QString& script_url, const QString& color,
                                 const QVariant& creation);
    Q_INVOKABLE void removeHighlight(const QString& id);
    QString error() const;
    bool has_book() const;

    // The system appearance is the fallback for the independent page palette.
    bool dark() const;
    Q_INVOKABLE void setDark(bool dark);
    QColor page_background() const;
    QColor info_color() const;
    bool uses_custom_info_color() const;
    bool restoring() const;
    Q_INVOKABLE void revealAfterLoadFailure(const QString& url);
    Q_INVOKABLE void setReaderColors(const QString& background, const QString& text);

    bool vertical_writing() const;
    void setVerticalWriting(bool vertical);
    int font_size() const;
    void setFontSize(int size);
    int horizontal_padding() const;
    void setHorizontalPadding(int padding);
    int vertical_padding() const;
    void setVerticalPadding(int padding);

    // Today's characters and whole seconds, and the session's speed and minutes.
    int statistics_today_characters() const;
    int statistics_today_seconds() const;
    int statistics_session_speed() const;
    int statistics_session_minutes() const;
    bool statistics_enabled() const;
    bool tracking() const;
    // The inspector's derived text, following Features/Reader/StatisticsView.swift
    // (ReaderStatisticsContentView). Empty until a book is open with a tracker.
    QVariantMap statistics_inspector() const;
    Q_INVOKABLE void toggleTracking();
    Q_INVOKABLE void tickTracking();

    // Imports the EPUB into Books and opens it. The title names the folder.
    Q_INVOKABLE bool importAndOpen(const QUrl& file);
    // Opens an already imported book folder.
    Q_INVOKABLE bool openBook(const QString& folder);
    Q_INVOKABLE void closeBook();

    // The web view size; the injected page size follows it.
    Q_INVOKABLE void setPageSize(int width, int height);

    Q_INVOKABLE void goToChapter(int index);
    Q_INVOKABLE void goToContentsEntry(int index, const QString& fragment);
    Q_INVOKABLE void goToProgress(double progress);

    // A JavaScript expression for the page to run, or empty when no book is
    // open.
    Q_INVOKABLE QString pageTurnScript(bool forward) const;
    // The value the page turn script returned.
    Q_INVOKABLE void handlePageTurnResult(bool forward, const QVariant& result);

    // Called by the page after a scroll or pagination with
    // window.hoshiReader.calculateProgress().
    Q_INVOKABLE void updateChapterProgress(double progress);
    Q_INVOKABLE void saveBookmark();

    // The reader page's visibility, matching NativeReaderView's
    // isReaderWindowActive. A hidden page pauses time; the statistics sheet
    // does not hide the page.
    Q_INVOKABLE void setReaderActive(bool active);

    // QWebChannel entry point for window.hoshiSend.
    Q_INVOKABLE void message(const QString& name, const QString& payload);

    // Scheme handler access. document_for returns the injected chapter HTML
    // when path is one of the spine entries, else an empty string.
    std::string document_for(const std::string& path);
    std::optional<niratan::reader::SessionResource> resource(const std::string& path);
    bool is_open() const;

   signals:
    void bookChanged();
    void chapterChanged();
    void progressChanged();
    void chapterUrlChanged();
    void restoreStateChanged();
    void errorChanged();
    void settingsChanged();
    void paletteChanged();
    void statisticsChanged();
    void statisticsInspectorChanged();
    void paginationChanged();
    void pageChanged(int index);
    void documentRestored(bool revealCue);
    void lookupRequested(const QString& text, const QRectF& anchor);
    void readerTappedOutside();
    void searchChanged();
    void highlightsChanged();
    // The page should run navigation for this direction ("forward"/"backward").
    void pageTurnRequested(const QString& direction);

   private:
    // A default-constructible singleton would let QML bypass create().
    ReaderViewModel();

    struct State;
    std::unique_ptr<State> state_;

    void load_chapter(int index, double progress, const QString& fragment = {});
    void reload_document();
    void reload_settings(bool reflow);
    void update_palette();
    void set_pagination(int index, int count);
    void persist_bookmark();
    // Credits the elapsed time without characters and writes the live
    // session. Called before a chapter change; never on a page turn.
    void save_statistics();
    // Also notifies on book changes and timer ticks, which may change the
    // inspector even if the cached values did not change.
    void refresh_statistics(bool notify = false);
    std::string shell_document(const std::string& path);
};

}  // namespace niratan::viewmodels

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
// A chapter load the reader starts itself (a page turn into the next chapter,
// a contents entry, a character or progress jump) emits chapterTransitionStarted
// before the document reloads, so the Sasayaki player can pause for the
// transition. A Sasayaki cue load uses goToChapterAt, which does not emit it:
// the player already prepared the transition. goToChapterAt flushes statistics
// at the old position, writes the destination bookmark before the restore and
// resets the statistics baseline at the destination, following
// NativeReaderView.loadChapterForSasayaki and establishProgrammaticDestination.
// chapterCharacterCount is the chapter's readable characters, which turns a
// cue's character offset into reader progress.
//
// Appearance settings from the active profile's reader_settings.json apply
// live. The chapter reloads at its recorded progress because layout can
// change which scripts are injected. This restore neither saves the bookmark
// nor credits statistics. Dictionary settings still require reopening.
// TOC fragment jumps use the document's anchor; a missing anchor leaves the
// reader at the start of that spine item. Following ReaderPageIndex.swift's
// progress, page numbers accumulate measured spine counts and are unavailable
// in continuous mode. Unmeasured spine items contribute zero pages.
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
// tracking at the open instant. Page Turn mode starts on a page turn or a
// continuous scroll that changes reading progress after restore.
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
//
// Reader mining follows selection.js (selectTextAtPoint and getSentence),
// NativeReaderView.swift (NativeReaderWebView.Coordinator's textSelected,
// NativeReaderModel.title and coverURL, and popupLayer), and PopupView.swift
// (mineEntry). The JavaScript sentence is required and relayed verbatim, not
// re-extracted from the lookup text or the context sheet's sentence list.
// Linux retains its guards against blank text and nonpositive rect sizes;
// accepted lookup text is not trimmed. Mining metadata uses the EPUB title
// (falling back to the library title if missing or empty) and the opened
// book's cover reference. The reader's existing gallery server also serves the
// cover on Swift's fixed port 18765. If that port is unavailable, the gallery
// uses an ephemeral port and coverUrl is empty, but the local attachment path
// remains. No cover means both cover values are empty. Nested popup selections
// have their own context. The cover bytes are installed on the server when
// coverUrl is read, where Swift installs them while building the card; the
// URL is the same either way. Rect fields must be JSON numbers: a boolean,
// which Swift's cgFloatValue reads as 0 or 1, is rejected, as no reader
// script sends one.
//
// Jump history
// ------------
// Following NativeReaderView.swift's backHistory and forwardHistory, each
// opened book starts with both stacks empty. A valid chapter, character or
// matching internal-link jump records the current chapter and progress, then
// clears the forward stack; an invalid chapter or an unmatched link changes
// neither. backTarget and forwardTarget are the top entry's global raw
// character offset, or -1 when there is no resolvable entry; zero is a valid
// target. Their Text properties apply the active profile's display count to
// that offset, without grouping. Back and forward pop in LIFO order, push the
// current exact position onto the other stack and restore the popped entry
// without a fragment at progress clamped to 0…1. A restore uses the
// programmatic destination path, so reading statistics get no credit for the
// jump. Manual page turns and continuous scrolling clear only the forward
// stack; programmatic restores and synced bookmarks do not. A matching
// same-spine link asks the page to jump in the open document through
// internalLinkJumpRequested; the page reports the resulting progress through
// syncProgressAfterProgrammaticJump, which stores it with no history change.

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
    Q_PROPERTY(QString documentTitle READ document_title NOTIFY bookChanged)
    Q_PROPERTY(QString coverPath READ cover_path NOTIFY bookChanged)
    Q_PROPERTY(QString coverUrl READ cover_url NOTIFY bookChanged)
    Q_PROPERTY(QString chapterTitle READ chapter_title NOTIFY chapterChanged)
    Q_PROPERTY(int chapterIndex READ chapter_index NOTIFY chapterChanged)
    Q_PROPERTY(int chapterCount READ chapter_count NOTIFY bookChanged)
    Q_PROPERTY(QString bookFolder READ book_folder NOTIFY bookChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(double chapterProgress READ chapter_progress NOTIFY progressChanged)
    Q_PROPERTY(double logicalChapterProgress READ logical_chapter_progress NOTIFY progressChanged)
    Q_PROPERTY(int lookupOffset READ lookup_offset NOTIFY lookupOffsetChanged)
    Q_PROPERTY(int characterOffset READ character_offset NOTIFY progressChanged)
    Q_PROPERTY(
        int chapterCharactersRemaining READ chapter_characters_remaining NOTIFY progressChanged)
    Q_PROPERTY(int pageCount READ page_count NOTIFY paginationChanged)
    Q_PROPERTY(int pageIndex READ page_index NOTIFY paginationChanged)
    Q_PROPERTY(int chapterPageCount READ chapter_page_count NOTIFY paginationChanged)
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
    Q_PROPERTY(bool popupActionBar READ popup_action_bar NOTIFY settingsChanged)
    Q_PROPERTY(bool popupDisableTransparency READ popup_disable_transparency NOTIFY settingsChanged)
    Q_PROPERTY(bool popupFullWidth READ popup_full_width NOTIFY settingsChanged)
    Q_PROPERTY(int popupHeight READ popup_height NOTIFY settingsChanged)
    Q_PROPERTY(double popupScale READ popup_scale NOTIFY settingsChanged)
    Q_PROPERTY(bool popupSwipeToDismiss READ popup_swipe_to_dismiss NOTIFY settingsChanged)
    Q_PROPERTY(int popupSwipeThreshold READ popup_swipe_threshold NOTIFY settingsChanged)
    Q_PROPERTY(int popupWidth READ popup_width NOTIFY settingsChanged)
    // The interface appearance, Core/UserConfig.swift `preferredColorScheme`:
    // Light and Dark force theirs, Custom takes `uiTheme`, and System and
    // Sepia (and any unknown value) yield "System", which follows the desktop.
    Q_PROPERTY(QString interfaceTheme READ interface_theme NOTIFY settingsChanged)
    Q_PROPERTY(bool englishContent READ english_content NOTIFY settingsChanged)
    Q_PROPERTY(QString chapterUrl READ chapter_url NOTIFY chapterUrlChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY bookChanged)
    Q_PROPERTY(QVariantList galleryImages READ gallery_images NOTIFY galleryChanged)
    Q_PROPERTY(bool galleryLoading READ gallery_loading NOTIFY galleryChanged)
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
    Q_PROPERTY(
        bool verticalWriting READ vertical_writing WRITE setVerticalWriting NOTIFY settingsChanged)
    Q_PROPERTY(int fontSize READ font_size WRITE setFontSize NOTIFY settingsChanged)
    Q_PROPERTY(int horizontalPadding READ horizontal_padding WRITE setHorizontalPadding NOTIFY
                   settingsChanged)
    Q_PROPERTY(
        int verticalPadding READ vertical_padding WRITE setVerticalPadding NOTIFY settingsChanged)
    Q_PROPERTY(
        int statisticsTodayCharacters READ statistics_today_characters NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsTodaySeconds READ statistics_today_seconds NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsSessionSpeed READ statistics_session_speed NOTIFY statisticsChanged)
    Q_PROPERTY(
        int statisticsSessionMinutes READ statistics_session_minutes NOTIFY statisticsChanged)
    Q_PROPERTY(bool statisticsEnabled READ statistics_enabled NOTIFY statisticsChanged)
    Q_PROPERTY(bool tracking READ tracking NOTIFY statisticsChanged)
    Q_PROPERTY(
        QVariantMap statisticsInspector READ statistics_inspector NOTIFY statisticsInspectorChanged)
    // The reader overlays' text, following NativeReaderView.progressString and
    // statisticsString, and ReaderGoToView.progressText for the current
    // position. reader_presentation.h owns the wording; these supply the
    // book's raw counts, the table-of-contents chapter range, the measured
    // pages and the session totals. Empty without a book.
    Q_PROPERTY(QString progressText READ progress_text NOTIFY infoTextChanged)
    Q_PROPERTY(QString statisticsText READ statistics_text NOTIFY infoTextChanged)
    Q_PROPERTY(QString positionText READ position_text NOTIFY infoTextChanged)
    // The top history entry's raw global character offset, or -1 when none is
    // available, and its display count under the active profile.
    Q_PROPERTY(int backTarget READ back_target NOTIFY historyChanged)
    Q_PROPERTY(int forwardTarget READ forward_target NOTIFY historyChanged)
    Q_PROPERTY(QString backTargetText READ back_target_text NOTIFY historyChanged)
    Q_PROPERTY(QString forwardTargetText READ forward_target_text NOTIFY historyChanged)

   public:
    // Port zero lets isolated readers use an OS-assigned listener.
    explicit ReaderViewModel(quint16 media_port);
    ~ReaderViewModel() override;

    // The process-wide reader. The page and the reader:// scheme handler must
    // share one open book, so QML reaches the same instance main() created.
    // Only the first call selects the port; normal callers use the Swift default.
    static ReaderViewModel* instance(std::optional<quint16> first_media_port = std::nullopt);
    static ReaderViewModel* create(QQmlEngine* engine, QJSEngine* js_engine);

    QString title() const;
    QString document_title() const;
    QString cover_path() const;
    QString cover_url() const;
    QString chapter_title() const;
    int chapter_index() const;
    int chapter_count() const;
    QString book_folder() const;
    double progress() const;
    double chapter_progress() const;
    double logical_chapter_progress() const;
    // The last selection's normalizedOffset, or -1 when it had none.
    int lookup_offset() const;
    int character_offset() const;
    int chapter_characters_remaining() const;
    int page_count() const;
    int page_index() const;
    int chapter_page_count() const;
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
    bool popup_action_bar() const;
    bool popup_disable_transparency() const;
    bool popup_full_width() const;
    int popup_height() const;
    double popup_scale() const;
    bool popup_swipe_to_dismiss() const;
    int popup_swipe_threshold() const;
    int popup_width() const;
    QString interface_theme() const;
    bool english_content() const;
    QString chapter_url() const;
    QVariantList chapters() const;
    QVariantList gallery_images() const;
    bool gallery_loading() const;
    Q_INVOKABLE void loadGallery();
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
    // Rebuilds the reader document so changed cue colours apply without
    // reopening the book (NativeReaderView.updateSasayakiColors). Defined here
    // because reader_view_model.cpp is frozen at its size baseline.
    Q_INVOKABLE void refreshSasayakiColors() { reload_document(); }

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

    QString progress_text() const;
    QString statistics_text() const;
    QString position_text() const;
    int back_target() const;
    int forward_target() const;
    QString back_target_text() const;
    QString forward_target_text() const;
    // ReaderGoToView.progressText for a search result's raw character.
    Q_INVOKABLE QString positionTextAt(int rawCharacter) const;
    // The Jump to alert's Go action: a valid display count jumps to its raw
    // character and returns true; anything else changes nothing and returns
    // false, which the page reports as Invalid input.
    Q_INVOKABLE bool jumpToDisplayCount(const QString& input);

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
    // NativeReaderView.navigateBackwards and navigateForwards.
    Q_INVOKABLE void navigateBackwards();
    Q_INVOKABLE void navigateForwards();
    // NativeReaderView.jumpToLink: true for a chapter link, which this model
    // handles internally, and false for any other navigation.
    Q_INVOKABLE bool jumpToLink(const QUrl& url);
    // The progress the open document reached after a same-spine link jump;
    // NativeReaderView.syncProgressAfterProgrammaticJump.
    Q_INVOKABLE void syncProgressAfterProgrammaticJump(double progress);
    // The reader half of NativeReaderView.loadChapterForSasayaki: statistics
    // are flushed at the old position, then the destination chapter and
    // progress are stored with their bookmark before the document reloads.
    Q_INVOKABLE void goToChapterAt(int index, double progress);
    // NativeReaderView.syncBookmarkToSasayakiCue's reader side: the same
    // chapter stores the cue progress and resets the statistics baseline
    // without reloading; another chapter loads at that progress.
    Q_INVOKABLE void syncBookmarkToProgress(int index, double progress);
    // The chapter's readable characters; 0 for an unknown index or chapter.
    int chapterCharacterCount(int index) const;

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
    // The reader is about to reload a chapter it started itself, so the
    // Sasayaki player pauses first (NativeReaderView's prepareTransition calls).
    void chapterTransitionStarted();
    void lookupRequested(const QString& text, const QString& sentence, const QRectF& anchor);
    // The selection's normalizedOffset changed, or became -1 without one.
    void lookupOffsetChanged();
    void readerTappedOutside();
    void searchChanged();
    void highlightsChanged();
    void galleryChanged();
    // The back or forward target, its text or the displayed profile changed.
    void historyChanged();
    // The page should jump within the open chapter to this decoded fragment.
    // A null fragment restores progress zero; a present fragment, including an
    // empty one, requests a fragment jump.
    void internalLinkJumpRequested(const QString& fragment);
    // progressText, statisticsText or positionText may have changed.
    void infoTextChanged();
    // The page should run navigation for this direction ("forward"/"backward").
    void pageTurnRequested(const QString& direction);

   private:
    struct State;
    std::unique_ptr<State> state_;

    void handle_text_selected(const QJsonObject& data);
    void load_chapter(int index, double progress, const QString& fragment = {});
    void clear_gallery();
    void scan_gallery_chapter(int index, std::size_t revision, const QString& route);
    void reload_document(bool reflow = false);
    void reload_settings(bool reflow);
    void update_palette();
    void set_pagination(int index, int count);
    void persist_bookmark();
    // Credits the elapsed time without characters and writes the live
    // session. Called before a chapter change; never on a page turn.
    void save_statistics();
    void start_tracking_on_navigation();
    // Also notifies on book changes and timer ticks, which may change the
    // inspector even if the cached values did not change.
    void refresh_statistics(bool notify = false);
    std::string shell_document(const std::string& path);
};

}  // namespace niratan::viewmodels

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
//
// Reader settings come from the active profile's reader_settings.json; the
// page edits them through the setters, which rebuild the injected document.

#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include <QVariantList>
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
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(double chapterProgress READ chapter_progress NOTIFY progressChanged)
    Q_PROPERTY(QString chapterUrl READ chapter_url NOTIFY chapterUrlChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY bookChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool hasBook READ has_book NOTIFY bookChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY paletteChanged)
    Q_PROPERTY(bool verticalWriting READ vertical_writing WRITE setVerticalWriting NOTIFY
                   settingsChanged)
    Q_PROPERTY(int fontSize READ font_size WRITE setFontSize NOTIFY settingsChanged)
    Q_PROPERTY(int horizontalPadding READ horizontal_padding WRITE setHorizontalPadding NOTIFY
                   settingsChanged)
    Q_PROPERTY(int verticalPadding READ vertical_padding WRITE setVerticalPadding NOTIFY
                   settingsChanged)

   public:
    explicit ReaderViewModel(QObject* parent = nullptr);
    ~ReaderViewModel() override;

    // The process-wide reader. The page and the reader:// scheme handler must
    // share one open book, so QML reaches the same instance main() created.
    static ReaderViewModel* instance();
    static ReaderViewModel* create(QQmlEngine* engine, QJSEngine* js_engine);

    QString title() const;
    QString chapter_title() const;
    int chapter_index() const;
    int chapter_count() const;
    double progress() const;
    double chapter_progress() const;
    QString chapter_url() const;
    QVariantList chapters() const;
    QString error() const;
    bool has_book() const;

    // Pushed from QML's Theme singleton: the window appearance and the two
    // reader colours the stylesheet uses.
    bool dark() const;
    void setDark(bool dark);
    Q_INVOKABLE void setReaderColors(const QString& background, const QString& text);

    bool vertical_writing() const;
    void setVerticalWriting(bool vertical);
    int font_size() const;
    void setFontSize(int size);
    int horizontal_padding() const;
    void setHorizontalPadding(int padding);
    int vertical_padding() const;
    void setVerticalPadding(int padding);

    // Imports the EPUB into Books and opens it. The title names the folder.
    Q_INVOKABLE bool importAndOpen(const QUrl& file);
    // Opens an already imported book folder.
    Q_INVOKABLE bool openBook(const QString& folder);
    Q_INVOKABLE void closeBook();

    // The web view size; the injected page size follows it.
    Q_INVOKABLE void setPageSize(int width, int height);

    Q_INVOKABLE void goToChapter(int index);
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
    void errorChanged();
    void settingsChanged();
    void paletteChanged();
    void pageChanged(int index);
    // The page should run navigation for this direction ("forward"/"backward").
    void pageTurnRequested(const QString& direction);

   private:
    struct State;
    std::unique_ptr<State> state_;

    void load_chapter(int index, double progress);
    void reload_document();
    void persist_bookmark();
    std::string shell_document(const std::string& path);
};

}  // namespace niratan::viewmodels

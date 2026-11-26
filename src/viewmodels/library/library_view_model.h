// SPDX-License-Identifier: GPL-3.0-or-later
//
// State and commands for the bookshelf, following
// Features/Bookshelf/BookshelfViewModel.swift (loadBooks, sortBooks,
// shelfSections, shelf naming, import, delete, mark read, importDroppedEPUBs),
// Features/Bookshelf/BookshelfDropSupport.swift (BookshelfFileDropTarget),
// Features/Bookshelf/BookCell.swift (bookExportURL) and, for the save dialog,
// Niratan-win's NovelLibraryPageViewModel.ExportNovelAsync with
// NovelExportFileName.CreateBaseName and NovelLibraryService.ExportEpubAsync.
// Features/Bookshelf/BookView.swift supplies BookshelfLayout.
//
// The model lists the books under Books/ with their cover, progress and shelf
// membership. Progress is the bookmark's character count over bookinfo.json's
// total, or over the metadata character count when there is no bookinfo; a
// non-positive total reads as 0. The reading section is exactly
// 0 < progress < 0.999; finished books stay in All Books.
//
// The rules are the ones in models/books/library_model.h: this class only
// reads BookStorage into Library values, keeps the Library state, calls the
// model for selection, ordering, shelves, progress and mark-read, and projects
// the result into QVariantList. Sorting is Recent, Title or Manual, a user
// shelf uses its own id order for Manual, and an unknown shelf selection falls
// back to all.
//
// The class never exposes QML types beyond QVariantList, and never opens the
// reader: it emits bookOpened(folder) and the window decides. After a successful
// open, recordBookAccess follows NativeReaderView.swift (loadBook) by updating
// the latest stored metadata without changing the rest of the book record.

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::viewmodels {

class LibraryViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList books READ books NOTIFY booksChanged)
    Q_PROPERTY(QVariantList shelves READ shelves NOTIFY shelvesChanged)
    Q_PROPERTY(QString selectedShelf READ selected_shelf WRITE setSelectedShelf NOTIFY
                   selectionChanged)
    Q_PROPERTY(int sortOption READ sort_option WRITE setSortOption NOTIFY sortChanged)
    Q_PROPERTY(int coverWidth READ cover_width WRITE setCoverWidth NOTIFY layoutChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool importError READ isImportError NOTIFY errorChanged)
    // BookshelfViewModel.importBooksProgress: "Importing k / n..." while a
    // batch imports, empty for a single file and after the batch.
    Q_PROPERTY(QString importProgress READ import_progress NOTIFY importProgressChanged)
    Q_PROPERTY(int bookCount READ book_count NOTIFY booksChanged)
    // The badges of the shelf column's smart rows, LibraryShelfSidebar's
    // LibraryShelfSmartRow.count: "all", "reading" and "unshelved", each the
    // book count of that selection.
    Q_PROPERTY(QVariantMap smartCounts READ smart_counts NOTIFY booksChanged)

   public:
    explicit LibraryViewModel(QObject* parent = nullptr);
    ~LibraryViewModel() override;

    QVariantList books() const;
    QVariantList shelves() const;
    QVariantMap smart_counts() const;
    QString selected_shelf() const;
    void setSelectedShelf(const QString& shelf);
    int sort_option() const;
    void setSortOption(int option);
    int cover_width() const;
    void setCoverWidth(int width);
    QString error() const;
    bool isImportError() const;
    QString import_progress() const;
    int book_count() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void importBooks(const QList<QUrl>& files);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void openBook(const QString& folder);
    Q_INVOKABLE void recordBookAccess(const QString& folder);
    Q_INVOKABLE void renameBook(const QString& folder, const QString& title);
    Q_INVOKABLE void deleteBook(const QString& folder);
    Q_INVOKABLE bool isLocalBook(const QString& folder) const;
    Q_INVOKABLE void markRead(const QString& folder);
    // BookshelfFileDropTarget.accepts: at least one URL ends in ".epub",
    // case-insensitively. A mixed drop is accepted.
    Q_INVOKABLE bool acceptsDrop(const QList<QUrl>& files) const;
    // BookshelfViewModel.importDroppedEPUBs: keep only ".epub" URLs and
    // import them in order; false when none remain.
    Q_INVOKABLE bool importDroppedEPUBs(const QList<QUrl>& files);
    // BookCell.bookExportURL: true only when the book stores an EPUB name.
    Q_INVOKABLE bool canExportBook(const QString& folder) const;
    // NovelExportFileName.CreateBaseName for the book's display title.
    Q_INVOKABLE QString exportBaseName(const QString& folder) const;
    // NovelLibraryService.ExportEpubAsync after the save dialog. Success
    // reports bookExported; a refusal sets error without changing the library.
    Q_INVOKABLE void exportBook(const QString& folder, const QUrl& destination);

    Q_INVOKABLE QString createShelf(const QString& name);
    Q_INVOKABLE bool renameShelf(const QString& from, const QString& to);
    Q_INVOKABLE void deleteShelf(const QString& name);
    // The first shelf holding the book, following
    // BookshelfViewModel.shelfName(containing:); empty when it is on none or
    // unknown.
    Q_INVOKABLE QString shelfForBook(const QString& folder) const;
    Q_INVOKABLE void moveBookToShelf(const QString& folder, const QString& shelf);
    Q_INVOKABLE void moveBooksToShelf(const QStringList& folders, const QString& shelf);

    // ShelfView.reorderBook: require two distinct cards in the current full
    // section, then switch to Manual before persisting the order. The grid
    // sends only eligible drag hits, not filtered results or repeat hits.
    Q_INVOKABLE bool reorderBook(const QString& source, const QString& target);
    // LibraryShelfSidebar.onMove: custom rows only, with an insertion offset
    // in the original custom list. The sidebar disables this during rename.
    Q_INVOKABLE bool moveShelves(const QList<int>& source, int destination);

   signals:
    void booksChanged();
    void shelvesChanged();
    void selectionChanged();
    void sortChanged();
    void layoutChanged();
    void errorChanged();
    void bookOpened(const QString& folder);
    void dictionaryRequested();
    // A completed export, naming the destination that received the EPUB.
    void bookExported(const QString& destination);
    void importProgressChanged();

   private:
    struct State;
    std::unique_ptr<State> state_;
    QString import_progress_;
};

}  // namespace niratan::viewmodels

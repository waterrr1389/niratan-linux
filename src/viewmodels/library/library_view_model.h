// SPDX-License-Identifier: GPL-3.0-or-later
//
// State and commands for the bookshelf, following
// Features/Bookshelf/BookshelfViewModel.swift (loadBooks, sortBooks,
// shelfSections, shelf naming, import, delete, mark read) and
// Features/Bookshelf/BookView.swift (BookshelfLayout).
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
    Q_PROPERTY(int bookCount READ book_count NOTIFY booksChanged)

   public:
    explicit LibraryViewModel(QObject* parent = nullptr);
    ~LibraryViewModel() override;

    QVariantList books() const;
    QVariantList shelves() const;
    QString selected_shelf() const;
    void setSelectedShelf(const QString& shelf);
    int sort_option() const;
    void setSortOption(int option);
    int cover_width() const;
    void setCoverWidth(int width);
    QString error() const;
    int book_count() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void importBooks(const QList<QUrl>& files);
    Q_INVOKABLE void openBook(const QString& folder);
    Q_INVOKABLE void recordBookAccess(const QString& folder);
    Q_INVOKABLE void renameBook(const QString& folder, const QString& title);
    Q_INVOKABLE void deleteBook(const QString& folder);
    Q_INVOKABLE bool isLocalBook(const QString& folder) const;
    Q_INVOKABLE void markRead(const QString& folder);

    Q_INVOKABLE QString createShelf(const QString& name);
    Q_INVOKABLE bool renameShelf(const QString& from, const QString& to);
    Q_INVOKABLE void deleteShelf(const QString& name);
    Q_INVOKABLE void moveBookToShelf(const QString& folder, const QString& shelf);
    Q_INVOKABLE void moveBooksToShelf(const QStringList& folders, const QString& shelf);

   signals:
    void booksChanged();
    void shelvesChanged();
    void selectionChanged();
    void sortChanged();
    void layoutChanged();
    void errorChanged();
    void bookOpened(const QString& folder);
    void dictionaryRequested();

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::viewmodels

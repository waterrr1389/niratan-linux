// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bookshelf view model's shelf-column counts, following
// Features/Bookshelf/LibraryShelfSidebar.swift (LibraryShelfSmartRow.count,
// drawn as each row's badge) and NativeMac/NativeReuseViews.swift
// (NativeBookshelfReuseView.shelfSmartRows: All Books, Currently Reading and
// Unshelved, each counted with BookshelfViewModel.bookCount(for:)).
#include "models/books/book.h"
#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"
#include "viewmodels/library/library_view_model.h"

#include <QSignalSpy>
#include <QtTest>

#include "data_root.h"

using niratan::books::BookInfo;
using niratan::books::Bookmark;
using niratan::books::BookMetadata;
using niratan::books::BookShelf;
using niratan::books::BookStorage;
using niratan::books::ChapterInfo;
using niratan::viewmodels::LibraryViewModel;

namespace {

void add_book(const BookStorage& storage, const std::string& folder, double progress) {
    BookMetadata book;
    book.id = "ID-" + folder;
    book.title = folder;
    book.folder = folder;
    book.character_count = 1000;
    QVERIFY(storage.save_metadata(book));
    Bookmark bookmark;
    bookmark.progress = progress;
    bookmark.character_count = static_cast<int>(progress * 1000);
    QVERIFY(storage.save_bookmark(folder, bookmark));
}

}  // namespace

class LibraryViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void smart_counts_follow_the_library();
    void smart_counts_change_with_shelf_membership();
    void shelf_for_book_follows_moves();
    void smart_counts_drop_a_book_marked_read();
};

// All counts every book, Currently Reading only 0 < progress < 0.999, and
// Unshelved the books on no shelf.
void LibraryViewModelTest::smart_counts_follow_the_library() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const BookStorage storage(niratan::storage::data_root());
    add_book(storage, "reading", 0.5);
    add_book(storage, "finished", 1.0);
    add_book(storage, "unread", 0.0);
    QVERIFY(storage.save_shelves({BookShelf{"Shelf", {"ID-reading"}}}));

    LibraryViewModel model;
    const QVariantMap counts = model.property("smartCounts").toMap();
    QCOMPARE(counts.value(QStringLiteral("all")).toInt(), 3);
    QCOMPARE(counts.value(QStringLiteral("reading")).toInt(), 1);
    QCOMPARE(counts.value(QStringLiteral("unshelved")).toInt(), 2);
}

// Moving a book onto a shelf lowers Unshelved and notifies through
// booksChanged, the counts' change signal.
void LibraryViewModelTest::smart_counts_change_with_shelf_membership() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const BookStorage storage(niratan::storage::data_root());
    add_book(storage, "first", 0.2);
    add_book(storage, "second", 0.0);
    QVERIFY(storage.save_shelves({BookShelf{"Shelf", {}}}));

    LibraryViewModel model;
    QCOMPARE(model.property("smartCounts").toMap().value(QStringLiteral("unshelved")).toInt(), 2);

    QSignalSpy changed(&model, &LibraryViewModel::booksChanged);
    model.moveBookToShelf(QStringLiteral("second"), QStringLiteral("Shelf"));
    QVERIFY(changed.count() > 0);
    const QVariantMap counts = model.property("smartCounts").toMap();
    QCOMPARE(counts.value(QStringLiteral("all")).toInt(), 2);
    QCOMPARE(counts.value(QStringLiteral("reading")).toInt(), 1);
    QCOMPARE(counts.value(QStringLiteral("unshelved")).toInt(), 1);
}

// shelfForBook names the first shelf holding the book, as
// BookshelfViewModel.shelfName(containing:) does, and follows moves; None and
// an unknown folder give an empty name.
void LibraryViewModelTest::shelf_for_book_follows_moves() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const BookStorage storage(niratan::storage::data_root());
    add_book(storage, "a", 0.0);
    add_book(storage, "b", 0.0);
    add_book(storage, "c", 0.0);
    QVERIFY(storage.save_shelves(
        {BookShelf{"Novel", {"ID-a", "ID-b", "ID-c"}}, BookShelf{"Other", {"ID-c"}}}));

    LibraryViewModel model;
    QCOMPARE(model.shelfForBook(QStringLiteral("a")), QStringLiteral("Novel"));
    QCOMPARE(model.shelfForBook(QStringLiteral("c")), QStringLiteral("Novel"));
    model.moveBookToShelf(QStringLiteral("a"), QStringLiteral("Other"));
    QCOMPARE(model.shelfForBook(QStringLiteral("a")), QStringLiteral("Other"));
    QCOMPARE(model.shelfForBook(QStringLiteral("b")), QStringLiteral("Novel"));
    model.moveBookToShelf(QStringLiteral("a"), QString());
    QCOMPARE(model.shelfForBook(QStringLiteral("a")), QString());
    QCOMPARE(model.shelfForBook(QStringLiteral("b")), QStringLiteral("Novel"));
    QCOMPARE(model.shelfForBook(QStringLiteral("missing")), QString());
}

// Marking a book read moves it out of Currently Reading without a refresh;
// the count still follows and booksChanged fires.
void LibraryViewModelTest::smart_counts_drop_a_book_marked_read() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const BookStorage storage(niratan::storage::data_root());
    add_book(storage, "reading", 0.4);
    BookInfo info;
    info.character_count = 1000;
    info.chapter_info["chapter"] = ChapterInfo{0, 0, 1000, std::nullopt};
    QVERIFY(storage.save_book_info("reading", info));

    LibraryViewModel model;
    QCOMPARE(model.property("smartCounts").toMap().value(QStringLiteral("reading")).toInt(), 1);

    QSignalSpy changed(&model, &LibraryViewModel::booksChanged);
    model.markRead(QStringLiteral("reading"));
    QVERIFY(changed.count() > 0);
    QCOMPARE(model.property("smartCounts").toMap().value(QStringLiteral("reading")).toInt(), 0);
    QCOMPARE(model.property("smartCounts").toMap().value(QStringLiteral("all")).toInt(), 1);
}

QTEST_GUILESS_MAIN(LibraryViewModelTest)
#include "tst_library_view_model.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bookshelf view model's shelf-column counts, following
// Features/Bookshelf/LibraryShelfSidebar.swift (LibraryShelfSmartRow.count,
// drawn as each row's badge) and NativeMac/NativeReuseViews.swift
// (NativeBookshelfReuseView.shelfSmartRows: All Books, Currently Reading and
// Unshelved, each counted with BookshelfViewModel.bookCount(for:)).
// Reordering follows ShelfView.reorderBook and BookshelfViewModel.moveBook,
// moveShelves, persistManualBookOrder and saveShelves.
#include "models/books/book.h"
#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"
#include "viewmodels/library/library_view_model.h"

#include <QFile>
#include <QSignalSpy>
#include <QtTest>

#include "../flows/epub_fixture.h"
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

QStringList entries(const QVariantList& rows, const QString& key) {
    QStringList values;
    for (const QVariant& row : rows) {
        values.append(row.toMap().value(key).toString());
    }
    return values;
}

}  // namespace

class LibraryViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void smart_counts_follow_the_library();
    void smart_counts_change_with_shelf_membership();
    void shelf_for_book_follows_moves();
    void smart_counts_drop_a_book_marked_read();
    void drop_accepts_a_mixed_epub_drag();
    void drop_imports_only_epub_files();
    void drop_skips_duplicates_and_reports_failures_by_name();
    void export_commands_copy_and_report();
    void book_drag_persists_manual_order_data();
    void book_drag_persists_manual_order();
    void named_shelf_drag_persists_only_its_order();
    void shelf_drag_persists_order_without_changing_selection();
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

// BookshelfFileDropTarget.accepts accepts a mixed drag when at least one file
// is an EPUB, whatever the case of its extension.
void LibraryViewModelTest::drop_accepts_a_mixed_epub_drag() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    LibraryViewModel model;
    QVERIFY(model.acceptsDrop({QUrl::fromLocalFile(QStringLiteral("/tmp/A.EPUB")),
                               QUrl::fromLocalFile(QStringLiteral("/tmp/notes.txt"))}));
    QVERIFY(!model.acceptsDrop({QUrl::fromLocalFile(QStringLiteral("/tmp/notes.txt"))}));
    QVERIFY(!model.acceptsDrop({}));
    QVERIFY(!model.acceptsDrop({QUrl::fromLocalFile(QStringLiteral("/tmp/plain-epub"))}));
}

// importDroppedEPUBs keeps the EPUB URLs in dropped order and does nothing
// when none remain.
void LibraryViewModelTest::drop_imports_only_epub_files() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString first = root.path() + QStringLiteral("/First.epub");
    const QString second = root.path() + QStringLiteral("/Second.epub");
    QVERIFY(niratan::test::write_epub(first.toStdString(), "First", "一", "二"));
    QVERIFY(niratan::test::write_epub(second.toStdString(), "Second", "三", "四"));
    const QString notes = root.path() + QStringLiteral("/notes.txt");
    QFile notes_file(notes);
    QVERIFY(notes_file.open(QIODevice::WriteOnly));
    notes_file.write("not an epub");
    notes_file.close();

    LibraryViewModel model;
    QSignalSpy errors(&model, &LibraryViewModel::errorChanged);
    QVERIFY(model.importDroppedEPUBs(
        {QUrl::fromLocalFile(notes), QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)}));
    QCOMPARE(model.property("bookCount").toInt(), 2);
    QCOMPARE(errors.count(), 0);
    QVERIFY(!model.importDroppedEPUBs({QUrl::fromLocalFile(notes)}));
    QCOMPARE(model.property("bookCount").toInt(), 2);
    QCOMPARE(errors.count(), 0);
}

// processImport skips an existing title silently; a real failure is reported
// by file name after the other files were attempted.
void LibraryViewModelTest::drop_skips_duplicates_and_reports_failures_by_name() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString same = root.path() + QStringLiteral("/Same.epub");
    const QString good = root.path() + QStringLiteral("/Good.epub");
    QVERIFY(niratan::test::write_epub(same.toStdString(), "Same", "一", "二"));
    QVERIFY(niratan::test::write_epub(good.toStdString(), "Good", "三", "四"));
    const QString broken = root.path() + QStringLiteral("/broken.epub");
    QFile broken_file(broken);
    QVERIFY(broken_file.open(QIODevice::WriteOnly));
    broken_file.write("not an epub");
    broken_file.close();

    LibraryViewModel model;
    QVERIFY(model.importDroppedEPUBs({QUrl::fromLocalFile(same), QUrl::fromLocalFile(same)}));
    QCOMPARE(model.property("bookCount").toInt(), 1);
    QCOMPARE(model.error(), QString());

    QVERIFY(model.importDroppedEPUBs({QUrl::fromLocalFile(broken), QUrl::fromLocalFile(good)}));
    QCOMPARE(model.property("bookCount").toInt(), 2);
    QVERIFY(model.error().contains(QStringLiteral("broken.epub")));
    QVERIFY(model.isImportError());
}

// ExportNovelAsync names the file from the display title, copies the EPUB and
// reports success; a refused destination reports an error and changes nothing.
void LibraryViewModelTest::export_commands_copy_and_report() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const BookStorage storage(niratan::storage::data_root());
    const QString source = root.path() + QStringLiteral("/Story.epub");
    QVERIFY(niratan::test::write_epub(source.toStdString(), "Story", "一", "二"));
    QVERIFY(storage.import_book(source.toStdString()).book.has_value());
    auto metadata = storage.load_metadata("Story");
    QVERIFY(metadata.has_value());
    metadata->renamed_title = "\u3000Renamed\u3000";
    QVERIFY(storage.save_metadata(*metadata));
    add_book(storage, "plain", 0.0);

    LibraryViewModel model;
    QVERIFY(model.canExportBook(QStringLiteral("Story")));
    QVERIFY(!model.canExportBook(QStringLiteral("plain")));
    QVERIFY(!model.canExportBook(QStringLiteral("missing")));
    QCOMPARE(model.exportBaseName(QStringLiteral("Story")), QStringLiteral("Renamed"));

    const QVariantList before = model.books();
    const QString destination = root.path() + QStringLiteral("/out.epub");
    QSignalSpy exported(&model, &LibraryViewModel::bookExported);
    model.exportBook(QStringLiteral("Story"), QUrl::fromLocalFile(destination));
    QCOMPARE(exported.count(), 1);
    QCOMPARE(exported.at(0).at(0).toString(), destination);
    QVERIFY(QFile::exists(destination));
    QCOMPARE(model.books(), before);
    QVERIFY(storage.load_shelves().empty());

    const QString invalid = root.path() + QStringLiteral("/missing/out.epub");
    QSignalSpy errors(&model, &LibraryViewModel::errorChanged);
    model.exportBook(QStringLiteral("Story"), QUrl::fromLocalFile(invalid));
    QCOMPARE(exported.count(), 1);
    QVERIFY(!model.error().isEmpty());
    QVERIFY(errors.count() > 0);
    QCOMPARE(model.books(), before);
}

void LibraryViewModelTest::book_drag_persists_manual_order_data() {
    QTest::addColumn<QString>("section");
    QTest::addColumn<int>("starting_sort");
    QTest::addColumn<QStringList>("displayed");
    QTest::addColumn<QStringList>("stored");
    QTest::newRow("all from title") << QStringLiteral("all") << 1 << QStringList{"a", "d", "c", "b"}
                                    << QStringList{"ID-a", "ID-d", "ID-c", "ID-b"};
    QTest::newRow("unshelved from recent")
        << QStringLiteral("unshelved") << 0 << QStringList{"a", "d"}
        << QStringList{"ID-c", "ID-b", "ID-a", "ID-d"};
}

void LibraryViewModelTest::book_drag_persists_manual_order() {
    QFETCH(QString, section);
    QFETCH(int, starting_sort);
    QFETCH(QStringList, displayed);
    QFETCH(QStringList, stored);
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const BookStorage storage(niratan::storage::data_root());
    add_book(storage, "a", 0.2);
    add_book(storage, "b", 0.2);
    add_book(storage, "c", 0.0);
    add_book(storage, "d", 0.0);
    const std::vector<BookShelf> shelves{{"Shelf", {"ID-b", "ID-c"}}};
    const std::vector<std::string> original{"ID-d", "ID-c", "ID-b", "ID-a"};
    QVERIFY(storage.save_shelves(shelves));
    QVERIFY(storage.save_book_order(original));

    LibraryViewModel model;
    model.setSortOption(starting_sort);
    QSignalSpy sorted(&model, &LibraryViewModel::sortChanged);
    model.setSelectedShelf(QStringLiteral("reading"));
    QVERIFY(!model.reorderBook(QStringLiteral("a"), QStringLiteral("b")));
    model.setSelectedShelf(QStringLiteral("unshelved"));
    QVERIFY(!model.reorderBook(QStringLiteral("a"), QStringLiteral("b")));
    QVERIFY(!model.reorderBook(QStringLiteral("b"), QStringLiteral("a")));
    QVERIFY(!model.reorderBook(QStringLiteral("a"), QStringLiteral("a")));
    QVERIFY(!model.reorderBook(QStringLiteral("a"), QStringLiteral("missing")));
    QCOMPARE(sorted.count(), 0);
    QCOMPARE(storage.load_library_prefs().sort_option, starting_sort);
    QCOMPARE(storage.load_book_order(), original);

    model.setSelectedShelf(section);
    // The title order would move a after d. Manual order puts a before d.
    QVERIFY(model.reorderBook(QStringLiteral("a"), QStringLiteral("d")));
    QCOMPARE(sorted.count(), 1);
    QCOMPARE(model.sort_option(), 2);
    QCOMPARE(storage.load_library_prefs().sort_option, 2);
    QCOMPARE(entries(model.books(), QStringLiteral("folder")), displayed);
    std::vector<std::string> expected;
    for (const QString& id : stored) {
        expected.push_back(id.toStdString());
    }
    QCOMPARE(storage.load_book_order(), expected);
    QCOMPARE(storage.load_shelves(), shelves);
}

void LibraryViewModelTest::named_shelf_drag_persists_only_its_order() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const BookStorage storage(niratan::storage::data_root());
    add_book(storage, "a", 0.0);
    add_book(storage, "b", 0.0);
    add_book(storage, "c", 0.0);
    const std::vector<std::string> global{"ID-c", "ID-b", "ID-a"};
    QVERIFY(storage.save_book_order(global));
    QVERIFY(storage.save_shelves(
        {BookShelf{"Shelf", {"ID-a", "ID-b"}}, BookShelf{"Other", {"ID-c", "ID-a"}}}));
    LibraryViewModel model;
    model.setSortOption(1);
    model.setSelectedShelf(QStringLiteral("shelf:Shelf"));
    QVERIFY(model.reorderBook(QStringLiteral("a"), QStringLiteral("b")));
    QCOMPARE(storage.load_library_prefs().sort_option, 2);
    QCOMPARE(storage.load_book_order(), global);
    const std::vector<BookShelf> expected{{"Shelf", {"ID-b", "ID-a"}}, {"Other", {"ID-c", "ID-a"}}};
    QCOMPARE(storage.load_shelves(), expected);
    QCOMPARE(entries(model.books(), QStringLiteral("folder")), (QStringList{"b", "a"}));
}

void LibraryViewModelTest::shelf_drag_persists_order_without_changing_selection() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const BookStorage storage(niratan::storage::data_root());
    add_book(storage, "a", 0.0);
    add_book(storage, "b", 0.0);
    const BookShelf first{"First", {"ID-a"}};
    const BookShelf second{"Second", {"ID-b"}};
    const BookShelf third{"Third", {}};
    QVERIFY(storage.save_shelves({first, second, third}));
    LibraryViewModel model;
    model.setSelectedShelf(QStringLiteral("shelf:First"));
    QSignalSpy shelves_changed(&model, &LibraryViewModel::shelvesChanged);
    QSignalSpy selection_changed(&model, &LibraryViewModel::selectionChanged);
    QVERIFY(model.moveShelves({0}, 3));
    const std::vector<BookShelf> expected{second, third, first};
    QCOMPARE(storage.load_shelves(), expected);
    QCOMPARE(entries(model.shelves(), QStringLiteral("name")),
             (QStringList{"Second", "Third", "First"}));
    QCOMPARE(entries(model.books(), QStringLiteral("folder")), (QStringList{"a"}));
    QCOMPARE(model.selected_shelf(), QStringLiteral("shelf:First"));
    QCOMPARE(shelves_changed.count(), 1);
    QCOMPARE(selection_changed.count(), 0);
}

QTEST_GUILESS_MAIN(LibraryViewModelTest)
#include "tst_library_view_model.moc"

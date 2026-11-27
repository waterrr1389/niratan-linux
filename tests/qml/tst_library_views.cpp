// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bookshelf's search field and inline shelf rename, and the reader's
// focus-mode key, following NativeMac/NativeReuseViews.swift (`body`,
// `displayedSection`, `ContentUnavailableView.search`),
// Features/Bookshelf/LibraryShelfSidebar.swift (`shelfRow`, `beginRename`,
// `commitRename`, `cancelRename`, the `.onMove` gate) and
// Features/Reader/ReaderShortcutActions.swift (`toggleFocusMode`) with
// NativeReaderView's `handleReaderToggleFocusModeShortcut`.
//
// The library has three books: "Alpha", "Beta" shown as "Gamma", and "Zeta"
// shown as "Delta", so a query can match either the stored or the displayed
// title. "Shelf A" holds Alpha and "Other" holds Beta. The reader page opens
// Alpha; its web view never loads, so the test answers the restore itself.

#include "models/books/book.h"
#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"
#include "viewmodels/library/library_view_model.h"
#include "viewmodels/reader/reader_view_model.h"

#include <QDir>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrlQuery>
#include <QVariantMap>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>
#include <functional>
#include <memory>

#include "data_root.h"
#include "epub_fixture.h"

using niratan::books::BookMetadata;
using niratan::books::BookShelf;
using niratan::books::BookStorage;
using niratan::viewmodels::LibraryViewModel;
using niratan::viewmodels::ReaderViewModel;

namespace {

bool add_book(const BookStorage& storage, const std::string& folder, const std::string& title,
              const std::string& renamed) {
    BookMetadata book;
    book.id = "ID-" + folder;
    book.folder = folder;
    book.title = title;
    if (!renamed.empty()) {
        book.renamed_title = renamed;
    }
    return storage.save_metadata(book);
}

// Alpha carries a readable EPUB so the reader page can open it.
bool add_readable_book(const BookStorage& storage) {
    const auto directory = storage.book_directory("alpha");
    if (!directory || !QDir().mkpath(QString::fromStdString(directory->string()))) {
        return false;
    }
    std::string second;
    for (int index = 0; index < 1000; ++index) {
        second += "あ";
    }
    if (!niratan::test::write_epub(*directory / "book.epub", "Alpha", "あいうえお", second)) {
        return false;
    }
    BookMetadata book;
    book.id = "ID-alpha";
    book.folder = "alpha";
    book.title = "Alpha";
    book.epub = "book.epub";
    return storage.save_metadata(book);
}

QString revision(const ReaderViewModel& reader) {
    return QUrlQuery(QUrl(reader.chapter_url())).queryItemValue(QStringLiteral("v"));
}

void complete_restore(ReaderViewModel& reader) {
    reader.message(QStringLiteral("restoreCompleted"),
                   QString::fromUtf8(QJsonDocument(QJsonObject{{"token", revision(reader)}})
                                         .toJson(QJsonDocument::Compact)));
}

QList<QQuickItem*> items_named(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> found;
    if (root == nullptr) {
        return found;
    }
    std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
        if (item->objectName() == name) {
            found.append(item);
        }
        for (QQuickItem* child : item->childItems()) {
            walk(child);
        }
    };
    walk(root);
    return found;
}

}  // namespace

class LibraryViewsTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void cleanup();

    void search_narrows_the_shelf_and_marks_it_filtered();
    void no_results_names_the_query();
    void inline_rename_commits_and_cancels();
    void focus_key_toggles_and_skips_a_text_field();

   private:
    bool show_page(const QUrl& url, int width, int height);
    QQuickItem* named(const QString& name) const;
    QList<QQuickItem*> named_items(const QString& name) const;
    QQuickItem* shelf_row(const QString& name) const;
    QStringList shelf_names() const;
    QStringList grid_titles() const;
    QQuickItem* visible_name_field() const;
    bool begin_rename(const QString& name);

    niratan::test::TempDataRoot data_root_;
    QQmlEngine engine_;
    std::unique_ptr<QQuickWindow> window_;
    std::unique_ptr<QObject> object_;
    QQuickItem* page_ = nullptr;
};

void LibraryViewsTest::initTestCase() {
    QVERIFY(data_root_.isValid());
    const BookStorage storage(niratan::storage::data_root());
    QVERIFY(add_readable_book(storage));
    QVERIFY(add_book(storage, "beta", "Beta", "Gamma"));
    QVERIFY(add_book(storage, "zeta", "Zeta", "Delta"));
    QVERIFY(storage.save_shelves(
        {BookShelf{"Shelf A", {"ID-alpha"}}, BookShelf{"Other", {"ID-beta"}}}));
}

void LibraryViewsTest::cleanup() {
    if (auto* reader = ReaderViewModel::instance(0); reader && reader->has_book()) {
        reader->closeBook();
    }
    window_.reset();
    object_.reset();
    page_ = nullptr;
}

bool LibraryViewsTest::show_page(const QUrl& url, int width, int height) {
    window_ = std::make_unique<QQuickWindow>();
    window_->resize(width, height);
    QQmlComponent component(&engine_, url);
    if (component.isError()) {
        qWarning() << component.errors();
        return false;
    }
    object_.reset(component.beginCreate(engine_.rootContext()));
    auto* item = qobject_cast<QQuickItem*>(object_.get());
    if (item == nullptr) {
        return false;
    }
    object_.release();
    item->setParent(window_.get());
    item->setParentItem(window_->contentItem());
    item->setSize(QSizeF(width, height));
    component.completeCreate();
    if (component.isError()) {
        qWarning() << component.errors();
        return false;
    }
    page_ = item;
    window_->show();
    return QTest::qWaitForWindowExposed(window_.get());
}

QList<QQuickItem*> LibraryViewsTest::named_items(const QString& name) const {
    return items_named(window_ ? window_->contentItem() : nullptr, name);
}

QQuickItem* LibraryViewsTest::named(const QString& name) const {
    const auto found = named_items(name);
    return found.isEmpty() ? nullptr : found.first();
}

QQuickItem* LibraryViewsTest::shelf_row(const QString& name) const {
    const auto rows = named_items(QStringLiteral("libraryCustomShelfRow"));
    for (QQuickItem* row : rows) {
        if (row->property("modelData").toMap().value(QStringLiteral("name")).toString() == name) {
            return row;
        }
    }
    return nullptr;
}

QStringList LibraryViewsTest::shelf_names() const {
    auto* model = page_ ? page_->findChild<LibraryViewModel*>() : nullptr;
    if (model == nullptr) {
        return {};
    }
    QStringList names;
    for (const QVariant& row : model->shelves()) {
        names.append(row.toMap().value(QStringLiteral("name")).toString());
    }
    return names;
}

QStringList LibraryViewsTest::grid_titles() const {
    QStringList titles;
    if (page_ == nullptr) {
        return titles;
    }
    for (const QVariant& row : page_->property("displayedBooks").toList()) {
        titles.append(row.toMap().value(QStringLiteral("title")).toString());
    }
    return titles;
}

QQuickItem* LibraryViewsTest::visible_name_field() const {
    for (QQuickItem* field : named_items(QStringLiteral("libraryShelfNameField"))) {
        if (field->isVisible()) {
            return field;
        }
    }
    return nullptr;
}

bool LibraryViewsTest::begin_rename(const QString& name) {
    auto* row = shelf_row(name);
    if (row == nullptr || !QMetaObject::invokeMethod(row, "contextMenuRequested")) {
        return false;
    }
    auto* rename = named(QStringLiteral("libraryRenameShelfItem"));
    if (rename == nullptr) {
        return false;
    }
    for (int waited = 0; waited < 200 && !rename->property("visible").toBool(); waited += 10) {
        QTest::qWait(10);
    }
    return rename->property("visible").toBool() && QMetaObject::invokeMethod(rename, "triggered");
}

// B1/B2/B3: the toolbar field narrows the sorted shelf by either title, turns
// cover reordering off, and clearing it restores both.
void LibraryViewsTest::search_narrows_the_shelf_and_marks_it_filtered() {
    QVERIFY(show_page(QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/LibraryPage.qml")),
                      1100, 720));
    auto* grid = named(QStringLiteral("libraryBookGrid"));
    auto* field = named(QStringLiteral("librarySearchField"));
    QVERIFY(grid);
    QVERIFY(field);
    const QStringList all = grid_titles();
    QCOMPARE(all.size(), 3);
    QCOMPARE(grid->property("count").toInt(), 3);
    QVERIFY(grid->property("reorderEnabled").toBool());

    field->setProperty("text", QStringLiteral("  ga  "));
    QTRY_COMPARE(grid_titles(), (QStringList{"Gamma"}));
    QCOMPARE(grid->property("count").toInt(), 1);
    QVERIFY(!grid->property("reorderEnabled").toBool());

    // "Zeta" is the stored title of the book shown as "Delta".
    field->setProperty("text", QStringLiteral("zeta"));
    QTRY_COMPARE(grid_titles(), (QStringList{"Delta"}));

    // Selection keeps the query and the filter; the field returns afterwards.
    page_->setProperty("selecting", true);
    QTRY_VERIFY(!field->isVisible());
    QCOMPARE(grid_titles(), (QStringList{"Delta"}));
    page_->setProperty("selecting", false);
    QTRY_VERIFY(field->isVisible());
    QCOMPARE(field->property("text").toString(), QStringLiteral("zeta"));

    field->setProperty("text", QString());
    QTRY_COMPARE(grid_titles(), all);
    QVERIFY(grid->property("reorderEnabled").toBool());
}

// B2: no matches show "No Results" naming the query instead of the shelf's
// empty copy, and clearing the query brings the shelf back.
void LibraryViewsTest::no_results_names_the_query() {
    QVERIFY(show_page(QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/LibraryPage.qml")),
                      1100, 720));
    auto* field = named(QStringLiteral("librarySearchField"));
    QVERIFY(field);
    field->setProperty("text", QStringLiteral("qqq"));
    auto* empty = named(QStringLiteral("libraryShelfEmptyView"));
    QVERIFY(empty);
    QTRY_VERIFY(empty->isVisible());
    QCOMPARE(empty->property("title").toString(), QStringLiteral("No Results"));
    QVERIFY(empty->property("message").toString().contains(QStringLiteral("qqq")));
    QCOMPARE(named(QStringLiteral("libraryBookGrid"))->property("count").toInt(), 0);

    field->setProperty("text", QString());
    QTRY_VERIFY(!empty->isVisible());
    QCOMPARE(named(QStringLiteral("libraryBookGrid"))->property("count").toInt(), 3);
}

// B5-B8: the context menu's Rename starts an inline editor holding the name
// with its placeholder, Return commits and follows the selection, Escape and
// an empty name change nothing, and a duplicate is rejected.
void LibraryViewsTest::inline_rename_commits_and_cancels() {
    QVERIFY(show_page(QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/LibraryPage.qml")),
                      1100, 720));
    auto* model = page_->findChild<LibraryViewModel*>();
    auto* column = named(QStringLiteral("libraryShelfColumn"));
    QVERIFY(model);
    QVERIFY(column);
    QVERIFY(column->property("shelfDraggingEnabled").toBool());

    auto* row = shelf_row(QStringLiteral("Shelf A"));
    QVERIFY(row);
    QMetaObject::invokeMethod(row, "clicked");
    QTRY_COMPARE(model->selected_shelf(), QStringLiteral("shelf:Shelf A"));

    // A double click starts the same inline editor (LibraryShelfSidebar's
    // primaryAction).
    row = shelf_row(QStringLiteral("Shelf A"));
    QVERIFY(row);
    const QPoint row_point =
        row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint();
    QTest::mouseDClick(window_.get(), Qt::LeftButton, Qt::NoModifier, row_point);
    QTRY_VERIFY(visible_name_field());
    auto* editor = visible_name_field();
    QVERIFY(editor);
    QCOMPARE(editor->property("text").toString(), QStringLiteral("Shelf A"));
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QTRY_VERIFY(!visible_name_field());

    QVERIFY(begin_rename(QStringLiteral("Shelf A")));
    editor = visible_name_field();
    QVERIFY(editor);
    QCOMPARE(editor->property("text").toString(), QStringLiteral("Shelf A"));
    QCOMPARE(editor->property("placeholderText").toString(), QStringLiteral("Shelf name"));
    QTRY_VERIFY(editor->property("activeFocus").toBool());
    QVERIFY(shelf_row(QStringLiteral("Shelf A"))->property("editing").toBool());
    QVERIFY(!column->property("shelfDraggingEnabled").toBool());

    editor->setProperty("text", QStringLiteral("Shelf B"));
    QTest::keyClick(window_.get(), Qt::Key_Return);
    QTRY_VERIFY(shelf_names().contains(QStringLiteral("Shelf B")));
    QVERIFY(!shelf_names().contains(QStringLiteral("Shelf A")));
    QTRY_COMPARE(model->selected_shelf(), QStringLiteral("shelf:Shelf B"));
    QTRY_VERIFY(column->property("shelfDraggingEnabled").toBool());

    QVERIFY(begin_rename(QStringLiteral("Shelf B")));
    editor = visible_name_field();
    QVERIFY(editor);
    QTRY_VERIFY(editor->property("activeFocus").toBool());
    editor->setProperty("text", QStringLiteral("Cancelled"));
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QTRY_VERIFY(!visible_name_field());
    QVERIFY(shelf_names().contains(QStringLiteral("Shelf B")));
    QVERIFY(!shelf_names().contains(QStringLiteral("Cancelled")));

    QVERIFY(begin_rename(QStringLiteral("Shelf B")));
    editor = visible_name_field();
    QVERIFY(editor);
    QTRY_VERIFY(editor->property("activeFocus").toBool());
    editor->setProperty("text", QStringLiteral("Shelf C"));
    column->forceActiveFocus();
    QTRY_VERIFY(shelf_names().contains(QStringLiteral("Shelf C")));
    QVERIFY(!shelf_names().contains(QStringLiteral("Shelf B")));
    QVERIFY(!visible_name_field());

    QVERIFY(begin_rename(QStringLiteral("Shelf C")));
    editor = visible_name_field();
    QVERIFY(editor);
    QTRY_VERIFY(editor->property("activeFocus").toBool());
    editor->setProperty("text", QString());
    QTest::keyClick(window_.get(), Qt::Key_Return);
    QTRY_VERIFY(!visible_name_field());
    QVERIFY(shelf_names().contains(QStringLiteral("Shelf C")));
    QVERIFY(shelf_names().contains(QStringLiteral("Other")));
    QVERIFY(column->property("shelfDraggingEnabled").toBool());

    QVERIFY(begin_rename(QStringLiteral("Shelf C")));
    editor = visible_name_field();
    QVERIFY(editor);
    QTRY_VERIFY(editor->property("activeFocus").toBool());
    editor->setProperty("text", QStringLiteral("Other"));
    QTest::keyClick(window_.get(), Qt::Key_Return);
    QTRY_VERIFY(!visible_name_field());
    QVERIFY(shelf_names().contains(QStringLiteral("Shelf C")));
    QVERIFY(shelf_names().contains(QStringLiteral("Other")));
    QVERIFY(column->property("shelfDraggingEnabled").toBool());
}

// B4: "f" toggles focus mode and is left to a focused text field.
void LibraryViewsTest::focus_key_toggles_and_skips_a_text_field() {
    auto* reader = ReaderViewModel::instance(0);
    QVERIFY(reader);
    QVERIFY(show_page(QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/ReaderPage.qml")),
                      1100, 760));
    QVERIFY(reader->openBook(QStringLiteral("alpha")));
    complete_restore(*reader);
    QTest::qWait(50);

    QTest::keyClick(window_.get(), Qt::Key_F);
    QTRY_VERIFY(page_->property("focusMode").toBool());
    QTest::keyClick(window_.get(), Qt::Key_F);
    QTRY_VERIFY(!page_->property("focusMode").toBool());

    QQmlComponent fieldComponent(&engine_);
    fieldComponent.setData("import QtQuick.Controls\nTextField { objectName: 'testProbeField' }",
                           QUrl());
    std::unique_ptr<QObject> probe(fieldComponent.create());
    QVERIFY2(probe, qPrintable(fieldComponent.errorString()));
    auto* field = qobject_cast<QQuickItem*>(probe.get());
    QVERIFY(field);
    probe.release();
    field->setParent(window_.get());
    field->setParentItem(window_->contentItem());
    field->forceActiveFocus();
    QTRY_VERIFY(field->hasActiveFocus());
    QTest::keyClick(window_.get(), Qt::Key_F);
    QTest::qWait(50);
    QVERIFY(!page_->property("focusMode").toBool());
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));
    LibraryViewsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_library_views.moc"

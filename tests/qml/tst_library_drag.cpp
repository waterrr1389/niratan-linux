// SPDX-License-Identifier: GPL-3.0-or-later
//
// Drag reordering wiring for the bookshelf, following ShelfView.reorderBook,
// beginDragIfNeeded, endDrag and bookDragVisualState, and
// LibraryShelfSidebar.body with NativeMac/NativeReuseViews.swift's onMove.
// Real pointer input drives the page: a click still opens a book, a drag moves
// a card and clears its source/target state, Reading and filtered gestures do
// nothing, and custom shelf rows move while smart rows and rename mode do not.

#include "models/books/book.h"
#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"
#include "viewmodels/library/library_view_model.h"

#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"

#include "../viewmodels/data_root.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QUrl>
#include <QTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <memory>
#include <string>
#include <vector>

namespace {

QStringList g_warnings;
QtMessageHandler g_previous_handler = nullptr;

void collect_qml_messages(QtMsgType type, const QMessageLogContext& context,
                          const QString& message) {
    if (type == QtWarningMsg || type == QtCriticalMsg) {
        const QString category = QString::fromUtf8(context.category ? context.category : "");
        if (category.startsWith(QStringLiteral("qml")) ||
            message.startsWith(QStringLiteral("QML")) || message.contains(QStringLiteral(".qml"))) {
            g_warnings.append(message);
        }
    }
    if (g_previous_handler != nullptr) {
        g_previous_handler(type, context, message);
    }
}

void collect_visual_items(QQuickItem* item, QList<QQuickItem*>& items) {
    items.append(item);
    for (QQuickItem* child : item->childItems()) {
        collect_visual_items(child, items);
    }
}

QList<QQuickItem*> visual_items(QQuickItem* item) {
    QList<QQuickItem*> items;
    collect_visual_items(item, items);
    return items;
}

QQuickItem* item_named(QQuickItem* page, const QString& name, const QString& title) {
    for (QQuickItem* item : visual_items(page)) {
        if (item->objectName() == name && item->property("title").toString() == title) {
            return item;
        }
    }
    return nullptr;
}

QPoint center(const QQuickItem* item) {
    return item->mapToScene(item->boundingRect().center()).toPoint();
}

void add_book(const niratan::books::BookStorage& storage, const std::string& folder) {
    niratan::books::BookMetadata book;
    book.id = folder;
    book.folder = folder;
    book.title = folder;
    book.character_count = 1000;
    QVERIFY(storage.save_metadata(book));
    niratan::books::Bookmark bookmark;
    bookmark.progress = 0.2;
    bookmark.character_count = 200;
    QVERIFY(storage.save_bookmark(folder, bookmark));
}

}  // namespace

class LibraryDragTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void library_drag_wires_books_and_custom_shelves();
    void a_book_drags_in_the_main_window_data();
    void a_book_drags_in_the_main_window();
};

void LibraryDragTest::initTestCase() {
    g_previous_handler = qInstallMessageHandler(collect_qml_messages);
}

// ShelfView.reorderBook runs during the drag; the sidebar onMove only sends
// custom shelf offsets and leaves the selected identity alone.
void LibraryDragTest::library_drag_wires_books_and_custom_shelves() {
    using niratan::books::BookShelf;
    using niratan::books::BookStorage;
    using niratan::viewmodels::LibraryViewModel;
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const BookStorage storage(niratan::storage::data_root());
    for (const std::string folder : {"Alpha", "Beta", "Gamma"}) {
        add_book(storage, folder);
    }
    const std::vector<std::string> initial{"Alpha", "Beta", "Gamma"};
    QVERIFY(storage.save_book_order(initial));
    const BookShelf first{"First", {"Alpha", "Beta"}};
    const BookShelf second{"Second", {"Gamma"}};
    QVERIFY(storage.save_shelves({first, second}));

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1100, 680);
    const QUrl url(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/LibraryPage.qml"));
    QQmlComponent component(&engine, url);
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.beginCreate(engine.rootContext()));
    QVERIFY(object);
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page);
    page->setParentItem(window.contentItem());
    page->setWidth(window.width());
    page->setHeight(window.height());
    component.completeCreate();
    auto* model = page->findChild<LibraryViewModel*>();
    QVERIFY(model);
    auto* grid = page->findChild<QQuickItem*>(QStringLiteral("libraryBookGrid"));
    QVERIFY(grid);
    model->setSortOption(1);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const auto card = [page](const QString& title) {
        return item_named(page, QStringLiteral("libraryBookCard"), title);
    };
    const auto row = [page](const QString& title) {
        return item_named(page, QStringLiteral("libraryCustomShelfRow"), title);
    };
    QTRY_VERIFY(card(QStringLiteral("Alpha")) && card(QStringLiteral("Beta")));
    const QPoint alpha = center(card(QStringLiteral("Alpha")));
    QSignalSpy opened(model, &LibraryViewModel::bookOpened);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, alpha);
    QTRY_COMPARE(opened.count(), 1);

    model->setSelectedShelf(QStringLiteral("reading"));
    QVERIFY(QMetaObject::invokeMethod(grid, "forceLayout"));
    QTRY_VERIFY(card(QStringLiteral("Alpha")) && card(QStringLiteral("Beta")));
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier,
                      center(card(QStringLiteral("Alpha"))));
    QTest::mouseMove(&window, center(card(QStringLiteral("Beta"))));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier,
                        center(card(QStringLiteral("Beta"))));
    QCOMPARE(storage.load_book_order(), initial);
    QCOMPARE(model->sort_option(), 1);

    model->setSelectedShelf(QStringLiteral("all"));
    QVERIFY(QMetaObject::invokeMethod(grid, "forceLayout"));
    QTRY_VERIFY(card(QStringLiteral("Alpha")) && card(QStringLiteral("Beta")));
    QSignalSpy moved(model, &LibraryViewModel::booksChanged);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, alpha);
    QTest::mouseMove(&window, alpha + QPoint(20, 0));
    QCOMPARE(model->sort_option(), 1);
    // Just outside the target card, within ShelfView's expanded hit area.
    const auto* beta_card = card(QStringLiteral("Beta"));
    QVERIFY(beta_card);
    const QPoint beta_edge = beta_card->mapToScene(QPointF(-1, beta_card->height() / 2)).toPoint();
    QTest::mouseMove(&window, beta_edge);
    const std::vector<std::string> reordered{"Beta", "Alpha", "Gamma"};
    QTRY_COMPARE(storage.load_book_order(), reordered);
    QTRY_VERIFY(card(QStringLiteral("Alpha")) && card(QStringLiteral("Beta")));
    QVERIFY(card(QStringLiteral("Alpha"))->property("dragSource").toBool());
    QVERIFY(card(QStringLiteral("Beta"))->property("dragTarget").toBool());
    QCOMPARE(model->sort_option(), 2);
    QCOMPARE(moved.count(), 1);
    QTest::mouseMove(&window, center(card(QStringLiteral("Beta"))));
    QCOMPARE(moved.count(), 1);
    const auto* gamma = card(QStringLiteral("Gamma"));
    QVERIFY(gamma);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, center(gamma));
    const std::vector<std::string> released{"Beta", "Gamma", "Alpha"};
    QTRY_COMPARE(storage.load_book_order(), released);
    QTRY_VERIFY(!card(QStringLiteral("Alpha"))->property("dragSource").toBool());
    QVERIFY(!card(QStringLiteral("Beta"))->property("dragTarget").toBool());
    QCOMPARE(opened.count(), 1);

    model->setSelectedShelf(QStringLiteral("shelf:First"));
    QTRY_VERIFY(row(QStringLiteral("First")) && row(QStringLiteral("Second")));
    const QPoint from = center(row(QStringLiteral("First")));
    const auto* last = row(QStringLiteral("Second"));
    const QPoint end = last->mapToScene(QPointF(last->width() / 2, last->height() - 1)).toPoint();
    const auto* smart =
        item_named(page, QStringLiteral("librarySmartShelfRow"), QStringLiteral("All Books"));
    QVERIFY(smart);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, center(smart));
    QTest::mouseMove(&window, end);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, end);
    const std::vector<BookShelf> original_shelves{first, second};
    QCOMPARE(storage.load_shelves(), original_shelves);

    // An open inline editor turns shelf dragging off, and cancelling restores
    // it (LibraryShelfSidebar's .onMove gate).
    auto* column = page->findChild<QObject*>(QStringLiteral("libraryShelfColumn"));
    QVERIFY(column);
    QVERIFY(
        QMetaObject::invokeMethod(column, "beginRename", Q_ARG(QVariant, QStringLiteral("First"))));
    QTRY_VERIFY(!column->property("shelfDraggingEnabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(column, "cancelRename",
                                      Q_ARG(QVariant, QStringLiteral("First"))));
    QTRY_VERIFY(column->property("shelfDraggingEnabled").toBool());

    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(&window, end);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, end);
    const std::vector<BookShelf> shelf_order{second, first};
    QTRY_COMPARE(storage.load_shelves(), shelf_order);
    QCOMPARE(model->selected_shelf(), QStringLiteral("shelf:First"));
    QTest::qWait(50);
    QVERIFY2(g_warnings.isEmpty(), qPrintable(g_warnings.join('\n')));
}

// In the app the page sits in MainWindow; a drag there moves the book in the
// many small steps a real pointer sends. On Wayland a laptop's touchpad
// reports itself as a touchpad rather than a mouse, and drags the same way,
// as a Mac trackpad drags a book.
void LibraryDragTest::a_book_drags_in_the_main_window_data() {
    QTest::addColumn<int>("type");
    QTest::newRow("mouse") << int(QInputDevice::DeviceType::Mouse);
    QTest::newRow("touchpad") << int(QInputDevice::DeviceType::TouchPad);
}

void LibraryDragTest::a_book_drags_in_the_main_window() {
    QFETCH(int, type);
    using niratan::books::BookStorage;
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const BookStorage storage(niratan::storage::data_root());
    for (const std::string folder : {"Alpha", "Beta", "Gamma"}) {
        add_book(storage, folder);
    }
    QVERIFY(storage.save_book_order({"Alpha", "Beta", "Gamma"}));

    QQmlApplicationEngine engine;
    engine.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QQuickItem* content = window->contentItem();
    const auto card = [content](const QString& title) {
        return item_named(content, QStringLiteral("libraryBookCard"), title);
    };
    QTRY_VERIFY(card(QStringLiteral("Alpha")) && card(QStringLiteral("Beta")));

    static QPointingDevice mouse(
        QStringLiteral("test mouse"), 9001, QInputDevice::DeviceType::Mouse,
        QPointingDevice::PointerType::Generic, QInputDevice::Capability::Position, 1, 3);
    static QPointingDevice touchpad(
        QStringLiteral("test touchpad"), 9002, QInputDevice::DeviceType::TouchPad,
        QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position, 1, 3);
    const QPointingDevice* device =
        type == int(QInputDevice::DeviceType::TouchPad) ? &touchpad : &mouse;
    const auto send = [window, device](QEvent::Type event_type, QPointF position,
                                       Qt::MouseButtons buttons) {
        const Qt::MouseButton button =
            event_type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
        QMouseEvent event(event_type, position, position, window->mapToGlobal(position), button,
                          buttons, Qt::NoModifier, device);
        QGuiApplication::sendEvent(window, &event);
    };

    // Across Beta to Gamma in one gesture: the drag lasts until the release.
    const std::vector<std::string> reordered{"Beta", "Gamma", "Alpha"};
    const QPointF from = center(card(QStringLiteral("Alpha")));
    const QPointF to = center(card(QStringLiteral("Gamma")));
    send(QEvent::MouseButtonPress, from, Qt::LeftButton);
    for (int i = 1; i <= 80; ++i) {
        send(QEvent::MouseMove, from + (to - from) * i / 80, Qt::LeftButton);
        QTest::qWait(5);
        if (i > 10) {
            const QQuickItem* alpha = card(QStringLiteral("Alpha"));
            QVERIFY2(alpha && alpha->property("dragSource").toBool(),
                     qPrintable(QStringLiteral("the drag ended at step %1").arg(i)));
        }
    }
    QTRY_COMPARE(storage.load_book_order(), reordered);
    // Still held, back to where Alpha started: the swaps undo.
    for (int i = 1; i <= 80; ++i) {
        send(QEvent::MouseMove, to + (from - to) * i / 80, Qt::LeftButton);
        QTest::qWait(5);
    }
    send(QEvent::MouseButtonRelease, from, Qt::NoButton);
    const std::vector<std::string> restored{"Alpha", "Beta", "Gamma"};
    QTRY_COMPARE(storage.load_book_order(), restored);

    // Alpha onto its neighbour Beta and, still held, straight back.
    const QPointF next = center(card(QStringLiteral("Beta")));
    send(QEvent::MouseButtonPress, from, Qt::LeftButton);
    for (int i = 1; i <= 40; ++i) {
        send(QEvent::MouseMove, from + (next - from) * i / 40, Qt::LeftButton);
        QTest::qWait(5);
    }
    const std::vector<std::string> swapped{"Beta", "Alpha", "Gamma"};
    QTRY_COMPARE(storage.load_book_order(), swapped);
    for (int i = 1; i <= 40; ++i) {
        send(QEvent::MouseMove, next + (from - next) * i / 40, Qt::LeftButton);
        QTest::qWait(5);
    }
    send(QEvent::MouseButtonRelease, from, Qt::NoButton);
    QTRY_COMPARE(storage.load_book_order(), restored);
}

int main(int argc, char* argv[]) {
    niratan::ui::register_popup_url_schemes();
    niratan::ui::register_reader_url_schemes();
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    LibraryDragTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_library_drag.moc"

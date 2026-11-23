// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video library page, following Features/Video/VideoLibraryView.swift:
// `VideoLibraryView.body` and `content` (mode column plus detail),
// `videoToolbarContent`'s local branch, `libraryContent` (empty state, list and
// poster grid), `libraryListRow`, `open`, `shouldHideSingleSectionHeader`,
// `deleteCollection`, `VideoLibrarySidebarView.body`, `sidebarRow`,
// `VideoLibraryContentHeader.body` (including the narrow mode menu),
// `VideoLibrarySortMenu`, `VideoLibraryLayoutPicker`,
// `VideoLibrarySourceToolbarButtons`, `VideoLibraryPosterGridView.body`,
// `VideoLibraryPosterCardView`, `VideoLibraryRowView`,
// `VideoLibraryItemContextMenu` (local branch) and the Error and Delete
// Collection alerts. `VideoLibraryViewModel.sections`, `modeCounts`,
// `emptyTitleKey` and `emptyDescriptionKey` supply the content.
//
// Only load, fitting 960x640, wiring and the search toolbar's top gap (owner
// decision D2) are asserted, never another size, spacing or colour value. The
// data root is a temporary directory with two generated videos, one bound
// subtitle, one saved playback position and one collection, so no test reads
// the developer's library.
//
// The shell test loads MainWindow and follows a card into PlayerPage, because
// `VideoLibraryView.open` hands the URL, subtitle and flag to the shell rather
// than starting playback itself.

#include "../viewmodels/data_root.h"

#include "services/player/playback_history.h"
#include "services/storage/app_paths.h"
#include "services/video/library_catalog.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMetaObject>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

#include <algorithm>
#include <clocale>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>

Q_IMPORT_QML_PLUGIN(NiratanPlugin)
Q_IMPORT_QML_PLUGIN(NiratanStylePlugin)

namespace {

QStringList g_warnings;
QtMessageHandler g_previous_handler = nullptr;

QString error_text(const QQmlComponent& component) {
    if (!component.isError()) {
        return {};
    }
    QStringList lines;
    for (const QQmlError& error : component.errors()) {
        lines.append(error.toString());
    }
    return lines.join('\n');
}

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

QString warnings_after(const QString& label) {
    QTest::qWait(50);
    QString report;
    if (!g_warnings.isEmpty()) {
        report = label + QStringLiteral(":\n") + g_warnings.join('\n');
    }
    g_warnings.clear();
    return report;
}

void collect_visual_texts(QQuickItem* item, QStringList& texts) {
    const QVariant text = item->property("text");
    if (text.isValid() && !text.toString().isEmpty()) {
        texts.append(text.toString());
    }
    for (QQuickItem* child : item->childItems()) {
        collect_visual_texts(child, texts);
    }
}

QStringList visual_texts(QQuickItem* page) {
    QStringList texts;
    collect_visual_texts(page, texts);
    return texts;
}

void collect_named(QQuickItem* item, const QString& name, QList<QQuickItem*>& found) {
    if (item->objectName() == name) {
        found.append(item);
    }
    for (QQuickItem* child : item->childItems()) {
        collect_named(child, name, found);
    }
}

QList<QQuickItem*> items_named(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> found;
    if (root != nullptr) {
        collect_named(root, name, found);
    }
    return found;
}

QQuickItem* item_named(QQuickItem* root, const QString& name) {
    const auto found = items_named(root, name);
    return found.isEmpty() ? nullptr : found.first();
}

bool shown(const QQuickItem* item) {
    return item != nullptr && item->isVisible() && item->opacity() > 0 && item->width() > 0 &&
        item->height() > 0;
}

QQuickItem* shown_item_named(QQuickItem* root, const QString& name) {
    for (QQuickItem* item : items_named(root, name)) {
        if (shown(item)) {
            return item;
        }
    }
    return nullptr;
}

QList<QQuickItem*> shown_items_named_prefix(QQuickItem* root, const QString& prefix) {
    QList<QQuickItem*> found;
    std::function<void(QQuickItem*)> collect = [&](QQuickItem* item) {
        if (item->objectName().startsWith(prefix) && shown(item)) {
            found.append(item);
        }
        for (QQuickItem* child : item->childItems()) {
            collect(child);
        }
    };
    if (root != nullptr) {
        collect(root);
    }
    return found;
}

QQuickItem* shown_control(QQuickItem* item, const char* type, const QString& text) {
    if (item->inherits(type) && item->property("text").toString() == text && shown(item)) {
        return item;
    }
    for (QQuickItem* child : item->childItems()) {
        if (QQuickItem* found = shown_control(child, type, text)) {
            return found;
        }
    }
    return nullptr;
}

// Mirrors the load test's viewport rule: a row may scroll vertically but never
// leaves the page or its clipping parents sideways.
bool fits_viewport(const QQuickItem& item, const QRectF& window_bounds) {
    auto visible_bounds = item.mapRectToScene(item.boundingRect());
    if (visible_bounds.isEmpty()) {
        return false;
    }

    QList<QRectF> clips{window_bounds};
    for (auto* parent = item.parentItem(); parent; parent = parent->parentItem()) {
        if (!parent->clip()) {
            continue;
        }
        const auto viewport = parent->mapRectToScene(parent->boundingRect());
        if (parent->inherits("QQuickFlickable")) {
            visible_bounds.setTop(std::max(visible_bounds.top(), viewport.top()));
            visible_bounds.setBottom(std::min(visible_bounds.bottom(), viewport.bottom()));
        }
        clips.append(viewport);
    }
    const auto fits_horizontally = [&](const QRectF& clip) {
        return visible_bounds.left() >= clip.left() && visible_bounds.right() <= clip.right();
    };
    if (!std::ranges::all_of(clips, fits_horizontally)) {
        return false;
    }
    if (visible_bounds.height() <= 0) {
        return true;
    }
    return std::ranges::all_of(clips,
                               [&](const QRectF& clip) { return clip.contains(visible_bounds); });
}

QString touch_file(const QString& path, int size) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(QByteArray(size, 'x'));
    file.close();
    return QFileInfo(path).absoluteFilePath();
}

struct Fixture {
    QString source;
    QString alpha;
    QString beta;
    QString subtitle;
    QString collection_id;
};

Fixture make_fixture(const QString& base) {
    Fixture fixture;
    fixture.source = base + QStringLiteral("/Anime");
    QDir().mkpath(fixture.source);
    fixture.alpha = touch_file(fixture.source + QStringLiteral("/Alpha.mp4"), 10);
    fixture.beta = touch_file(fixture.source + QStringLiteral("/Beta.mkv"), 30);
    fixture.subtitle = touch_file(fixture.source + QStringLiteral("/Alpha.ja.srt"), 4);

    const auto root = niratan::storage::data_root();
    niratan::video::LibraryStore store(root / "video_library.json");
    const auto source = store.add_source(std::filesystem::path(fixture.source.toStdString()));
    store.scan_source(source.id);
    store.bind_subtitle(fixture.alpha, fixture.subtitle);
    const auto collection = store.create_collection(QStringLiteral("Arc"), {fixture.beta});
    fixture.collection_id = collection.id.toString(QUuid::WithoutBraces).toUpper();

    niratan::player::PlaybackHistory history(root / "video_playback_history.json");
    history.save(fixture.alpha, 20, 100);
    return fixture;
}

void click(QQuickItem* item) {
    QVERIFY(item != nullptr);
    QVERIFY(QMetaObject::invokeMethod(item, "clicked"));
}

void trigger_menu_item(QQuickItem* menu_item) {
    QVERIFY(menu_item != nullptr);
    QVERIFY(QMetaObject::invokeMethod(menu_item, "triggered"));
}

QQuickItem* row_with_id(QQuickItem* page, const QString& id) {
    for (QQuickItem* row : items_named(page, QStringLiteral("videoRow"))) {
        if (row->property("rowID").toString() == id) {
            return row;
        }
    }
    return nullptr;
}

QQuickItem* card_with_id(QQuickItem* page, const QString& id) {
    for (QQuickItem* card : items_named(page, QStringLiteral("videoCard"))) {
        if (card->property("rowID").toString() == id) {
            return card;
        }
    }
    return nullptr;
}

QObject* find_model(QQuickItem* page) {
    return page->findChild<QObject*>(QStringLiteral("videoLibraryModel"));
}

}  // namespace

class VideoLibraryViewTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void page_loads_with_the_mode_column_and_toolbar();
    void empty_library_shows_the_add_folder_message();
    void sidebar_selects_the_mode_and_the_empty_copy_follows();
    void search_sort_layout_and_add_write_the_model();
    void clicking_a_row_opens_it();
    void opening_a_vanished_video_shows_the_error_and_emits_nothing();
    void play_from_beginning_clears_progress_and_sets_the_flag();
    void context_menu_marks_watched_and_clears_progress();
    void narrow_page_moves_the_modes_into_the_title_menu();
    void poster_grid_opens_a_card_and_cover_size_follows_the_layout();
    void large_library_creates_only_viewport_delegates();
    void selection_keeps_the_visible_delegates();
    void context_menu_is_created_on_demand_for_rows_and_cards();
    void list_row_shows_favorite_metadata_and_progress();
    void folder_headers_collapse_and_a_single_section_hides_its_header();
    void refresh_finds_missing_files_and_the_missing_row_appears();
    void delete_collection_asks_then_removes_only_the_collection();
    void stored_layout_mode_survives_a_new_page();
    void search_toolbar_keeps_a_top_gap_and_shared_centre();
    void collapsed_sidebar_keeps_the_search_toolbar_clear();
    void default_window_fits_every_visible_control();
    void main_window_pushes_the_player_with_the_video();

   private:
    std::unique_ptr<QObject> create_page(QQuickWindow& window, const QSize& size, QString& error);

    QQmlApplicationEngine engine_;
};

void VideoLibraryViewTest::initTestCase() {
    g_previous_handler = qInstallMessageHandler(collect_qml_messages);
}

std::unique_ptr<QObject> VideoLibraryViewTest::create_page(QQuickWindow& window, const QSize& size,
                                                           QString& error) {
    window.resize(size);
    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) {
        error = QStringLiteral("the test window was not exposed");
        return {};
    }
    window.requestActivate();
    if (!QTest::qWaitForWindowActive(&window)) {
        error = QStringLiteral("the test window was not activated");
        return {};
    }

    const QUrl url(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/VideoLibraryPage.qml"));
    QQmlComponent component(&engine_, url);
    if (component.isError()) {
        error = QStringLiteral("VideoLibraryPage: ") + error_text(component);
        return {};
    }

    std::unique_ptr<QObject> object(component.beginCreate(engine_.rootContext()));
    if (!object) {
        error = QStringLiteral("VideoLibraryPage: ") + error_text(component);
        return {};
    }

    auto* item = qobject_cast<QQuickItem*>(object.get());
    if (item == nullptr) {
        error = QStringLiteral("VideoLibraryPage is not an Item");
        return {};
    }
    item->setParentItem(window.contentItem());
    item->setWidth(size.width());
    item->setHeight(size.height());
    component.completeCreate();
    return object;
}

// B2, B3, B4: the mode column, its sections, the content header and the
// toolbar are all present at the default window.
void VideoLibraryViewTest::page_loads_with_the_mode_column_and_toolbar() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);

    const QString warnings = warnings_after(QStringLiteral("VideoLibraryPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));

    QQuickItem* sidebar = shown_item_named(page, QStringLiteral("librarySidebar"));
    QVERIFY2(sidebar != nullptr, "the mode column is hidden at 960x640");

    const QStringList texts = visual_texts(page);
    const QStringList required{
        QStringLiteral("Library"),
        QStringLiteral("Organization"),
        QStringLiteral("Continue Watching"),
        QStringLiteral("Recent"),
        QStringLiteral("All Videos"),
        QStringLiteral("Unwatched"),
        QStringLiteral("Finished"),
        QStringLiteral("Favorites"),
        QStringLiteral("Series"),
        QStringLiteral("Folders"),
        QStringLiteral("Collections"),
        QStringLiteral("Needs Review"),
        QStringLiteral("Alpha"),
        QStringLiteral("1:20 left"),
    };
    for (const QString& text : required) {
        QVERIFY2(texts.contains(text), qPrintable(QStringLiteral("missing text: ") + text));
    }

    auto* model = find_model(page);
    QVERIFY(model != nullptr);

    // The mode order is Swift's libraryModes then organizationModes; the
    // fixture has no missing file, so Missing is absent.
    QList<QQuickItem*> mode_rows = shown_items_named_prefix(page, QStringLiteral("modeRow-"));
    std::sort(mode_rows.begin(), mode_rows.end(), [](QQuickItem* left, QQuickItem* right) {
        return left->mapToScene(QPointF()).y() < right->mapToScene(QPointF()).y();
    });
    QStringList modes;
    for (QQuickItem* row : mode_rows) {
        modes.append(row->property("mode").toString());
    }
    QCOMPARE(modes,
             QStringList({QStringLiteral("continueWatching"), QStringLiteral("recent"),
                          QStringLiteral("all"), QStringLiteral("unwatched"),
                          QStringLiteral("finished"), QStringLiteral("favorites"),
                          QStringLiteral("series"), QStringLiteral("folders"),
                          QStringLiteral("collections"), QStringLiteral("needsReview")}));

    QQuickItem* all_badge = shown_item_named(page, QStringLiteral("modeBadge-all"));
    QVERIFY2(all_badge != nullptr, "the All Videos count badge is missing");
    QCOMPARE(all_badge->property("text").toString(), QStringLiteral("2"));
    QVERIFY2(shown_item_named(page, QStringLiteral("modeBadge-favorites")) == nullptr,
             "a zero count badge is drawn");

    model->setProperty("displayMode", QStringLiteral("all"));
    QQuickItem* header_count = item_named(page, QStringLiteral("headerCount"));
    QVERIFY2(header_count != nullptr, "the content header has no count");
    QTRY_COMPARE(header_count->property("text").toString(), QStringLiteral("2"));

    QVERIFY2(shown_item_named(page, QStringLiteral("scanningOverlay")) == nullptr,
             "the scanning overlay shows while idle");
    QQuickItem* overlay = item_named(page, QStringLiteral("scanningOverlay"));
    QVERIFY2(overlay != nullptr, "the scanning overlay is missing");
    QVERIFY2(visual_texts(overlay).contains(QStringLiteral("Scanning Video Folders...")),
             "the scanning overlay has no text");

    auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY2(field != nullptr, "the toolbar has no search field");
    QCOMPARE(field->property("placeholderText").toString(), QStringLiteral("Search Videos"));
    QVERIFY2(shown_item_named(page, QStringLiteral("sortButton")) != nullptr,
             "the toolbar has no sort button");
    QVERIFY2(shown_item_named(page, QStringLiteral("refreshButton")) != nullptr,
             "the toolbar has no refresh button");
    QVERIFY2(shown_item_named(page, QStringLiteral("addButton")) != nullptr,
             "the toolbar has no add button");
    QVERIFY2(shown_item_named(page, QStringLiteral("coverSizeButton")) != nullptr,
             "the poster cover-size control is missing");
    QVERIFY(!fixture.alpha.isEmpty());
}

// B8: no sources draws the film-stack empty state with the add-folder action.
void VideoLibraryViewTest::empty_library_shows_the_add_folder_message() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);

    const QStringList texts = visual_texts(page);
    QVERIFY2(texts.contains(QStringLiteral("No Video Folders")),
             qPrintable(texts.join(QStringLiteral(" | "))));
    QVERIFY2(texts.contains(QStringLiteral("Add a local folder to build your video bookshelf.")),
             "the empty description is missing");
    QVERIFY2(texts.contains(QStringLiteral("Add Video Folder")),
             "the add-folder button is missing");
    QVERIFY2(!shown_item_named(page, QStringLiteral("videoList")),
             "a list is drawn without sources");
    QVERIFY2(!shown_item_named(page, QStringLiteral("videoGrid")),
             "a grid is drawn without sources");
}

// B2: a mode row selects the mode; B8: the empty copy follows it.
void VideoLibraryViewTest::sidebar_selects_the_mode_and_the_empty_copy_follows() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    QQuickItem* favorites = shown_item_named(page, QStringLiteral("modeRow-favorites"));
    QVERIFY2(favorites != nullptr, "the Favorites mode row is missing");
    click(favorites);
    QTRY_COMPARE(model->property("displayMode").toString(), QStringLiteral("favorites"));
    QTRY_VERIFY(visual_texts(page).contains(QStringLiteral("No Favorite Videos")));
    QTRY_VERIFY(visual_texts(page).contains(QStringLiteral("Favorite videos will appear here.")));
}

// B4: search writes searchText; the sort menu writes sortOption; the layout
// picker writes layoutMode; the Add menu's folder item opens the picker and
// its accepted paths reach addFolders.
void VideoLibraryViewTest::search_sort_layout_and_add_write_the_model() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    auto* field = page->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY(field != nullptr);
    field->setProperty("text", QStringLiteral("alp"));
    QTRY_COMPARE(model->property("searchText").toString(), QStringLiteral("alp"));

    QQuickItem* sort_button = shown_item_named(page, QStringLiteral("sortButton"));
    QVERIFY(sort_button != nullptr);
    click(sort_button);
    for (const QString& option :
         QStringList{QStringLiteral("Recent Playback"), QStringLiteral("File Name"),
                     QStringLiteral("Modified Date"), QStringLiteral("Folder")}) {
        QTRY_VERIFY2(shown_control(window.contentItem(), "QQuickMenuItem", option) != nullptr,
                     qPrintable(QStringLiteral("the Sort menu has no ") + option));
    }
    QQuickItem* sort_item = nullptr;
    QTRY_VERIFY((sort_item = shown_control(window.contentItem(), "QQuickMenuItem",
                                           QStringLiteral("Modified Date"))) != nullptr);
    trigger_menu_item(sort_item);
    QTRY_COMPARE(model->property("sortOption").toString(), QStringLiteral("modifiedDate"));

    QQuickItem* list_button = shown_item_named(page, QStringLiteral("layoutListButton"));
    QVERIFY2(list_button != nullptr, "the List layout button is missing");
    click(list_button);
    QTRY_COMPARE(model->property("layoutMode").toString(), QStringLiteral("list"));
    QTRY_VERIFY(!shown_item_named(page, QStringLiteral("coverSizeButton")));

    QQuickItem* posters_button = shown_item_named(page, QStringLiteral("layoutPostersButton"));
    QVERIFY2(posters_button != nullptr, "the Posters layout button is missing");
    click(posters_button);
    QTRY_COMPARE(model->property("layoutMode").toString(), QStringLiteral("posters"));
    QQuickItem* cover_size = nullptr;
    QTRY_VERIFY((cover_size = shown_item_named(page, QStringLiteral("coverSizeButton"))) !=
                nullptr);
    click(cover_size);
    QQuickItem* slider = nullptr;
    QTRY_VERIFY((slider = shown_item_named(page, QStringLiteral("coverSizeSlider"))) != nullptr);
    slider->setProperty("value", 200.0);
    QVERIFY(QMetaObject::invokeMethod(slider, "moved"));
    QTRY_COMPARE(model->property("posterSize").toDouble(), 200.0);

    QQuickItem* add_button = shown_item_named(page, QStringLiteral("addButton"));
    QVERIFY(add_button != nullptr);
    click(add_button);
    QQuickItem* add_item = nullptr;
    QTRY_VERIFY((add_item = shown_control(window.contentItem(), "QQuickMenuItem",
                                          QStringLiteral("Add Video Folder"))) != nullptr);
    trigger_menu_item(add_item);

    auto* dialog = page->findChild<QObject*>(QStringLiteral("folderDialog"));
    QVERIFY2(dialog != nullptr, "the folder picker is missing");
    QTemporaryDir extra;
    QVERIFY(extra.isValid());
    QDir().mkpath(extra.path() + QStringLiteral("/Second"));
    dialog->setProperty("selectedFolder",
                        QUrl::fromLocalFile(extra.path() + QStringLiteral("/Second")));
    QVERIFY(QMetaObject::invokeMethod(dialog, "accepted"));
    QTRY_COMPARE(model->property("sourceSummaries").toList().size(), 2);
}

// B4: Missing contains stale catalog entries until Refresh rescans.
void VideoLibraryViewTest::refresh_finds_missing_files_and_the_missing_row_appears() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());
    QVERIFY(QFile::remove(fixture.alpha));

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    QTRY_COMPARE(model->property("modeCounts").toMap().value("missing").toInt(), 1);
    QTRY_VERIFY(shown_item_named(page, QStringLiteral("modeRow-missing")) != nullptr);

    QQuickItem* refresh = shown_item_named(page, QStringLiteral("refreshButton"));
    QVERIFY(refresh != nullptr);
    click(refresh);
    QTRY_COMPARE(model->property("modeCounts").toMap().value("missing").toInt(), 0);
    QTRY_COMPARE(model->property("modeCounts").toMap().value("all").toInt(), 1);
    QTRY_VERIFY(row_with_id(page, fixture.alpha) == nullptr);
    QTRY_VERIFY(shown_item_named(page, QStringLiteral("modeRow-missing")) == nullptr);
}

// B1: a row selects the item and emits its path, bound subtitle and false.
void VideoLibraryViewTest::clicking_a_row_opens_it() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    QQuickItem* list_button = shown_item_named(page, QStringLiteral("layoutListButton"));
    QVERIFY(list_button != nullptr);
    click(list_button);

    QQuickItem* row = nullptr;
    QTRY_VERIFY((row = row_with_id(page, fixture.alpha)) != nullptr);
    QSignalSpy spy(page, SIGNAL(videoOpened(QUrl, QUrl, bool)));
    click(row);

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUrl(), QUrl::fromLocalFile(fixture.alpha));
    QCOMPARE(spy.at(0).at(1).toUrl(), QUrl::fromLocalFile(fixture.subtitle));
    QCOMPARE(spy.at(0).at(2).toBool(), false);
    QTRY_COMPARE(model->property("selectedItemID").toString(), fixture.alpha);
}

// B1, B9: a row whose file disappeared shows the view model's error, emits
// nothing, and ACK clears the message.
void VideoLibraryViewTest::opening_a_vanished_video_shows_the_error_and_emits_nothing() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    click(shown_item_named(page, QStringLiteral("layoutListButton")));
    QQuickItem* row = nullptr;
    QTRY_VERIFY((row = row_with_id(page, fixture.alpha)) != nullptr);
    QVERIFY(QFile::remove(fixture.alpha));

    QSignalSpy spy(page, SIGNAL(videoOpened(QUrl, QUrl, bool)));
    click(row);
    QTest::qWait(100);
    QCOMPARE(spy.count(), 0);

    auto* dialog = page->findChild<QObject*>(QStringLiteral("errorDialog"));
    QVERIFY2(dialog != nullptr, "the error alert is missing");
    QTRY_VERIFY(dialog->property("visible").toBool());
    QCOMPARE(dialog->property("title").toString(), QStringLiteral("Error"));
    QVERIFY2(
        visual_texts(page).contains(QStringLiteral("The saved video file is no longer available.")),
        "the error message is missing");

    QVERIFY(QMetaObject::invokeMethod(dialog, "accept"));
    QTRY_VERIFY(!dialog->property("visible").toBool());
    QTRY_COMPARE(model->property("errorMessage").toString(), QString());
    QTRY_VERIFY(row_with_id(page, fixture.alpha) == nullptr);
}

// B1, B7: Play from Beginning clears the stored progress before emitting the
// row with the flag set.
void VideoLibraryViewTest::play_from_beginning_clears_progress_and_sets_the_flag() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    click(shown_item_named(page, QStringLiteral("layoutListButton")));
    QQuickItem* row = nullptr;
    QTRY_VERIFY((row = row_with_id(page, fixture.alpha)) != nullptr);

    const QPoint center = row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint();
    QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, center);
    QQuickItem* item = nullptr;
    QTRY_VERIFY((item = shown_control(window.contentItem(), "QQuickMenuItem",
                                      QStringLiteral("Play from Beginning"))) != nullptr);
    QVERIFY2(shown_control(window.contentItem(), "QQuickMenuItem", QStringLiteral("Details")) ==
                 nullptr,
             "the context menu still offers Details");
    QVERIFY2(shown_control(window.contentItem(), "QQuickMenuItem",
                           QStringLiteral("Reveal in File Manager")) != nullptr,
             "the context menu has no Reveal in File Manager");

    QSignalSpy spy(page, SIGNAL(videoOpened(QUrl, QUrl, bool)));
    trigger_menu_item(item);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUrl(), QUrl::fromLocalFile(fixture.alpha));
    QCOMPARE(spy.at(0).at(2).toBool(), true);

    QTRY_VERIFY(row_with_id(page, fixture.alpha) == nullptr);
    QVERIFY(model->setProperty("displayMode", QStringLiteral("all")));
    QQuickItem* reloaded = nullptr;
    QTRY_VERIFY((reloaded = row_with_id(page, fixture.alpha)) != nullptr);
    QVERIFY2(!reloaded->property("rowData").toMap().value("playback").isValid(),
             "Play from Beginning kept the stored progress");
}

// B7: Mark as Watched and Clear Progress reach the view model; Clear Progress
// is disabled while the item has no playback state.
void VideoLibraryViewTest::context_menu_marks_watched_and_clears_progress() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    click(shown_item_named(page, QStringLiteral("layoutListButton")));
    QQuickItem* row = nullptr;
    QTRY_VERIFY((row = row_with_id(page, fixture.alpha)) != nullptr);

    const auto open_menu = [&](QQuickItem* target) {
        const QPoint center =
            target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint();
        QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, center);
    };

    open_menu(row);
    QQuickItem* mark = nullptr;
    QTRY_VERIFY((mark = shown_control(window.contentItem(), "QQuickMenuItem",
                                      QStringLiteral("Mark as Watched"))) != nullptr);
    trigger_menu_item(mark);
    QTRY_VERIFY(row_with_id(page, fixture.alpha) == nullptr);
    QVERIFY(model->setProperty("displayMode", QStringLiteral("finished")));
    QTRY_VERIFY((row = row_with_id(page, fixture.alpha)) != nullptr);
    QVERIFY(row->property("rowData").toMap().value("playback").toMap().value("finished").toBool());

    open_menu(row);
    QQuickItem* clear = nullptr;
    QTRY_VERIFY((clear = shown_control(window.contentItem(), "QQuickMenuItem",
                                       QStringLiteral("Clear Progress"))) != nullptr);
    QVERIFY2(clear->property("enabled").toBool(), "Clear Progress is disabled with a state");
    trigger_menu_item(clear);
    QTRY_VERIFY(row_with_id(page, fixture.alpha) == nullptr);
    QVERIFY(model->setProperty("displayMode", QStringLiteral("all")));
    QTRY_VERIFY((row = row_with_id(page, fixture.alpha)) != nullptr);
    QVERIFY(!row->property("rowData").toMap().value("playback").isValid());

    QQuickItem* beta = nullptr;
    QTRY_VERIFY((beta = row_with_id(page, fixture.beta)) != nullptr);
    open_menu(beta);
    QQuickItem* disabled_clear = nullptr;
    QTRY_VERIFY((disabled_clear = shown_control(window.contentItem(), "QQuickMenuItem",
                                                QStringLiteral("Clear Progress"))) != nullptr);
    QVERIFY2(!disabled_clear->property("enabled").toBool(),
             "Clear Progress is enabled without a playback state");
    trigger_menu_item(disabled_clear);
    QTRY_VERIFY((beta = row_with_id(page, fixture.beta)) != nullptr);
    QVERIFY(!beta->property("rowData").toMap().value("playback").isValid());
}

// B3: below 760 the mode column is hidden and the title becomes a menu holding
// both groups.
void VideoLibraryViewTest::narrow_page_moves_the_modes_into_the_title_menu() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(700, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    QVERIFY2(shown_item_named(page, QStringLiteral("librarySidebar")) == nullptr,
             "the mode column stays at 700 wide");
    QQuickItem* mode_menu = shown_item_named(page, QStringLiteral("modeMenu"));
    QVERIFY2(mode_menu != nullptr, "the title is not a mode menu when narrow");
    click(mode_menu);

    for (const QString& title :
         QStringList{QStringLiteral("Continue Watching"), QStringLiteral("Recent"),
                     QStringLiteral("All Videos"), QStringLiteral("Unwatched"),
                     QStringLiteral("Finished"), QStringLiteral("Favorites"),
                     QStringLiteral("Missing"), QStringLiteral("Series"), QStringLiteral("Folders"),
                     QStringLiteral("Collections"), QStringLiteral("Needs Review")}) {
        QTRY_VERIFY2(shown_control(window.contentItem(), "QQuickMenuItem", title) != nullptr,
                     qPrintable(QStringLiteral("the mode menu has no ") + title));
    }
    QQuickItem* item = nullptr;
    QTRY_VERIFY((item = shown_control(window.contentItem(), "QQuickMenuItem",
                                      QStringLiteral("Favorites"))) != nullptr);
    trigger_menu_item(item);
    QTRY_COMPARE(model->property("displayMode").toString(), QStringLiteral("favorites"));
}

// B5, B6: the poster grid shows the same rows; a card opens its item and the
// cover-size control follows the layout.
void VideoLibraryViewTest::poster_grid_opens_a_card_and_cover_size_follows_the_layout() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());
    {
        niratan::video::LibraryStore store(niratan::storage::data_root() / "video_library.json");
        store.set_favorite(fixture.alpha, true);
    }

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);
    QTRY_COMPARE(model->property("layoutMode").toString(), QStringLiteral("posters"));

    QQuickItem* card = nullptr;
    QTRY_VERIFY((card = card_with_id(page, fixture.alpha)) != nullptr);
    QVERIFY2(shown_item_named(card, QStringLiteral("favoriteStar")) != nullptr,
             "the favorite star is missing");
    QQuickItem* progress = item_named(card, QStringLiteral("progressTrack"));
    QVERIFY2(progress != nullptr, "the progress track is missing");
    QVERIFY2(std::abs(progress->property("progress").toDouble() - 0.2) < 0.001,
             "the progress track does not follow the playback state");
    const QStringList card_texts = visual_texts(card);
    QVERIFY2(card_texts.contains(QStringLiteral("1:20 left")), "the remaining time is missing");
    QVERIFY2(card_texts.contains(QStringLiteral("Anime")), "the parent folder is missing");
    QVERIFY2(
        std::ranges::any_of(
            card_texts, [](const QString& text) { return text.startsWith(QStringLiteral("10")); }),
        "the file size is missing");
    QVERIFY2(card_texts.contains(QStringLiteral("Alpha")), "the card has no display title");

    QSignalSpy spy(page, SIGNAL(videoOpened(QUrl, QUrl, bool)));
    click(card);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUrl(), QUrl::fromLocalFile(fixture.alpha));
    QCOMPARE(spy.at(0).at(2).toBool(), false);
    QTRY_VERIFY(card_with_id(page, fixture.alpha)->property("selected").toBool());
}

void VideoLibraryViewTest::large_library_creates_only_viewport_delegates() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString folder = root.path() + QStringLiteral("/Anime");
    QString first;
    QString last;
    for (int i = 1; i <= 320; ++i) {
        last = touch_file(folder + QStringLiteral("/Episode %1.mkv").arg(i, 3, 10, u'0'), 1);
        QVERIFY(!last.isEmpty());
        if (i == 1) {
            first = last;
        }
    }
    niratan::video::LibraryStore store(niratan::storage::data_root() / "video_library.json");
    const auto source = store.add_source(std::filesystem::path(folder.toStdString()));
    QVERIFY(store.scan_source(source.id));
    QCOMPARE(store.catalog().items.size(), 320);

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    auto* model = find_model(page);
    QVERIFY(model != nullptr);
    QCOMPARE(model->property("displayMode").toString(), QStringLiteral("all"));

    const auto check_layout = [page, &first, &last](const QString& view_name,
                                                    const QString& delegate_name, auto find) {
        QTRY_VERIFY(find(page, first) != nullptr);
        QVERIFY(items_named(page, delegate_name).size() < 60);
        QVERIFY(find(page, last) == nullptr);

        auto* view = item_named(page, view_name);
        QVERIFY(view != nullptr);
        QVERIFY(QMetaObject::invokeMethod(view, "positionViewAtEnd"));
        QTRY_VERIFY(find(page, last) != nullptr);
        QVERIFY(items_named(page, delegate_name).size() < 60);
        if (auto* first_item = find(page, first)) {
            const QRectF viewport = view->mapRectToScene(view->boundingRect());
            QVERIFY(!viewport.intersects(first_item->mapRectToScene(first_item->boundingRect())));
        }
        QVERIFY(QMetaObject::invokeMethod(view, "positionViewAtBeginning"));
        QTRY_VERIFY(find(page, first) != nullptr);
    };
    check_layout(QStringLiteral("videoLibraryPosterList"), QStringLiteral("videoCard"),
                 card_with_id);
    QVERIFY(model->setProperty("layoutMode", QStringLiteral("list")));
    check_layout(QStringLiteral("videoLibraryList"), QStringLiteral("videoRow"), row_with_id);
    const QString warnings = warnings_after(QStringLiteral("large video library"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

void VideoLibraryViewTest::selection_keeps_the_visible_delegates() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    auto* model = find_model(page);
    QVERIFY(model != nullptr);

    QTRY_VERIFY(card_with_id(page, fixture.alpha) != nullptr);
    QPointer<QQuickItem> card = card_with_id(page, fixture.alpha);
    QVERIFY(!card->property("selected").toBool());
    QVERIFY(QMetaObject::invokeMethod(model, "select", Q_ARG(QString, fixture.alpha)));
    QVERIFY(card);
    QCOMPARE(card_with_id(page, fixture.alpha), card.data());
    QVERIFY(card->property("selected").toBool());
}

void VideoLibraryViewTest::context_menu_is_created_on_demand_for_rows_and_cards() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    auto* model = find_model(page);
    QVERIFY(model != nullptr);
    QVERIFY(model->setProperty("displayMode", QStringLiteral("all")));

    const QString menu_name = QStringLiteral("videoContextMenu");
    const auto right_click = [&window](QQuickItem* item) {
        const QPoint center =
            item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
        QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, center);
    };
    QTRY_VERIFY(card_with_id(page, fixture.alpha) != nullptr);
    QVERIFY(page->findChildren<QObject*>(menu_name).isEmpty());
    right_click(card_with_id(page, fixture.alpha));

    QObject* menu = nullptr;
    QTRY_VERIFY((menu = page->findChild<QObject*>(menu_name)) != nullptr);
    QTRY_VERIFY(menu->property("visible").toBool());
    auto* clear =
        shown_control(window.contentItem(), "QQuickMenuItem", QStringLiteral("Clear Progress"));
    QVERIFY(clear != nullptr);
    QVERIFY(clear->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(menu, "close"));
    QTRY_VERIFY(!menu->property("visible").toBool());

    QVERIFY(model->setProperty("layoutMode", QStringLiteral("list")));
    QTRY_VERIFY(row_with_id(page, fixture.beta) != nullptr);
    right_click(row_with_id(page, fixture.beta));
    QTRY_VERIFY(menu->property("visible").toBool());
    QCOMPARE(page->findChildren<QObject*>(menu_name), QList<QObject*>{menu});
    clear = shown_control(window.contentItem(), "QQuickMenuItem", QStringLiteral("Clear Progress"));
    QVERIFY(clear != nullptr);
    QVERIFY(!clear->property("enabled").toBool());
}

// B5: the list row draws the favorite star, the metadata line, the playback
// text and the progress track, and follows a watch change.
void VideoLibraryViewTest::list_row_shows_favorite_metadata_and_progress() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());
    {
        niratan::video::LibraryStore store(niratan::storage::data_root() / "video_library.json");
        store.set_favorite(fixture.alpha, true);
    }

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    click(shown_item_named(page, QStringLiteral("layoutListButton")));
    QQuickItem* row = nullptr;
    QTRY_VERIFY((row = row_with_id(page, fixture.alpha)) != nullptr);
    QVERIFY2(shown_item_named(row, QStringLiteral("favoriteStar")) != nullptr,
             "the favorite star is missing");
    const QStringList row_texts = visual_texts(row);
    QVERIFY2(row_texts.contains(QStringLiteral("Anime")), "the source name is missing");
    QVERIFY2(row_texts.contains(QStringLiteral("\u00b7")), "the metadata separator is missing");
    QVERIFY2(
        std::ranges::any_of(
            row_texts, [](const QString& text) { return text.startsWith(QStringLiteral("10")); }),
        "the file size is missing");

    QQuickItem* progress = item_named(row, QStringLiteral("progressTrack"));
    QVERIFY2(progress != nullptr, "the progress track is missing");
    QVERIFY2(std::abs(progress->property("progress").toDouble() - 0.2) < 0.001,
             "the progress track does not follow the playback state");

    QVERIFY(QMetaObject::invokeMethod(model, "markWatched", Q_ARG(QString, fixture.alpha)));
    QTRY_VERIFY(row_with_id(page, fixture.alpha) == nullptr);
    QVERIFY(model->setProperty("displayMode", QStringLiteral("finished")));
    QTRY_VERIFY((row = row_with_id(page, fixture.alpha)) != nullptr);
    QVERIFY(visual_texts(row).contains(QStringLiteral("Watched")));
}

// B5: a generated single section hides its header, a folder section shows one
// and collapses its rows.
void VideoLibraryViewTest::folder_headers_collapse_and_a_single_section_hides_its_header() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    click(shown_item_named(page, QStringLiteral("layoutListButton")));
    QTRY_VERIFY(row_with_id(page, fixture.alpha) != nullptr);
    QVERIFY2(shown_item_named(page, QStringLiteral("sectionHeader")) == nullptr,
             "the single Continue Watching section draws a header");

    model->setProperty("displayMode", QStringLiteral("folders"));
    QQuickItem* header = nullptr;
    QTRY_VERIFY((header = shown_item_named(page, QStringLiteral("sectionHeader"))) != nullptr);
    const QStringList header_texts = visual_texts(header);
    QVERIFY2(header_texts.contains(QStringLiteral("Anime")), "the folder title is missing");
    QVERIFY2(header_texts.contains(QStringLiteral("2")), "the folder count is missing");

    click(header);
    QTRY_VERIFY(row_with_id(page, fixture.alpha) == nullptr);
    header = shown_item_named(page, QStringLiteral("sectionHeader"));
    QVERIFY(header != nullptr);
    click(header);
    QTRY_VERIFY(row_with_id(page, fixture.alpha) != nullptr);
}

// B10: the section menu asks, then removes the collection while its videos
// stay in the library.
void VideoLibraryViewTest::delete_collection_asks_then_removes_only_the_collection() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);

    QQuickItem* collections = shown_item_named(page, QStringLiteral("modeRow-collections"));
    QVERIFY(collections != nullptr);
    click(collections);
    QTRY_VERIFY(visual_texts(page).contains(QStringLiteral("Arc")));

    QQuickItem* actions = nullptr;
    QTRY_VERIFY((actions = shown_item_named(page, QStringLiteral("collectionActionsButton"))) !=
                nullptr);
    click(actions);
    QQuickItem* delete_item = nullptr;
    QTRY_VERIFY((delete_item = shown_control(window.contentItem(), "QQuickMenuItem",
                                             QStringLiteral("Delete Collection"))) != nullptr);
    trigger_menu_item(delete_item);

    auto* dialog = page->findChild<QObject*>(QStringLiteral("deleteCollectionDialog"));
    QVERIFY2(dialog != nullptr, "the Delete Collection confirmation is missing");
    QTRY_VERIFY(dialog->property("visible").toBool());
    QCOMPARE(dialog->property("title").toString(), QStringLiteral("Delete Collection?"));
    QVERIFY2(visual_texts(page).contains(QStringLiteral(
                 "This removes the collection but keeps its videos in your library.")),
             "the confirmation message is missing");
    QQuickItem* confirm = shown_item_named(page, QStringLiteral("confirmDeleteCollection"));
    QVERIFY2(confirm != nullptr, "the confirmation has no destructive button");
    QCOMPARE(confirm->property("text").toString(), QStringLiteral("Delete Collection"));
    QVERIFY2(shown_control(window.contentItem(), "QQuickButton", QStringLiteral("Cancel")) !=
                 nullptr,
             "the confirmation has no Cancel button");
    click(confirm);

    QTRY_COMPARE(model->property("modeCounts").toMap().value("collections").toInt(), 0);
    QVERIFY2(model->property("sections").toList().isEmpty(),
             "the collection section survives its deletion");

    model->setProperty("displayMode", QStringLiteral("all"));
    QTRY_VERIFY(card_with_id(page, fixture.beta) != nullptr);
}

// B11: a new page keeps the layout mode the previous page stored.
void VideoLibraryViewTest::stored_layout_mode_survives_a_new_page() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    make_fixture(root.path());

    QQuickWindow window;
    QString error;
    {
        std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
        QVERIFY2(object != nullptr, qPrintable(error));
        auto* page = qobject_cast<QQuickItem*>(object.get());
        QVERIFY(page != nullptr);
        QObject* model = find_model(page);
        QVERIFY(model != nullptr);
        QCOMPARE(model->property("posterSize").toDouble(), 160.0);
        click(shown_item_named(page, QStringLiteral("layoutListButton")));
        QTRY_COMPARE(model->property("layoutMode").toString(), QStringLiteral("list"));
        model->setProperty("posterSize", 200.0);
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QObject* model = find_model(page);
    QVERIFY(model != nullptr);
    QCOMPARE(model->property("layoutMode").toString(), QStringLiteral("list"));
    QCOMPARE(model->property("posterSize").toDouble(), 200.0);
}

// B5, B6: the search capsule keeps a visible gap (at least Theme.spacingSm,
// 8 px) below the page top while the toolbar stays the page's top row and the
// mode column stays visible; the capsule and the trailing toolbar controls
// share one vertical centre and every visible control stays inside the page.
void VideoLibraryViewTest::search_toolbar_keeps_a_top_gap_and_shared_centre() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);

    QVERIFY2(shown_item_named(page, QStringLiteral("librarySidebar")) != nullptr,
             "the mode column is hidden at 960x640");
    QQuickItem* search_bar = nullptr;
    QTRY_VERIFY((search_bar = shown_item_named(page, QStringLiteral("searchBar"))) != nullptr);
    QQuickItem* toolbar = search_bar->parentItem();
    QVERIFY2(toolbar != nullptr, "the search bar has no toolbar parent");

    QCOMPARE(toolbar->mapToItem(page, QPointF()).y(), 0.0);
    const QPointF search_top = search_bar->mapToItem(page, QPointF());
    QVERIFY2(search_top.y() >= 8.0,
             qPrintable(QStringLiteral("the search capsule sits flush with the page top: %1")
                            .arg(search_top.y())));

    const qreal centre = search_top.y() + search_bar->height() / 2;
    const QRectF bounds = page->mapRectToScene(page->boundingRect());
    QVERIFY2(fits_viewport(*search_bar, bounds), "the search capsule leaves the page");

    for (const QString& name :
         QStringList{QStringLiteral("sortButton"), QStringLiteral("layoutListButton"),
                     QStringLiteral("layoutPostersButton"), QStringLiteral("coverSizeButton"),
                     QStringLiteral("refreshButton"), QStringLiteral("addButton")}) {
        QQuickItem* control = shown_item_named(page, name);
        QVERIFY2(control != nullptr, qPrintable(QStringLiteral("the toolbar has no ") + name));
        const QPointF top = control->mapToItem(page, QPointF());
        QVERIFY2(qAbs(top.y() + control->height() / 2 - centre) <= 1.0,
                 qPrintable(QStringLiteral("%1 is not centred with the search capsule").arg(name)));
        QVERIFY2(fits_viewport(*control, bounds),
                 qPrintable(QStringLiteral("%1 leaves the page").arg(name)));
    }

    const QString warnings = warnings_after(QStringLiteral("VideoLibraryPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B7: the collapsed main sidebar passes toolbarInset 44 and the search field
// stays clear of the floating toggle without losing the top gap or the shared
// centre; expanding the sidebar returns the toolbar row to the page's leading
// edge.
void VideoLibraryViewTest::collapsed_sidebar_keeps_the_search_toolbar_clear() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    make_fixture(root.path());

    QQmlApplicationEngine engine;
    engine.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    QVERIFY2(!engine.rootObjects().isEmpty(), "MainWindow was not created");
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    window->setProperty("sectionIndex", 2);
    QQuickItem* search_bar = nullptr;
    QTRY_VERIFY((search_bar = shown_item_named(window->contentItem(),
                                               QStringLiteral("searchBar"))) != nullptr);
    QQuickItem* toolbar = search_bar->parentItem();
    QVERIFY2(toolbar != nullptr, "the search bar has no toolbar parent");
    // The page declares the toolbar row's height; the toolbar itself does not.
    QQuickItem* page = toolbar;
    while (page != nullptr && page->metaObject()->indexOfProperty("toolbarHeight") < 0) {
        page = page->parentItem();
    }
    QVERIFY2(page != nullptr, "the search bar has no page above it");

    QTRY_COMPARE(page->property("toolbarInset").toReal(), 0.0);
    QCOMPARE(toolbar->mapToItem(page, QPointF()).x(), 0.0);
    QVERIFY2(search_bar->mapToItem(page, QPointF()).x() < 44.0,
             "the expanded search bar keeps the collapsed-inset room");
    QTRY_VERIFY2(search_bar->mapToItem(page, QPointF()).y() >= 8.0,
                 "the expanded search capsule sits flush with the page top");

    window->setProperty("sidebarVisible", false);
    QTRY_COMPARE(page->property("toolbarInset").toReal(), 44.0);
    QTRY_VERIFY2(search_bar->mapToItem(page, QPointF()).x() >= 44.0,
                 qPrintable(QStringLiteral("the collapsed search bar starts inside the "
                                           "floating toggle: %1")
                                .arg(search_bar->mapToItem(page, QPointF()).x())));
    QTRY_VERIFY2(search_bar->mapToItem(page, QPointF()).y() >= 8.0,
                 "the collapsed search capsule sits flush with the page top");

    const QRectF bounds = page->mapRectToScene(page->boundingRect());
    QVERIFY2(fits_viewport(*search_bar, bounds), "the collapsed search capsule leaves the page");
    const qreal centre = search_bar->mapToItem(page, QPointF()).y() + search_bar->height() / 2;
    for (const QString& name :
         QStringList{QStringLiteral("sortButton"), QStringLiteral("layoutListButton"),
                     QStringLiteral("layoutPostersButton"), QStringLiteral("coverSizeButton"),
                     QStringLiteral("refreshButton"), QStringLiteral("addButton")}) {
        QQuickItem* control = shown_item_named(page, name);
        QVERIFY2(control != nullptr, qPrintable(QStringLiteral("the toolbar has no ") + name));
        const QPointF top = control->mapToItem(page, QPointF());
        QVERIFY2(qAbs(top.y() + control->height() / 2 - centre) <= 1.0,
                 qPrintable(QStringLiteral("%1 loses the shared centre when collapsed").arg(name)));
        QVERIFY2(fits_viewport(*control, bounds),
                 qPrintable(QStringLiteral("%1 leaves the page when collapsed").arg(name)));
    }

    window->setProperty("sidebarVisible", true);
    QTRY_COMPARE(page->property("toolbarInset").toReal(), 0.0);
    QTRY_COMPARE(toolbar->mapToItem(page, QPointF()).x(), 0.0);

    const QString warnings = warnings_after(QStringLiteral("MainWindow"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B12: at 960x640 nothing visible leaves the page, and the mode column is
// present.
void VideoLibraryViewTest::default_window_fits_every_visible_control() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    make_fixture(root.path());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object = create_page(window, QSize(960, 640), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);

    QVERIFY2(shown_item_named(page, QStringLiteral("librarySidebar")) != nullptr,
             "the mode column is missing at the default window");

    const QRectF bounds = page->mapRectToScene(page->boundingRect());
    std::function<void(QQuickItem*)> verify = [&](QQuickItem* item) {
        if (!shown(item)) {
            return;
        }
        QVERIFY2(fits_viewport(*item, bounds),
                 qPrintable(QStringLiteral("a %1 (%2) leaves the window")
                                .arg(QString::fromLatin1(item->metaObject()->className()),
                                     item->objectName())));
        for (QQuickItem* child : item->childItems()) {
            verify(child);
        }
    };
    verify(page);
}

// B1: MainWindow turns `videoOpened` into a PlayerPage carrying the URL,
// subtitle and flag; the player owns the call to its service.
void VideoLibraryViewTest::main_window_pushes_the_player_with_the_video() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const Fixture fixture = make_fixture(root.path());

    engine_.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    QVERIFY2(!engine_.rootObjects().isEmpty(), "MainWindow was not created");
    auto* window = qobject_cast<QQuickWindow*>(engine_.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    window->setProperty("sectionIndex", 2);
    QQuickItem* card = nullptr;
    QTRY_VERIFY((card = card_with_id(window->contentItem(), fixture.alpha)) != nullptr);
    auto* video = window->findChild<QQuickWindow*>(QStringLiteral("videoWindow"));
    QVERIFY2(video != nullptr, "the Video window is not declared");
    click(card);
    QTRY_VERIFY(video->isVisible());

    QQuickItem* player = nullptr;
    QTRY_VERIFY((player = video->findChild<QQuickItem*>(QStringLiteral("videoPlayerPage"))) !=
                nullptr);
    QCOMPARE(player->property("initialVideoUrl").toUrl(), QUrl::fromLocalFile(fixture.alpha));
    QCOMPARE(player->property("initialSubtitleUrl").toUrl(), QUrl::fromLocalFile(fixture.subtitle));
    QCOMPARE(player->property("startFromBeginning").toBool(), false);
    QTRY_COMPARE(player->property("videoFile").toUrl(), QUrl::fromLocalFile(fixture.alpha));

    niratan::player::PlaybackHistory history(niratan::storage::data_root() /
                                             "video_playback_history.json");
    history.save(fixture.alpha, 50, 100);
    QVERIFY(QMetaObject::invokeMethod(video, "close"));
    QTRY_VERIFY(!video->isVisible());
    QTRY_VERIFY(visual_texts(window->contentItem()).contains(QStringLiteral("0:50 left")));

    const QString warnings = warnings_after(QStringLiteral("MainWindow"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
    engine_.clearComponentCache();
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    VideoLibraryViewTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_video_library_view.moc"

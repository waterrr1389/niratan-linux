// SPDX-License-Identifier: GPL-3.0-or-later
//
// The opened reader page, following NativeMac/NativeReaderView.swift
// (NativeReaderView.body: the full-size NativeReaderWebView, onTapOutside,
// the side-panel dismiss overlay, nativeTopInfoOverlay,
// nativeBottomInfoOverlay, nativeBottomControls, toggleSidePanel,
// isSidePanelOpen, updateReaderContentCoverage, the .gallery overlay;
// NativeReaderSidePanel, NativeReaderGlassIconButton),
// NativeMac/NativeReaderInspector.swift (NativeReaderInspectorHeader,
// NativeReaderInspectorTabBar, NativeReaderEscapeDismissal),
// Features/Reader/StatisticsView.swift (ReaderStatisticsContentView.body,
// sessionHero, bookProgress, historyTable), Features/Reader/Search/ReaderGoToView.swift
// (body, bookSummary, ReaderGoToTab, chaptersTab, searchTab, the Jump to and
// Invalid input alerts), Features/Reader/Gallery/GalleryView.swift,
// Features/Sasayaki/SasayakiSheet.swift, and the Appearance inspector
// (NativeReaderView.body `.appearance` branch -> NativeReaderInspectorHeader +
// NativeSettingsDetailView(.appearance) -> Features/Settings/AppearanceView
// nativeAppearanceContent).
//
// The registered ReaderPage is created inside a window around a book imported
// into a temporary data root (two chapters of ten and eleven characters, the
// first showing one image), with a one-entry dictionary for the lookup case
// and a silent WAV for Sasayaki playback. The empty-gallery case opens the
// text-only epub_fixture.h book instead. The web view never loads the chapter (no reader://
// handler is installed), so the test answers each restore itself, as the page's script would, and
// sends the bridge messages the script sends.
//
// The page exposes, by objectName or property:
// - page: `focusMode` (bool), `activeTool` ("", "goTo", "appearance",
//   "statistics", "sasayaki", "gallery"), `statisticsContextActive`.
// - readerWeb: the chapter WebEngineView. readerLookupSurface: the lookup.
//   Its card exposes radius and border; with the action bar on it holds
//   lookupActionBar and one lookupCloseButton, and a cue adds
//   lookupSasayakiControls with lookupSasayakiReplay, lookupSasayakiPlayback
//   and lookupSasayakiJump.
// - readerTopInfo holding readerTopTitle and readerTopProgress (Text).
// - readerBottomControls holding readerBottomInfo (readerStatisticsLine,
//   readerBottomProgress), the history capsules readerBackButton and
//   readerForwardButton, and the IconButtons readerTrackingButton,
//   readerPlaybackButton, readerGoToButton, readerAppearanceButton,
//   readerStatisticsButton, readerGalleryButton, readerSasayakiButton.
// - readerLeadingPanel and readerTrailingPanel: the side panels (Rectangles).
//   Inside a panel: inspectorTitle (Text), inspectorCloseButton (IconButton),
//   inspectorTab items (`text`, `icon`, `selected`, signal `clicked`).
// - Go to: goToCover, goToTitle, goToPosition, goToJumpButton,
//   goToChapterRow (`selected`, signal `clicked`), goToSearchField
//   (TextField), goToSearchRow and goToHighlightRow (signal `clicked`; a
//   highlight row's right click opens a menu with a Delete item); the
//   dialogs goToJumpDialog with goToJumpField, and goToInvalidDialog.
// - Appearance: the full settings form (objectName "settings-appearance")
//   below inspectorTitle, with the Theme, Text, Layout, Display and Popup
//   cards and their rows. Statistics: statTile (Rectangle). Sasayaki: sasayakiInspector,
//   sasayakiPlaybackHeader, sasayakiSettingsContent.
// - readerGallery: the gallery overlay, with galleryThumbnail items and
//   galleryPreview holding galleryPrevious and galleryNext.

#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"
#include "models/profiles/reader_settings.h"
#include "services/books/book_storage.h"
#include "services/dictionary/dictionary_store.h"
#include "services/settings/settings_store.h"
#include "services/storage/app_paths.h"
#include "viewmodels/reader/reader_view_model.h"
#include "viewmodels/sasayaki/sasayaki_view_model.h"

#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QHash>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QVariantMap>
#include <QWebEngineProfile>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <zip.h>

#include <algorithm>
#include <clocale>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "data_root.h"
#include "epub_fixture.h"
#include "yomitan_fixture.h"

using niratan::viewmodels::ReaderViewModel;
using niratan::viewmodels::SasayakiViewModel;

namespace {

const QString kWord = QStringLiteral("学校");

// A 1x1 RGBA PNG.
const QByteArray kPixel =
    QByteArray::fromHex("89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c489"
                        "0000000d49444154789c63f8cfc0f01f00050001ff89993d1d0000000049454e44"
                        "ae426082");

// Two chapters of ten and eleven readable characters; the first also shows
// one manifested image, which the gallery lists.
bool write_book(const std::filesystem::path& path) {
    int error = 0;
    zip_t* archive = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (archive == nullptr) {
        return false;
    }
    const std::string opf =
        R"XML(<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
    <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
        <dc:title>読書の本</dc:title>
    </metadata>
    <manifest>
        <item id="first" href="first.xhtml" media-type="application/xhtml+xml"/>
        <item id="second" href="second.xhtml" media-type="application/xhtml+xml"/>
        <item id="pixel" href="pixel.png" media-type="image/png"/>
        <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    </manifest>
    <spine>
        <itemref idref="first"/>
        <itemref idref="second"/>
    </spine>
</package>
)XML";
    const std::vector<std::pair<std::string, std::string>> entries{
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml",
         R"XML(<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0">
    <rootfiles>
        <rootfile full-path="book.opf" media-type="application/oebps-package+xml"/>
    </rootfiles>
</container>
)XML"},
        {"book.opf", opf},
        {"nav.xhtml",
         R"XML(<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<body><nav epub:type="toc"><ol>
    <li><a href="first.xhtml">First Chapter</a></li>
    <li><a href="second.xhtml">Second Chapter</a></li>
</ol></nav></body>
</html>)XML"},
        {"first.xhtml",
         "<html><body><p>あいうえおかきくけこ</p><img src=\"pixel.png\"/></body></html>"},
        {"second.xhtml", "<html><body><p>さしすせそたちつあしす</p></body></html>"},
        {"pixel.png", kPixel.toStdString()},
    };
    for (const auto& [name, bytes] : entries) {
        zip_source_t* source = zip_source_buffer(archive, bytes.data(), bytes.size(), 0);
        if (source == nullptr || zip_file_add(archive, name.c_str(), source, 0) < 0) {
            if (source != nullptr) {
                zip_source_free(source);
            }
            zip_discard(archive);
            return false;
        }
    }
    if (zip_close(archive) < 0) {
        zip_discard(archive);
        return false;
    }
    return true;
}

// Two chapters; the first puts an anchor after several screens of vertical
// text, so a same-spine link reaches a nonzero progress. The second is short
// enough to load without an anchor.
bool write_link_book(const std::filesystem::path& path) {
    int error = 0;
    zip_t* archive = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (archive == nullptr) {
        return false;
    }
    const std::string opf =
        R"XML(<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
    <metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>History Links</dc:title></metadata>
    <manifest>
        <item id="first" href="chapter.xhtml" media-type="application/xhtml+xml"/>
        <item id="second" href="second.xhtml" media-type="application/xhtml+xml"/>
        <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    </manifest>
    <spine><itemref idref="first"/><itemref idref="second"/></spine>
</package>
)XML";
    const std::string nav =
        R"XML(<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<body><nav epub:type="toc"><ol>
<li><a href="chapter.xhtml">Deep</a></li>
<li><a href="second.xhtml">Second</a></li>
</ol></nav></body></html>)XML";
    std::string first = "<html><body><p id=\"links\">"
                        "<a id=\"link-target\" href=\"reader://book/chapter.xhtml#target\">one</a>"
                        "<a id=\"link-plain\" href=\"reader://book/chapter.xhtml\">two</a>"
                        "<a id=\"link-empty\" href=\"reader://book/chapter.xhtml#\">three</a>"
                        "</p>";
    for (int paragraph = 0; paragraph < 24; ++paragraph) {
        first += "<p>";
        for (int column = 0; column < 120; ++column) {
            first += "あ";
        }
        first += "</p>";
    }
    first += "<p id=\"target\">到達点</p></body></html>";
    const std::vector<std::pair<std::string, std::string>> entries{
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml",
         R"XML(<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0">
    <rootfiles>
        <rootfile full-path="book.opf" media-type="application/oebps-package+xml"/>
    </rootfiles>
</container>
)XML"},
        {"book.opf", opf},
        {"nav.xhtml", nav},
        {"chapter.xhtml", first},
        {"second.xhtml", "<html><body><p>二章</p></body></html>"},
    };
    for (const auto& [name, bytes] : entries) {
        zip_source_t* source = zip_source_buffer(archive, bytes.data(), bytes.size(), 0);
        if (source == nullptr || zip_file_add(archive, name.c_str(), source, 0) < 0) {
            if (source != nullptr) {
                zip_source_free(source);
            }
            zip_discard(archive);
            return false;
        }
    }
    if (zip_close(archive) < 0) {
        zip_discard(archive);
        return false;
    }
    return true;
}

// 8 kHz mono 8-bit PCM silence, long enough to stay playing during a test.
bool write_silent_wav(const QString& path, int seconds) {
    const quint32 samples = 8000u * static_cast<quint32>(seconds);
    QByteArray bytes;
    const auto put32 = [&bytes](quint32 value) {
        for (int shift = 0; shift < 32; shift += 8) {
            bytes.append(static_cast<char>((value >> shift) & 0xff));
        }
    };
    const auto put16 = [&bytes](quint16 value) {
        bytes.append(static_cast<char>(value & 0xff));
        bytes.append(static_cast<char>(value >> 8));
    };
    bytes.append("RIFF");
    put32(36 + samples);
    bytes.append("WAVEfmt ");
    put32(16);
    put16(1);
    put16(1);
    put32(8000);
    put32(8000);
    put16(1);
    put16(8);
    bytes.append("data");
    put32(samples);
    bytes.append(QByteArray(static_cast<qsizetype>(samples), static_cast<char>(0x80)));
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(bytes) == bytes.size();
}

QString revision(const ReaderViewModel& reader) {
    return QUrlQuery(QUrl(reader.chapter_url())).queryItemValue(QStringLiteral("v"));
}

void send(ReaderViewModel& reader, const QString& name, const QJsonObject& data) {
    reader.message(name, QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
}

void complete_restore(ReaderViewModel& reader) {
    send(reader, QStringLiteral("restoreCompleted"), {{"token", revision(reader)}});
}

bool write_global_settings(bool statistics, bool sasayaki) {
    const auto root = niratan::storage::data_root();
    std::error_code error;
    std::filesystem::create_directories(root, error);
    const QJsonObject settings{
        {"enableStatistics", statistics}, {"statisticsAutostartMode", "Off"},
        {"statisticsResetTime", 0},       {"statisticsResetTimeMigratedToMinutes", true},
        {"enableSasayaki", sasayaki},
    };
    QFile file(QString::fromStdString((root / "settings.json").string()));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(QJsonDocument(settings).toJson()) > 0;
}

bool install_dictionary(const std::filesystem::path& root) {
    const auto zip = root / "reader_dictionary.zip";
    try {
        niratan::test::write_stored_zip(
            zip,
            {{"index.json", niratan::test::minimal_index(QStringLiteral("Reader"))},
             {"term_bank_1.json",
              QByteArrayLiteral(R"([["学校","がっこう","","",0,["school"],1,""]])")}});
    } catch (const std::exception&) {
        return false;
    }
    return niratan::dictionary::DictionaryStore(niratan::storage::dictionaries_root())
        .import_zip(zip)
        .ok;
}

niratan::profiles::ReaderProfileSettings base_settings() {
    auto settings = niratan::profiles::default_reader_settings();
    settings.continuous_mode = false;
    settings.show_title = true;
    settings.show_progress = true;
    settings.show_chapter_progress = true;
    settings.progress_count = "Characters";
    settings.show_percentage = true;
    settings.show_progress_top = true;
    settings.show_reading_speed = true;
    settings.show_reading_time = true;
    settings.show_statistics_toggle = true;
    settings.show_sasayaki_toggle = true;
    return settings;
}

void collect(QQuickItem* item, const QString& name, QList<QQuickItem*>& found) {
    if (item->objectName() == name) {
        found.append(item);
    }
    for (QQuickItem* child : item->childItems()) {
        collect(child, name, found);
    }
}

QList<QQuickItem*> items_named(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> found;
    if (root != nullptr) {
        collect(root, name, found);
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

QList<QQuickItem*> shown_named(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> result;
    for (QQuickItem* item : items_named(root, name)) {
        if (shown(item)) {
            result.append(item);
        }
    }
    std::sort(result.begin(), result.end(), [](QQuickItem* a, QQuickItem* b) {
        return a->mapToScene(QPointF()).x() < b->mapToScene(QPointF()).x();
    });
    return result;
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

// A kit SegmentedPicker segment with the given title.
QQuickItem* shown_segment(QQuickItem* item, const QString& text) {
    if (item->objectName() == QStringLiteral("segment") &&
        item->property("text").toString() == text && shown(item)) {
        return item;
    }
    for (QQuickItem* child : item->childItems()) {
        if (QQuickItem* found = shown_segment(child, text)) {
            return found;
        }
    }
    return nullptr;
}

bool contains_control(QQuickItem* item) {
    for (QQuickItem* child : item->childItems()) {
        if (child->inherits("QQuickControl") || child->inherits("QQuickTextInput") ||
            contains_control(child)) {
            return true;
        }
    }
    return false;
}

bool has_type(QQuickItem* item, const char* prefix) {
    if (item->isVisible() &&
        QString::fromLatin1(item->metaObject()->className()).startsWith(QLatin1String(prefix))) {
        return true;
    }
    for (QQuickItem* child : item->childItems()) {
        if (has_type(child, prefix)) {
            return true;
        }
    }
    return false;
}

QStringList shown_texts(QQuickItem* item) {
    QStringList texts;
    if (!shown(item)) {
        return texts;
    }
    const QVariant text = item->property("text");
    if (text.isValid() && !text.toString().isEmpty()) {
        texts.append(text.toString());
    }
    for (QQuickItem* child : item->childItems()) {
        texts += shown_texts(child);
    }
    return texts;
}

void collect_shown_controls(QQuickItem* item, QList<QQuickItem*>& controls) {
    if (!shown(item)) {
        return;
    }
    if (item->inherits("QQuickControl") || item->inherits("QQuickTextInput")) {
        controls.append(item);
    }
    for (QQuickItem* child : item->childItems()) {
        collect_shown_controls(child, controls);
    }
}

}  // namespace

// Receives the value of a WebEngine runJavaScript callback.
class JsResultReceiver : public QObject {
    Q_OBJECT

   public:
    int request_id = 0;
    QVariant value;

    Q_INVOKABLE void receive(int id, const QVariant& result) {
        if (id == request_id) {
            value = result;
        }
    }
};

class ReaderPresentationTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void init();
    void cleanup();

    void web_view_fills_the_reader();
    void web_view_fills_the_minimum_window();
    void focus_mode_hides_the_chrome();
    void history_controls_follow_targets_and_focus_mode();
    void tap_outside_dismisses_a_lookup_first();
    void lookup_popup_is_a_card();
    void sasayaki_controls_replace_the_action_bar();
    void lookup_popup_fits_the_reader();
    void top_info_shows_title_and_progress();
    void top_info_hides_disabled_lines();
    void bottom_info_takes_progress_when_not_at_top();
    void trailing_buttons_follow_settings();
    void tracking_button_toggles_tracking();
    void one_tool_at_a_time();
    void outside_tap_closes_the_panel_only();
    void escape_closes_the_panel_with_web_focus();
    void inspector_header();
    void go_to_summary_and_tabs();
    void go_to_chapter_row_jumps_and_closes();
    void go_to_search_result_jumps_and_closes();
    void go_to_highlight_jumps_and_deletes();
    void go_to_jump_dialog();
    void appearance_panel_hosts_the_settings_form();
    void stored_padding_reaches_the_reader_document();
    void padding_setters_use_swift_ranges();
    void statistics_panel_keeps_the_tracking_context();
    void gallery_covers_the_reader();
    void gallery_without_images();
    void sasayaki_button_opens_its_inspector();
    void playback_toggle_follows_audio();
    void disabled_tools_are_hidden();
    void history_link_wiring();

   private:
    bool apply_settings(const niratan::profiles::ReaderProfileSettings& settings);
    bool create_page(int width, int height);
    void resize(int width, int height);
    QQuickItem* find(const char* name) const;
    void click(QQuickItem* item);
    void click_at(QPointF point);
    bool open_tool(const char* button, const char* tool);
    QQuickItem* open_panel(const char* name) const;
    void open_lookup();
    QObject* find_object(const char* name) const;

    niratan::test::TempDataRoot data_root_;
    ReaderViewModel* reader_ = nullptr;
    QString folder_;
    QQmlEngine engine_;
    std::unique_ptr<QQuickWindow> window_;
    std::unique_ptr<QObject> object_;
    QQuickItem* page_ = nullptr;
    std::optional<niratan::profiles::ReaderProfileSettings> saved_;
    JsResultReceiver js_receiver_;
};

void ReaderPresentationTest::initTestCase() {
    QVERIFY(data_root_.isValid());
    QVERIFY(write_global_settings(true, true));
    reader_ = ReaderViewModel::instance(0);
    QVERIFY(apply_settings(base_settings()));
    QVERIFY(install_dictionary(std::filesystem::path(data_root_.path().toStdString())));

    const QString source = data_root_.path() + QStringLiteral("/reader.epub");
    QVERIFY(write_book(source.toStdString()));
    QVERIFY(reader_->importAndOpen(QUrl::fromLocalFile(source)));
    folder_ = reader_->book_folder();
    QVERIFY(!folder_.isEmpty());
    complete_restore(*reader_);
}

void ReaderPresentationTest::init() {
    QVERIFY(reader_->has_book());
    QVERIFY(apply_settings(base_settings()));
    if (reader_->chapter_index() != 0) {
        reader_->jumpToCharacter(0);
    }
    if (reader_->restoring()) {
        complete_restore(*reader_);
    }
    QVERIFY(create_page(1100, 760));
}

void ReaderPresentationTest::cleanup() {
    object_.reset();
    window_.reset();
    page_ = nullptr;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    // A test that opened audio or matched subtitles leaves sidecars in the
    // book folder; later pages must not restore them.
    const QString book_dir = QString::fromStdString(niratan::storage::data_root().string()) +
        QStringLiteral("/Books/") + folder_;
    QFile::remove(book_dir + QStringLiteral("/sasayaki_playback.json"));
    QFile::remove(book_dir + QStringLiteral("/sasayaki_match.json"));
    if (reader_->tracking()) {
        reader_->toggleTracking();
    }
    // A slot that opened another book leaves the shared image book reopened.
    if (reader_->book_folder() != folder_) {
        reader_->closeBook();
        QVERIFY(reader_->openBook(folder_));
        complete_restore(*reader_);
    }
}

bool ReaderPresentationTest::apply_settings(
    const niratan::profiles::ReaderProfileSettings& settings) {
    auto store = niratan::settings::SettingsStore::create();
    if (!store) {
        return false;
    }
    const bool live = reader_ != nullptr && reader_->has_book() && saved_ != settings;
    saved_ = settings;
    QSignalSpy changed(reader_, &ReaderViewModel::settingsChanged);
    if (store->save_active_reader(settings) != niratan::settings::StoreError::none) {
        return false;
    }
    if (live && !changed.wait(5000)) {
        return false;
    }
    if (reader_ != nullptr && reader_->restoring()) {
        complete_restore(*reader_);
    }
    return true;
}

bool ReaderPresentationTest::create_page(int width, int height) {
    window_ = std::make_unique<QQuickWindow>();
    window_->resize(width, height);
    QQmlComponent component(
        &engine_, QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/ReaderPage.qml")));
    if (component.isError()) {
        qWarning() << component.errors();
        return false;
    }
    object_.reset(component.beginCreate(engine_.rootContext()));
    page_ = qobject_cast<QQuickItem*>(object_.get());
    if (page_ == nullptr) {
        return false;
    }
    page_->setParentItem(window_->contentItem());
    page_->setSize(QSizeF(width, height));
    component.completeCreate();
    window_->show();
    if (!QTest::qWaitForWindowExposed(window_.get())) {
        return false;
    }
    if (reader_->restoring()) {
        complete_restore(*reader_);
    }
    QTest::qWait(50);
    return true;
}

void ReaderPresentationTest::resize(int width, int height) {
    window_->resize(width, height);
    page_->setSize(QSizeF(width, height));
    QTest::qWait(50);
}

QQuickItem* ReaderPresentationTest::find(const char* name) const {
    return item_named(page_, QString::fromLatin1(name));
}

QObject* ReaderPresentationTest::find_object(const char* name) const {
    return page_->findChild<QObject*>(QString::fromLatin1(name));
}

void ReaderPresentationTest::click(QQuickItem* item) {
    QVERIFY(item != nullptr);
    click_at(item->mapToScene(QPointF(item->width() / 2, item->height() / 2)));
}

void ReaderPresentationTest::click_at(QPointF point) {
    QTest::mouseClick(window_.get(), Qt::LeftButton, Qt::NoModifier, point.toPoint());
    QTest::qWait(30);
}

bool ReaderPresentationTest::open_tool(const char* button, const char* tool) {
    QQuickItem* control = find(button);
    if (!shown(control)) {
        qWarning("%s is not shown", button);
        return false;
    }
    click_at(control->mapToScene(QPointF(control->width() / 2, control->height() / 2)));
    const bool opened = QTest::qWaitFor(
        [&] { return page_->property("activeTool").toString() == QString::fromLatin1(tool); });
    // NativeReaderView animates the panel with .smooth(duration: 0.22).
    QTest::qWait(300);
    return opened;
}

QQuickItem* ReaderPresentationTest::open_panel(const char* name) const {
    QQuickItem* panel = find(name);
    return shown(panel) ? panel : nullptr;
}

// NativeReaderView.handleSelection's textSelected message with the fixture
// dictionary word at normalized offset 0, which is the first cue's start.
void ReaderPresentationTest::open_lookup() {
    const QJsonObject selection{
        {"token", revision(*reader_)},
        {"text", kWord},
        {"sentence", kWord},
        {"rect", QJsonObject{{"x", 100}, {"y", 100}, {"width", 30}, {"height", 30}}},
        {"normalizedOffset", 0},
        {"miningContext",
         QJsonObject{{"currentIndex", 0},
                     {"sentences", QJsonArray{QJsonObject{{"id", "0"}, {"text", kWord}}}}}}};
    send(*reader_, QStringLiteral("textSelected"), selection);
    QQuickItem* surface = find("readerLookupSurface");
    QVERIFY(surface != nullptr);
    QTRY_VERIFY(surface->isVisible());
}

// NativeReaderView.body: the web view takes the whole GeometryReader; no
// header, back control, font buttons or progress bars take rows.
void ReaderPresentationTest::web_view_fills_the_reader() {
    QQuickItem* web = find("readerWeb");
    QVERIFY(web != nullptr);
    QCOMPARE(web->mapRectToItem(page_, QRectF(0, 0, web->width(), web->height())),
             QRectF(0, 0, 1100, 760));
    QVERIFY(!has_type(page_, "PageHeader"));
    QVERIFY(!has_type(page_, "ReaderProgress"));
    QVERIFY(!has_type(page_, "QQuickProgressBar"));
    const QStringList texts = shown_texts(page_);
    QVERIFY(!texts.contains(QStringLiteral("A+")));
    QVERIFY(!texts.contains(QStringLiteral("A-")));
}

void ReaderPresentationTest::web_view_fills_the_minimum_window() {
    resize(720, 520);
    QQuickItem* web = find("readerWeb");
    QVERIFY(web != nullptr);
    QCOMPARE(web->mapRectToItem(page_, QRectF(0, 0, web->width(), web->height())),
             QRectF(0, 0, 720, 520));

    QVERIFY(open_tool("readerAppearanceButton", "appearance"));
    QQuickItem* panel = open_panel("readerTrailingPanel");
    QVERIFY(panel != nullptr);
    const QRectF frame = panel->mapRectToItem(page_, QRectF(0, 0, panel->width(), panel->height()));
    // 720 * 0.32 falls below the 320 floor of the trailing inspector width.
    QCOMPARE(frame, QRectF(720 - 12 - 320, 12, 320, 520 - 24));
    QCOMPARE(web->mapRectToItem(page_, QRectF(0, 0, web->width(), web->height())),
             QRectF(0, 0, 720, 520));

    for (const char* name : {"readerGoToButton", "readerAppearanceButton", "readerGalleryButton"}) {
        QQuickItem* button = find(name);
        QVERIFY2(shown(button), name);
        const QRectF rect =
            button->mapRectToItem(page_, QRectF(0, 0, button->width(), button->height()));
        QVERIFY2(rect.left() >= 0 && rect.right() <= 720 && rect.bottom() <= 520, name);
    }
}

// onTapOutside without a popup toggles focus mode; focus mode removes the
// top and bottom overlays and their hit targets, and the chapter keeps its
// size.
void ReaderPresentationTest::focus_mode_hides_the_chrome() {
    QQuickItem* web = find("readerWeb");
    QVERIFY(web != nullptr);
    QCOMPARE(page_->property("focusMode").toBool(), false);
    QVERIFY(shown(find("readerTopInfo")));
    QVERIFY(shown(find("readerBottomControls")));
    QQuickItem* go_to = find("readerGoToButton");
    QVERIFY(shown(go_to));
    const QPointF go_to_centre =
        go_to->mapToScene(QPointF(go_to->width() / 2, go_to->height() / 2));

    send(*reader_, QStringLiteral("tapOutside"), {{"token", revision(*reader_)}});
    QTRY_COMPARE(page_->property("focusMode").toBool(), true);
    QTest::qWait(300);
    QVERIFY(!shown(find("readerTopInfo")));
    QVERIFY(!shown(find("readerBottomControls")));
    QCOMPARE(web->mapRectToItem(page_, QRectF(0, 0, web->width(), web->height())),
             QRectF(0, 0, 1100, 760));
    click_at(go_to_centre);
    QCOMPARE(page_->property("activeTool").toString(), QString());

    send(*reader_, QStringLiteral("tapOutside"), {{"token", revision(*reader_)}});
    QTRY_COMPARE(page_->property("focusMode").toBool(), false);
    QTRY_VERIFY(shown(find("readerTopInfo")));
    QVERIFY(shown(find("readerBottomControls")));
}

// nativeBottomControls: Back sits on the leading side and Forward before the
// trailing tools, independently shown only while their target resolves.
// Focus mode hides the whole bottom overlay; leaving it restores the controls.
void ReaderPresentationTest::history_controls_follow_targets_and_focus_mode() {
    reader_->closeBook();
    QVERIFY(reader_->openBook(folder_));
    complete_restore(*reader_);
    reader_->updateChapterProgress(0.4);

    QQuickItem* back = find("readerBackButton");
    QQuickItem* forward = find("readerForwardButton");
    QVERIFY(back != nullptr);
    QVERIFY(forward != nullptr);
    QVERIFY(!shown(back));
    QVERIFY(!shown(forward));

    reader_->goToContentsEntry(1, QString());
    complete_restore(*reader_);
    QTRY_VERIFY(shown(back));
    QVERIFY(!shown(forward));
    QCOMPARE(back->property("text").toString(), QStringLiteral("4"));

    click(back);
    if (reader_->restoring()) {
        complete_restore(*reader_);
    }
    QTRY_COMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.4);
    QTRY_VERIFY(!shown(back));
    QTRY_VERIFY(shown(forward));
    QCOMPARE(forward->property("text").toString(), QStringLiteral("10"));

    send(*reader_, QStringLiteral("tapOutside"), {{"token", revision(*reader_)}});
    QTRY_COMPARE(page_->property("focusMode").toBool(), true);
    QTest::qWait(300);
    QVERIFY(!shown(forward));

    send(*reader_, QStringLiteral("tapOutside"), {{"token", revision(*reader_)}});
    QTRY_COMPARE(page_->property("focusMode").toBool(), false);
    QTRY_VERIFY(shown(forward));
    click(forward);
    if (reader_->restoring()) {
        complete_restore(*reader_);
    }
    QTRY_COMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.0);
}

// onTapOutside with a popup closes the popup only; a text selection never
// toggles focus.
void ReaderPresentationTest::tap_outside_dismisses_a_lookup_first() {
    QQuickItem* surface = find("readerLookupSurface");
    QVERIFY(surface != nullptr);
    QVERIFY(!surface->isVisible());

    const QJsonObject selection{
        {"token", revision(*reader_)},
        {"text", kWord},
        {"sentence", kWord},
        {"rect", QJsonObject{{"x", 100}, {"y", 100}, {"width", 30}, {"height", 30}}},
        {"normalizedOffset", 0},
        {"miningContext",
         QJsonObject{{"currentIndex", 0},
                     {"sentences", QJsonArray{QJsonObject{{"id", "0"}, {"text", kWord}}}}}}};
    send(*reader_, QStringLiteral("textSelected"), selection);
    QTRY_VERIFY(surface->isVisible());
    QCOMPARE(page_->property("focusMode").toBool(), false);

    send(*reader_, QStringLiteral("tapOutside"), {{"token", revision(*reader_)}});
    QTRY_VERIFY(!surface->isVisible());
    QCOMPARE(page_->property("focusMode").toBool(), false);
}

// PopupView.body: an opaque card of Theme.popupBackground with a rounded
// corner and a 1 px hairline of textPrimary at 20%; PopupView.actionBar draws
// the close in a single bar. The surface itself is never translucent. The
// corner is the theme's lookupRadius, which matches the other floating
// panels where Swift draws 8: the owner asked for one radius across them.
void ReaderPresentationTest::lookup_popup_is_a_card() {
    auto settings = base_settings();
    settings.popup_action_bar = true;
    QVERIFY(apply_settings(settings));
    QTest::qWait(50);

    QQuickItem* surface = find("readerLookupSurface");
    QVERIFY(surface != nullptr);
    QVERIFY(!surface->isVisible());
    open_lookup();
    QVERIFY(shown(surface));
    QObject* theme =
        engine_.singletonInstance<QObject*>(QStringLiteral("Niratan"), QStringLiteral("Theme"));
    QVERIFY(theme != nullptr);
    QVERIFY(theme->property("lookupRadius").toReal() > 0);
    QCOMPARE(surface->property("radius").toReal(), theme->property("lookupRadius").toReal());
    QObject* border = surface->property("border").value<QObject*>();
    QVERIFY(border != nullptr);
    QCOMPARE(border->property("width").toReal(), 1.0);
    QCOMPARE(surface->property("opacity").toReal(), 1.0);

    QQuickItem* bar = item_named(surface, QStringLiteral("lookupActionBar"));
    QVERIFY2(shown(bar), "the action bar is not shown");
    QList<QQuickItem*> closes = items_named(surface, QStringLiteral("lookupCloseButton"));
    QCOMPARE(closes.size(), qsizetype(1));
    QVERIFY(shown(closes.front()));
    QVERIFY(closes.front()
                ->mapRectToItem(surface,
                                QRectF(0, 0, closes.front()->width(), closes.front()->height()))
                .center()
                .x() > surface->width() / 2);

    click(closes.front());
    QTRY_VERIFY(!surface->isVisible());
}

// PopupView.sasayakiControls(for:includesActionBar:): one row of replay,
// play/pause and jump replaces the action bar; with the bar on, Close sits in
// the same row rather than a second stacked bar.
void ReaderPresentationTest::sasayaki_controls_replace_the_action_bar() {
    auto settings = base_settings();
    settings.popup_action_bar = true;
    QVERIFY(apply_settings(settings));

    SasayakiViewModel* player = nullptr;
    const auto players = page_->findChildren<SasayakiViewModel*>();
    for (auto* candidate : players) {
        player = candidate;
    }
    QVERIFY(player != nullptr);
    const QString audio = data_root_.path() + QStringLiteral("/silence.wav");
    QVERIFY(write_silent_wav(audio, 60));
    QVERIFY(player->openAudio(QUrl::fromLocalFile(audio)));
    QTRY_VERIFY(player->has_audio());

    // The fixture chapter's ten characters; one cue covers offset 0..10.
    const QString subtitles = data_root_.path() + QStringLiteral("/lookup.srt");
    QFile file(subtitles);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray srt = QByteArrayLiteral("1\n"
                                             "00:00:01,000 --> 00:00:03,000\n"
                                             "あいうえおかきくけこ\n"
                                             "\n");
    QCOMPARE(file.write(srt), srt.size());
    file.close();
    QVERIFY(player->openSubtitles(QUrl::fromLocalFile(subtitles)));
    player->matchSubtitles(200);
    QTRY_VERIFY_WITH_TIMEOUT(!player->matching(), 15000);
    QVERIFY2(player->match_error().isEmpty(), qPrintable(player->match_error()));
    QVERIFY(player->has_match());

    QQuickItem* surface = find("readerLookupSurface");
    QVERIFY(surface != nullptr);
    open_lookup();

    QQuickItem* controls = item_named(surface, QStringLiteral("lookupSasayakiControls"));
    QVERIFY2(shown(controls), "the Sasayaki controls are not shown");
    QQuickItem* replay = item_named(surface, QStringLiteral("lookupSasayakiReplay"));
    QQuickItem* playback = item_named(surface, QStringLiteral("lookupSasayakiPlayback"));
    QQuickItem* jump = item_named(surface, QStringLiteral("lookupSasayakiJump"));
    QVERIFY(shown(replay));
    QVERIFY(shown(playback));
    QVERIFY(shown(jump));

    QQuickItem* bar = item_named(surface, QStringLiteral("lookupActionBar"));
    QVERIFY(shown(bar));
    QCOMPARE(controls->mapToScene(QPointF()).y(), bar->mapToScene(QPointF()).y());

    click(playback);
    QTRY_VERIFY_WITH_TIMEOUT(player->playing(), 10000);
    click(playback);
    QTRY_VERIFY(!player->playing());
    click(replay);
    QTRY_VERIFY_WITH_TIMEOUT(player->playing(), 10000);

    click(jump);
    QTRY_VERIFY(!surface->isVisible());
}

// PopupLayout keeps the surface inside the reader page at the default window
// and at the minimum one; every shown control stays inside the card.
void ReaderPresentationTest::lookup_popup_fits_the_reader() {
    open_lookup();

    const auto fits = [&] {
        QQuickItem* surface = find("readerLookupSurface");
        QVERIFY(surface != nullptr);
        const QRectF frame =
            surface->mapRectToItem(page_, QRectF(0, 0, surface->width(), surface->height()));
        QVERIFY2(frame.left() >= 0 && frame.top() >= 0 && frame.right() <= page_->width() &&
                     frame.bottom() <= page_->height(),
                 qPrintable(QStringLiteral("surface %1,%2 %3x%4 outside page %5x%6")
                                .arg(frame.left())
                                .arg(frame.top())
                                .arg(frame.width())
                                .arg(frame.height())
                                .arg(page_->width())
                                .arg(page_->height())));
        QList<QQuickItem*> controls;
        collect_shown_controls(surface, controls);
        for (QQuickItem* control : controls) {
            const QRectF rect =
                control->mapRectToItem(surface, QRectF(0, 0, control->width(), control->height()));
            QVERIFY2(rect.left() >= -0.5 && rect.right() <= surface->width() + 0.5,
                     qPrintable(QString::fromLatin1(control->metaObject()->className())));
        }
    };
    fits();

    // A fresh page at the minimum window, as web_view_fills_the_minimum_window
    // does, keeps the popup placement meaningful after the resize.
    object_.reset();
    window_.reset();
    page_ = nullptr;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(create_page(720, 520));
    open_lookup();
    fits();
}

// nativeTopInfoOverlay: centred title (one line, middle truncation) and the
// top progress line at 12 px with tabular digits; 96 horizontal and 10 top
// padding; text only.
void ReaderPresentationTest::top_info_shows_title_and_progress() {
    QQuickItem* info = find("readerTopInfo");
    QQuickItem* title = find("readerTopTitle");
    QQuickItem* progress = find("readerTopProgress");
    QVERIFY(shown(info));
    QVERIFY(shown(title));
    QVERIFY(shown(progress));
    QCOMPARE(title->property("text").toString(), reader_->title());
    QCOMPARE(title->property("elide").toInt(), 2);  // Text.ElideMiddle
    QCOMPARE(title->property("maximumLineCount").toInt(), 1);
    QCOMPARE(progress->property("text").toString(), reader_->progress_text());
    QVERIFY(!reader_->progress_text().isEmpty());
    const QFont font = progress->property("font").value<QFont>();
    QCOMPARE(font.pixelSize(), 12);
    QCOMPARE(font.featureValue(QFont::Tag("tnum")), 1u);

    const QRectF frame = info->mapRectToItem(page_, QRectF(0, 0, info->width(), info->height()));
    QCOMPARE(frame.top(), 10.0);
    QVERIFY(frame.left() >= 96);
    QVERIFY(frame.right() <= 1100 - 96);
    const QRectF title_frame =
        title->mapRectToItem(page_, QRectF(0, 0, title->width(), title->height()));
    QVERIFY(qAbs(title_frame.center().x() - 550) <= 1);
    QVERIFY(!contains_control(info));

    QVERIFY(!shown(find("readerBottomProgress")));
}

// nativeBottomInfoOverlay: statistics line, then the progress when it is not
// at the top; 11 px tabular digits.
void ReaderPresentationTest::bottom_info_takes_progress_when_not_at_top() {
    auto settings = base_settings();
    settings.show_progress_top = false;
    QVERIFY(apply_settings(settings));
    QTest::qWait(50);

    QVERIFY(!shown(find("readerTopProgress")));
    QQuickItem* progress = find("readerBottomProgress");
    QQuickItem* statistics = find("readerStatisticsLine");
    QVERIFY(shown(progress));
    QVERIFY(shown(statistics));
    QCOMPARE(progress->property("text").toString(), reader_->progress_text());
    QVERIFY(reader_->progress_text().contains(QLatin1Char('\n')));
    QCOMPARE(statistics->property("text").toString(), reader_->statistics_text());
    QVERIFY(!reader_->tracking());
    QVERIFY(!reader_->statistics_text().isEmpty());
    for (QQuickItem* line : {progress, statistics}) {
        const QFont font = line->property("font").value<QFont>();
        QCOMPARE(font.pixelSize(), 11);
        QCOMPARE(font.featureValue(QFont::Tag("tnum")), 1u);
    }
    const QPointF statistics_top = statistics->mapToItem(page_, QPointF());
    const QPointF progress_top = progress->mapToItem(page_, QPointF());
    QVERIFY(statistics_top.y() < progress_top.y());
    QVERIFY(!contains_control(find("readerBottomInfo")));

    settings.show_reading_speed = false;
    settings.show_reading_time = false;
    QVERIFY(apply_settings(settings));
    QTest::qWait(50);
    QVERIFY(!shown(find("readerStatisticsLine")));
}

// nativeBottomControls: the trailing HStack(spacing: 8) of 34 px glass
// buttons with 16 px symbols, inset 32 from the trailing edge and 18 from the
// bottom, in Swift's order.
void ReaderPresentationTest::trailing_buttons_follow_settings() {
    QVERIFY(!shown(find("readerPlaybackButton")));

    const QList<std::pair<const char*, QString>> expected{
        {"readerTrackingButton", QStringLiteral("chart.xyaxis.line")},
        {"readerGoToButton", QStringLiteral("list.bullet")},
        {"readerAppearanceButton", QStringLiteral("paintpalette")},
        {"readerStatisticsButton", QStringLiteral("chart.bar.xaxis")},
        {"readerGalleryButton", QStringLiteral("photo.on.rectangle")},
        {"readerSasayakiButton", QStringLiteral("headphones")},
    };
    const QHash<QString, QString> names{
        {QStringLiteral("readerGoToButton"), QStringLiteral("Go to")},
        {QStringLiteral("readerAppearanceButton"), QStringLiteral("Appearance")},
        {QStringLiteral("readerStatisticsButton"), QStringLiteral("Statistics")},
        {QStringLiteral("readerGalleryButton"), QStringLiteral("Gallery")},
        {QStringLiteral("readerSasayakiButton"), QStringLiteral("Sasayaki")},
    };

    QRectF previous;
    for (const auto& [name, icon] : expected) {
        QQuickItem* button = find(name);
        QVERIFY2(shown(button), name);
        QCOMPARE(button->property("icon").toString(), icon);
        QCOMPARE(button->property("diameter").toInt(), 34);
        QCOMPARE(button->property("iconSize").toReal(), 16.0);
        if (names.contains(QString::fromLatin1(name))) {
            QCOMPARE(button->property("accessibleName").toString(),
                     names.value(QString::fromLatin1(name)));
        }
        const QRectF rect =
            button->mapRectToItem(page_, QRectF(0, 0, button->width(), button->height()));
        QCOMPARE(rect.height(), 34.0);
        QCOMPARE(rect.bottom(), 760.0 - 18);
        if (previous.isValid()) {
            QCOMPARE(rect.left() - previous.right(), 8.0);
        }
        previous = rect;
    }
    QCOMPARE(previous.right(), 1100.0 - 32);

    auto settings = base_settings();
    settings.show_statistics_toggle = false;
    QVERIFY(apply_settings(settings));
    QTest::qWait(50);
    QVERIFY(!shown(find("readerTrackingButton")));
    QVERIFY(shown(find("readerStatisticsButton")));
}

// The statistics toggle shows timer while tracking, else chart.xyaxis.line.
void ReaderPresentationTest::tracking_button_toggles_tracking() {
    QQuickItem* button = find("readerTrackingButton");
    QVERIFY(shown(button));
    QVERIFY(!reader_->tracking());
    click(button);
    QTRY_VERIFY(reader_->tracking());
    QCOMPARE(button->property("icon").toString(), QStringLiteral("timer"));
    click(button);
    QTRY_VERIFY(!reader_->tracking());
    QCOMPARE(button->property("icon").toString(), QStringLiteral("chart.xyaxis.line"));
}

// toggleSidePanel: one active tool; Go to on the leading edge, the others on
// the trailing edge; Go to floats 440 wide and the trailing inspector at
// clamp(0.32 * width, 320, 440), both inside a 12 px inset with
// radius 20 and a 0.7 px border, and the chapter keeps its size. Swift lays
// the panels over nativeBottomControls, so an open trailing panel covers the
// bottom-trailing buttons and closes through its own xmark or Escape.
void ReaderPresentationTest::one_tool_at_a_time() {
    QQuickItem* web = find("readerWeb");
    QVERIFY(web != nullptr);

    QVERIFY(open_tool("readerGoToButton", "goTo"));
    QQuickItem* leading = open_panel("readerLeadingPanel");
    QVERIFY(leading != nullptr);
    QVERIFY(open_panel("readerTrailingPanel") == nullptr);
    QCOMPARE(leading->mapRectToItem(page_, QRectF(0, 0, leading->width(), leading->height())),
             QRectF(12, 12, 440, 760 - 24));
    QCOMPARE(leading->property("radius").toReal(), 20.0);
    auto* border = leading->property("border").value<QObject*>();
    QVERIFY(border != nullptr);
    QCOMPARE(border->property("width").toReal(), 0.7);
    QCOMPARE(web->mapRectToItem(page_, QRectF(0, 0, web->width(), web->height())),
             QRectF(0, 0, 1100, 760));

    // The bottom-trailing buttons lie outside the leading panel.
    QVERIFY(open_tool("readerAppearanceButton", "appearance"));
    QTRY_VERIFY(open_panel("readerLeadingPanel") == nullptr);
    QQuickItem* trailing = open_panel("readerTrailingPanel");
    QVERIFY(trailing != nullptr);
    QCOMPARE(trailing->mapRectToItem(page_, QRectF(0, 0, trailing->width(), trailing->height())),
             QRectF(1100 - 12 - 352, 12, 352, 760 - 24));
    QCOMPARE(web->mapRectToItem(page_, QRectF(0, 0, web->width(), web->height())),
             QRectF(0, 0, 1100, 760));

    click(item_named(trailing, QStringLiteral("inspectorCloseButton")));
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    QTRY_VERIFY(open_panel("readerTrailingPanel") == nullptr);

    QVERIFY(open_tool("readerStatisticsButton", "statistics"));
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());

    // Go to toggles: a second press on its uncovered button closes it.
    QVERIFY(open_tool("readerGoToButton", "goTo"));
    click(find("readerGoToButton"));
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
}

// The isSidePanelOpen overlay: a click on the page closes the panel and does
// not reach the chapter, so focus mode stays off.
void ReaderPresentationTest::outside_tap_closes_the_panel_only() {
    QVERIFY(open_tool("readerGoToButton", "goTo"));
    click_at(QPointF(800, 380));
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    QCOMPARE(page_->property("focusMode").toBool(), false);

    QVERIFY(open_tool("readerAppearanceButton", "appearance"));
    click_at(QPointF(300, 380));
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    QCOMPARE(page_->property("focusMode").toBool(), false);
}

// NativeReaderEscapeDismissal: Escape closes the panel even while the web
// view has focus, and does not close the reader.
void ReaderPresentationTest::escape_closes_the_panel_with_web_focus() {
    QSignalSpy back(page_, SIGNAL(backRequested()));
    QVERIFY(open_tool("readerAppearanceButton", "appearance"));
    QQuickItem* web = find("readerWeb");
    QVERIFY(web != nullptr);
    web->forceActiveFocus();
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    QCOMPARE(back.count(), 0);

    QVERIFY(open_tool("readerGoToButton", "goTo"));
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    QCOMPARE(back.count(), 0);
}

// NativeReaderInspectorHeader: leading title, 28 px xmark Close with an 11 px
// symbol, 18 horizontal and 16 top padding.
void ReaderPresentationTest::inspector_header() {
    QVERIFY(open_tool("readerGoToButton", "goTo"));
    QQuickItem* panel = open_panel("readerLeadingPanel");
    QVERIFY(panel != nullptr);
    QQuickItem* title = item_named(panel, QStringLiteral("inspectorTitle"));
    QVERIFY(shown(title));
    QCOMPARE(title->property("text").toString(), QStringLiteral("Go to"));
    const QPointF title_origin = title->mapToItem(panel, QPointF());
    QCOMPARE(title_origin.x(), 18.0);
    QVERIFY(title_origin.y() >= 16);

    QQuickItem* close = item_named(panel, QStringLiteral("inspectorCloseButton"));
    QVERIFY(shown(close));
    QCOMPARE(close->property("icon").toString(), QStringLiteral("xmark"));
    QCOMPARE(close->property("diameter").toInt(), 28);
    QCOMPARE(close->property("iconSize").toReal(), 11.0);
    QCOMPARE(close->property("accessibleName").toString(), QStringLiteral("Close"));
    const QRectF close_frame =
        close->mapRectToItem(panel, QRectF(0, 0, close->width(), close->height()));
    QCOMPARE(close_frame.right(), 440.0 - 18);

    click(close);
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
}

// ReaderGoToView.bookSummary and NativeReaderInspectorTabBar: cover 46x66,
// two-line title, current position; Search, Chapters and Highlights tabs of
// equal width, Chapters selected first, one selection at a time.
void ReaderPresentationTest::go_to_summary_and_tabs() {
    QVERIFY(open_tool("readerGoToButton", "goTo"));
    QQuickItem* panel = open_panel("readerLeadingPanel");
    QVERIFY(panel != nullptr);

    QQuickItem* cover = item_named(panel, QStringLiteral("goToCover"));
    QVERIFY(shown(cover));
    QCOMPARE(cover->width(), 46.0);
    QCOMPARE(cover->height(), 66.0);
    QQuickItem* title = item_named(panel, QStringLiteral("goToTitle"));
    QVERIFY(shown(title));
    QCOMPARE(title->property("text").toString(), reader_->title());
    QCOMPARE(title->property("maximumLineCount").toInt(), 2);
    QQuickItem* position = item_named(panel, QStringLiteral("goToPosition"));
    QVERIFY(shown(position));
    QCOMPARE(position->property("text").toString(), reader_->position_text());
    QVERIFY(!reader_->position_text().isEmpty());
    QVERIFY(shown(item_named(panel, QStringLiteral("goToJumpButton"))));

    const QList<QQuickItem*> tabs = shown_named(panel, QStringLiteral("inspectorTab"));
    QCOMPARE(tabs.size(), 3);
    const QStringList titles{QStringLiteral("Search"), QStringLiteral("Chapters"),
                             QStringLiteral("Highlights")};
    const QStringList icons{QStringLiteral("magnifyingglass"), QStringLiteral("list.bullet"),
                            QStringLiteral("highlighter")};
    for (int index = 0; index < tabs.size(); ++index) {
        QCOMPARE(tabs[index]->property("text").toString(), titles[index]);
        QCOMPARE(tabs[index]->property("icon").toString(), icons[index]);
        QCOMPARE(tabs[index]->property("selected").toBool(), index == 1);
        QCOMPARE(tabs[index]->width(), tabs[0]->width());
    }

    QVERIFY(QMetaObject::invokeMethod(tabs[0], "clicked"));
    QTRY_VERIFY(tabs[0]->property("selected").toBool());
    QVERIFY(!tabs[1]->property("selected").toBool());
    QVERIFY(!tabs[2]->property("selected").toBool());
    QTRY_VERIFY(shown(item_named(panel, QStringLiteral("goToSearchField"))));

    QVERIFY(QMetaObject::invokeMethod(tabs[2], "clicked"));
    QTRY_VERIFY(tabs[2]->property("selected").toBool());
    QVERIFY(shown_texts(panel).contains(QStringLiteral("No Highlights")));
}

// chaptersTab: the current chapter row is selected; choosing a chapter jumps
// and closes the inspector.
void ReaderPresentationTest::go_to_chapter_row_jumps_and_closes() {
    QVERIFY(open_tool("readerGoToButton", "goTo"));
    QQuickItem* panel = open_panel("readerLeadingPanel");
    QVERIFY(panel != nullptr);
    QList<QQuickItem*> rows;
    QTRY_COMPARE((rows = shown_named(panel, QStringLiteral("goToChapterRow"))).size(),
                 qsizetype(2));
    std::sort(rows.begin(), rows.end(), [](QQuickItem* a, QQuickItem* b) {
        return a->mapToScene(QPointF()).y() < b->mapToScene(QPointF()).y();
    });
    QVERIFY(rows[0]->property("selected").toBool());
    QVERIFY(!rows[1]->property("selected").toBool());

    QVERIFY(QMetaObject::invokeMethod(rows[1], "clicked"));
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    if (reader_->restoring()) {
        complete_restore(*reader_);
    }
    QCOMPARE(reader_->chapter_index(), 1);
}

// searchTab and searchResultsContent: the prompt before a query, No matches
// for a query without hits, then results; choosing a result jumps to its
// character (the second spine starts at 10) and closes the inspector.
void ReaderPresentationTest::go_to_search_result_jumps_and_closes() {
    QVERIFY(open_tool("readerGoToButton", "goTo"));
    QQuickItem* panel = open_panel("readerLeadingPanel");
    QVERIFY(panel != nullptr);
    const QList<QQuickItem*> tabs = shown_named(panel, QStringLiteral("inspectorTab"));
    QCOMPARE(tabs.size(), 3);
    QVERIFY(QMetaObject::invokeMethod(tabs[0], "clicked"));

    QQuickItem* field = nullptr;
    QTRY_VERIFY(shown(field = item_named(panel, QStringLiteral("goToSearchField"))));
    QTRY_VERIFY(shown_texts(panel).contains(QStringLiteral("Enter text to search this book")));

    const int chapter = reader_->chapter_index();
    {
        QSignalSpy searched(reader_, &ReaderViewModel::searchChanged);
        field->setProperty("text", QStringLiteral("not-in-this-book"));
        QVERIFY(QMetaObject::invokeMethod(field, "accepted"));
        QTRY_VERIFY(searched.count() >= 2 && reader_->search_state() == 2);
    }
    QTRY_VERIFY(shown_texts(panel).contains(QStringLiteral("No matches")));
    QVERIFY(shown_named(panel, QStringLiteral("goToSearchRow")).isEmpty());
    QCOMPARE(reader_->chapter_index(), chapter);
    QCOMPARE(page_->property("activeTool").toString(), QStringLiteral("goTo"));

    const auto scene_y = [](const QQuickItem* item) { return item->mapToScene(QPointF()).y(); };
    const auto by_y = [&](QList<QQuickItem*> items) {
        std::ranges::sort(items, {}, scene_y);
        return items;
    };

    // groupedSearchResults: one header per chapter with hits, naming it and
    // its count, above that chapter's results.
    {
        QSignalSpy searched(reader_, &ReaderViewModel::searchChanged);
        field->setProperty("text", QStringLiteral("あ"));
        QVERIFY(QMetaObject::invokeMethod(field, "accepted"));
        QTRY_VERIFY(searched.count() >= 2 && reader_->search_state() == 2);
    }
    QList<QQuickItem*> headers;
    QTRY_VERIFY((headers = shown_named(panel, QStringLiteral("goToSearchSection"))).size() == 2);
    headers = by_y(headers);
    QCOMPARE(shown_texts(headers[0]),
             QStringList({QStringLiteral("First Chapter"), QStringLiteral("1 result")}));
    QCOMPARE(shown_texts(headers[1]),
             QStringList({QStringLiteral("Second Chapter"), QStringLiteral("1 result")}));
    const QList<QQuickItem*> hit_rows = by_y(shown_named(panel, QStringLiteral("goToSearchRow")));
    QCOMPARE(hit_rows.size(), qsizetype(2));
    QVERIFY(scene_y(headers[0]) < scene_y(hit_rows[0]));
    QVERIFY(scene_y(hit_rows[0]) < scene_y(headers[1]));
    QVERIFY(scene_y(headers[1]) < scene_y(hit_rows[1]));

    {
        QSignalSpy searched(reader_, &ReaderViewModel::searchChanged);
        field->setProperty("text", QStringLiteral("しす"));
        QVERIFY(QMetaObject::invokeMethod(field, "accepted"));
        QTRY_VERIFY(searched.count() >= 2 && reader_->search_state() == 2);
    }
    QList<QQuickItem*> rows;
    QTRY_VERIFY((rows = shown_named(panel, QStringLiteral("goToSearchRow"))).size() == 2);
    rows = by_y(rows);
    const QList<QQuickItem*> sections = shown_named(panel, QStringLiteral("goToSearchSection"));
    QCOMPARE(sections.size(), qsizetype(1));
    QCOMPARE(shown_texts(sections.first()),
             QStringList({QStringLiteral("Second Chapter"), QStringLiteral("2 results")}));
    QVERIFY(scene_y(sections.first()) < scene_y(rows.first()));
    QVERIFY(QMetaObject::invokeMethod(rows.first(), "clicked"));
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    if (reader_->restoring()) {
        complete_restore(*reader_);
    }
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->character_offset(), 11);
}

// The Jump to alert: Cancel changes nothing, an invalid count shows Invalid
// input and keeps the inspector, a valid count jumps and closes it.
void ReaderPresentationTest::go_to_jump_dialog() {
    QVERIFY(open_tool("readerGoToButton", "goTo"));
    QQuickItem* panel = open_panel("readerLeadingPanel");
    QVERIFY(panel != nullptr);
    QQuickItem* jump = item_named(panel, QStringLiteral("goToJumpButton"));
    QVERIFY(shown(jump));

    QObject* dialog = find_object("goToJumpDialog");
    QObject* invalid = find_object("goToInvalidDialog");
    QVERIFY(dialog != nullptr);
    QVERIFY(invalid != nullptr);
    QObject* field = dialog->findChild<QObject*>(QStringLiteral("goToJumpField"));
    QVERIFY(field != nullptr);

    click(jump);
    QTRY_VERIFY(dialog->property("opened").toBool());
    field->setProperty("text", QStringLiteral("12"));
    QVERIFY(QMetaObject::invokeMethod(dialog, "reject"));
    QTRY_VERIFY(!dialog->property("visible").toBool());
    QCOMPARE(page_->property("activeTool").toString(), QStringLiteral("goTo"));
    QCOMPARE(reader_->chapter_index(), 0);

    click(jump);
    QTRY_VERIFY(dialog->property("opened").toBool());
    QCOMPARE(field->property("text").toString(), QString());
    field->setProperty("text", QStringLiteral("abc"));
    QVERIFY(QMetaObject::invokeMethod(dialog, "accept"));
    QTRY_VERIFY(invalid->property("visible").toBool());
    QCOMPARE(page_->property("activeTool").toString(), QStringLiteral("goTo"));
    QCOMPARE(reader_->chapter_index(), 0);
    QVERIFY(QMetaObject::invokeMethod(invalid, "close"));
    QTRY_VERIFY(!invalid->property("visible").toBool());

    click(jump);
    QTRY_VERIFY(dialog->property("opened").toBool());
    field->setProperty("text", QStringLiteral("12"));
    QVERIFY(QMetaObject::invokeMethod(dialog, "accept"));
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    if (reader_->restoring()) {
        complete_restore(*reader_);
    }
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->character_offset(), 12);
}

// NativeReaderView.body `.appearance` branch: NativeReaderInspectorHeader
// ("Appearance", Close) above NativeSettingsDetailView(.appearance), which is
// AppearanceView.nativeAppearanceContent: the same Theme, Text, Layout,
// Display and Popup cards as the Settings page, scrolling inside the panel.
void ReaderPresentationTest::appearance_panel_hosts_the_settings_form() {
    QVERIFY(open_tool("readerAppearanceButton", "appearance"));
    QQuickItem* panel = open_panel("readerTrailingPanel");
    QVERIFY(panel != nullptr);
    QQuickItem* title = item_named(panel, QStringLiteral("inspectorTitle"));
    QVERIFY(shown(title));
    QCOMPARE(title->property("text").toString(), QStringLiteral("Appearance"));
    QVERIFY(shown(item_named(panel, QStringLiteral("inspectorCloseButton"))));

    QQuickItem* form = item_named(panel, QStringLiteral("settings-appearance"));
    QVERIFY2(shown(form), "the Appearance panel does not host the settings form");

    const QStringList texts = shown_texts(panel);
    const QStringList cards{QStringLiteral("Theme"), QStringLiteral("Text"),
                            QStringLiteral("Layout"), QStringLiteral("Display"),
                            QStringLiteral("Popup")};
    for (const QString& card : cards) {
        QVERIFY2(texts.contains(card), qPrintable(card + QStringLiteral(" card title missing")));
    }
    const QStringList rows{
        QStringLiteral("Text Orientation"),   QStringLiteral("Font Size"),
        QStringLiteral("Furigana"),           QStringLiteral("Mode"),
        QStringLiteral("Horizontal Padding"), QStringLiteral("Vertical Padding")};
    for (const QString& row : rows) {
        QVERIFY2(texts.contains(row), qPrintable(row + QStringLiteral(" row missing")));
    }

    const QStringList themeSegments{QStringLiteral("System"), QStringLiteral("Light"),
                                    QStringLiteral("Dark"), QStringLiteral("Sepia"),
                                    QStringLiteral("Custom")};
    for (const QString& segment : themeSegments) {
        QVERIFY2(shown_segment(panel, segment),
                 qPrintable(segment + QStringLiteral(" theme segment missing")));
    }
    const QStringList furiganaSegments{QStringLiteral("Show"), QStringLiteral("Dimmed"),
                                       QStringLiteral("Tap to Reveal"), QStringLiteral("Hidden")};
    for (const QString& segment : furiganaSegments) {
        QVERIFY2(shown_segment(panel, segment),
                 qPrintable(segment + QStringLiteral(" furigana segment missing")));
    }

    // ScrollView clips vertically, so only horizontal clipping fails.
    QList<QQuickItem*> controls;
    collect_shown_controls(form, controls);
    QVERIFY(!controls.isEmpty());
    for (QQuickItem* control : controls) {
        const QRectF rect =
            control->mapRectToItem(panel, QRectF(0, 0, control->width(), control->height()));
        QVERIFY2(rect.left() >= -0.5 && rect.right() <= panel->width() + 0.5,
                 qPrintable(QString::fromLatin1(control->metaObject()->className())));
    }

    QQuickItem* horizontal = shown_segment(panel, "Horizontal");
    QVERIFY(horizontal != nullptr);
    click(horizontal);
    QTRY_COMPARE(reader_->vertical_writing(), false);
    QTRY_COMPARE(
        niratan::settings::SettingsStore::create()->active_reader().settings.vertical_writing,
        false);

    saved_.reset();
    QVERIFY(apply_settings(base_settings()));
}

// AppearanceView's Horizontal/Vertical Padding Steppers reach
// NativeReaderView.injectReader unchanged: PagePaddingCss, columnGap and
// bottomPaddingCss follow the saved values.
void ReaderPresentationTest::stored_padding_reaches_the_reader_document() {
    auto settings = base_settings();
    settings.horizontal_padding = 80;
    settings.vertical_padding = 50;
    settings.vertical_writing = true;
    settings.font_size = 22;
    QVERIFY(apply_settings(settings));
    complete_restore(*reader_);

    const std::string document = reader_->document_for("first.xhtml");
    QVERIFY2(document.find("padding: 25vh 40vw !important;") != std::string::npos,
             document.c_str());
    QVERIFY2(document.find("column-gap: calc(50vh + 22px) !important;") != std::string::npos,
             document.c_str());
    QVERIFY2(document.find("padding-bottom: calc(25vh + 22px) !important;") != std::string::npos,
             document.c_str());
    QVERIFY2(document.find("max-width: 20vw !important;") != std::string::npos, document.c_str());
    QVERIFY2(document.find("max-height: calc(50vh - 11px) !important;") != std::string::npos,
             document.c_str());

    reader_->closeBook();
    QVERIFY(reader_->openBook(folder_));
    complete_restore(*reader_);
    const std::string reopened = reader_->document_for("first.xhtml");
    QVERIFY2(reopened.find("padding: 25vh 40vw !important;") != std::string::npos,
             reopened.c_str());
    QVERIFY2(reopened.find("column-gap: calc(50vh + 22px) !important;") != std::string::npos,
             reopened.c_str());
}

// AppearanceView's Stepper ranges: Horizontal Padding 0...80 and Vertical
// Padding 0...50, clamped at both ends.
void ReaderPresentationTest::padding_setters_use_swift_ranges() {
    const int originalHorizontal = reader_->horizontal_padding();
    const int originalVertical = reader_->vertical_padding();

    reader_->setHorizontalPadding(80);
    QCOMPARE(reader_->horizontal_padding(), 80);
    reader_->setHorizontalPadding(81);
    QCOMPARE(reader_->horizontal_padding(), 80);
    reader_->setHorizontalPadding(-1);
    QCOMPARE(reader_->horizontal_padding(), 0);

    reader_->setVerticalPadding(50);
    QCOMPARE(reader_->vertical_padding(), 50);
    reader_->setVerticalPadding(51);
    QCOMPARE(reader_->vertical_padding(), 50);
    reader_->setVerticalPadding(-1);
    QCOMPARE(reader_->vertical_padding(), 0);

    // Direct edits are transient and do not invalidate the watched file.
    reader_->setHorizontalPadding(originalHorizontal);
    reader_->setVerticalPadding(originalVertical);
}

// updateReaderContentCoverage: the statistics inspector keeps the reader a
// tracking context; other tools and the gallery cover it.
void ReaderPresentationTest::statistics_panel_keeps_the_tracking_context() {
    QVERIFY(page_->property("statisticsContextActive").toBool());
    QVERIFY(open_tool("readerStatisticsButton", "statistics"));
    QQuickItem* panel = open_panel("readerTrailingPanel");
    QVERIFY(panel != nullptr);
    QVERIFY(page_->property("statisticsContextActive").toBool());
    const QList<QQuickItem*> heroes = shown_named(panel, QStringLiteral("sessionHero"));
    QCOMPARE(heroes.size(), qsizetype(1));
    QCOMPARE(heroes.front()->property("radius").toReal(), 16.0);
    const QStringList hero_texts = shown_texts(heroes.front());
    QVERIFY(hero_texts.contains(QStringLiteral("Session")));
    QVERIFY(hero_texts.contains(QStringLiteral("Paused")));
    QVERIFY(hero_texts.contains(QStringLiteral("Reading Speed")));
    QVERIFY(hero_texts.contains(QStringLiteral("Reading Time")));
    const QStringList panel_texts = shown_texts(panel);
    QVERIFY(panel_texts.contains(QStringLiteral("Book Progress")));
    QVERIFY(panel_texts.contains(QStringLiteral("History")));
    QVERIFY(panel_texts.contains(QStringLiteral("Today")));
    QVERIFY(panel_texts.contains(QStringLiteral("All Time")));
    click(item_named(panel, QStringLiteral("inspectorCloseButton")));
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    QVERIFY(page_->property("statisticsContextActive").toBool());

    QVERIFY(open_tool("readerGoToButton", "goTo"));
    QVERIFY(!page_->property("statisticsContextActive").toBool());
    // The bottom-trailing buttons lie outside the leading panel.
    QVERIFY(open_tool("readerAppearanceButton", "appearance"));
    QVERIFY(!page_->property("statisticsContextActive").toBool());
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    QVERIFY(page_->property("statisticsContextActive").toBool());
}

// The .gallery overlay covers the whole reader with its header. A thumbnail
// opens GalleryImagePreview, whose Previous and Next are disabled for a single
// image; the first Escape closes the preview only, the second the gallery.
// Browsing never moves the chapter.
void ReaderPresentationTest::gallery_covers_the_reader() {
    const int chapter = reader_->chapter_index();
    QVERIFY(open_tool("readerGalleryButton", "gallery"));
    QQuickItem* gallery = find("readerGallery");
    QVERIFY(shown(gallery));
    QCOMPARE(gallery->mapRectToItem(page_, QRectF(0, 0, gallery->width(), gallery->height())),
             QRectF(0, 0, 1100, 760));
    QVERIFY(!page_->property("statisticsContextActive").toBool());
    QVERIFY(shown_texts(gallery).contains(QStringLiteral("Gallery")));
    QTRY_VERIFY(!reader_->gallery_loading());
    QCOMPARE(reader_->gallery_images().size(), 1);

    QQuickItem* thumbnail = nullptr;
    QTRY_VERIFY(shown(thumbnail = item_named(gallery, QStringLiteral("galleryThumbnail"))));
    click(thumbnail);
    QQuickItem* preview = nullptr;
    QTRY_VERIFY(shown(preview = item_named(page_, QStringLiteral("galleryPreview"))));
    QQuickItem* previous = item_named(preview, QStringLiteral("galleryPrevious"));
    QQuickItem* next = item_named(preview, QStringLiteral("galleryNext"));
    QVERIFY(previous != nullptr);
    QVERIFY(next != nullptr);
    QVERIFY(!previous->isEnabled());
    QVERIFY(!next->isEnabled());
    QCOMPARE(reader_->chapter_index(), chapter);

    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QTRY_VERIFY(!shown(item_named(page_, QStringLiteral("galleryPreview"))));
    QCOMPARE(page_->property("activeTool").toString(), QStringLiteral("gallery"));
    QVERIFY(shown(item_named(gallery, QStringLiteral("galleryThumbnail"))));

    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    QTRY_VERIFY(!shown(find("readerGallery")));
    QCOMPARE(reader_->chapter_index(), chapter);
}

// GalleryView.body with no images: once indexing ends, the else-if
// images.isEmpty branch shows No Images and nothing to preview. Escape closes
// the gallery without moving the chapter. The text-only book is opened for
// this case only; cleanup reopens the image book.
void ReaderPresentationTest::gallery_without_images() {
    object_.reset();
    window_.reset();
    page_ = nullptr;
    const QString source = data_root_.path() + QStringLiteral("/text_only.epub");
    QVERIFY(niratan::test::write_epub(source.toStdString(), "文字だけの本", "あいうえお",
                                      "かきくけこ"));
    reader_->closeBook();
    QVERIFY(reader_->importAndOpen(QUrl::fromLocalFile(source)));
    complete_restore(*reader_);
    QVERIFY(create_page(1100, 760));

    const int chapter = reader_->chapter_index();
    QVERIFY(open_tool("readerGalleryButton", "gallery"));
    QQuickItem* gallery = find("readerGallery");
    QVERIFY(shown(gallery));
    QTRY_VERIFY(!reader_->gallery_loading());
    QCOMPARE(reader_->gallery_images().size(), 0);
    QTRY_VERIFY(shown_texts(gallery).contains(QStringLiteral("No Images")));
    QVERIFY(shown_named(page_, QStringLiteral("galleryThumbnail")).isEmpty());
    QVERIFY(!shown(item_named(page_, QStringLiteral("galleryPreview"))));

    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    QCOMPARE(reader_->chapter_index(), chapter);

    QVERIFY(open_tool("readerGoToButton", "goTo"));
    QQuickItem* panel = open_panel("readerLeadingPanel");
    QVERIFY(panel != nullptr);
    QTRY_VERIFY(shown_texts(panel).contains(QStringLiteral("No Chapters")));
    QVERIFY(shown_named(panel, QStringLiteral("goToChapterRow")).isEmpty());
}

// SasayakiSheet without audio: the playback header is absent and the
// automatic tab choice is Resources; choosing Settings shows its content.
void ReaderPresentationTest::sasayaki_button_opens_its_inspector() {
    QVERIFY(open_tool("readerSasayakiButton", "sasayaki"));
    QQuickItem* panel = open_panel("readerTrailingPanel");
    QVERIFY(panel != nullptr);
    QQuickItem* inspector = item_named(panel, QStringLiteral("sasayakiInspector"));
    QVERIFY(shown(inspector));
    QVERIFY(!shown(item_named(panel, QStringLiteral("sasayakiPlaybackHeader"))));

    const QList<QQuickItem*> tabs = shown_named(panel, QStringLiteral("inspectorTab"));
    QStringList titles;
    for (QQuickItem* tab : tabs) {
        titles.append(tab->property("text").toString());
    }
    QCOMPARE(titles,
             (QStringList{QStringLiteral("Resources"), QStringLiteral("Chapters"),
                          QStringLiteral("Settings")}));
    QVERIFY(tabs[0]->property("selected").toBool());
    QVERIFY(!tabs[1]->property("selected").toBool());
    QVERIFY(!tabs[2]->property("selected").toBool());

    QVERIFY(QMetaObject::invokeMethod(tabs[2], "clicked"));
    QTRY_VERIFY(tabs[2]->property("selected").toBool());
    QVERIFY(!tabs[0]->property("selected").toBool());
    QVERIFY(!tabs[1]->property("selected").toBool());
    QTRY_VERIFY(shown(item_named(panel, QStringLiteral("sasayakiSettingsContent"))));
    QCOMPARE(page_->property("activeTool").toString(), QStringLiteral("sasayaki"));
}

// nativeTopInfoOverlay without the title keeps the progress line; with no
// title and no progress there is no overlay.
void ReaderPresentationTest::top_info_hides_disabled_lines() {
    auto settings = base_settings();
    settings.show_title = false;
    settings.show_progress_top = true;
    QVERIFY(apply_settings(settings));
    QTest::qWait(50);
    QVERIFY(!shown(find("readerTopTitle")));
    QVERIFY(shown(find("readerTopProgress")));
    QCOMPARE(find("readerTopProgress")->property("text").toString(), reader_->progress_text());

    settings.show_progress = false;
    settings.show_chapter_progress = false;
    QVERIFY(apply_settings(settings));
    QTest::qWait(50);
    QVERIFY(reader_->progress_text().isEmpty());
    QVERIFY(!shown(find("readerTopInfo")));
}

// highlightsTab: a highlight row shows its trimmed text; choosing it jumps
// and closes the inspector; its Delete context action removes it and keeps
// the inspector open.
void ReaderPresentationTest::go_to_highlight_jumps_and_deletes() {
    reader_->jumpToCharacter(12);
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 1);
    QVERIFY(reader_->highlights().isEmpty());
    reader_->addHighlight(reader_->chapter_url(), QStringLiteral("yellow"),
                          QVariantMap{{"start", 2},
                                      {"offset", 2},
                                      {"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                      {"text", QStringLiteral("  しす  ")}});
    QCOMPARE(reader_->highlights().size(), 1);
    reader_->jumpToCharacter(0);
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);

    QVERIFY(open_tool("readerGoToButton", "goTo"));
    QQuickItem* panel = open_panel("readerLeadingPanel");
    QVERIFY(panel != nullptr);
    QList<QQuickItem*> tabs = shown_named(panel, QStringLiteral("inspectorTab"));
    QCOMPARE(tabs.size(), 3);
    QVERIFY(QMetaObject::invokeMethod(tabs[2], "clicked"));
    QList<QQuickItem*> rows;
    QTRY_VERIFY((rows = shown_named(panel, QStringLiteral("goToHighlightRow"))).size() == 1);
    QTRY_VERIFY(shown_texts(rows.first()).contains(QStringLiteral("しす")));
    QVERIFY(QMetaObject::invokeMethod(rows.first(), "clicked"));
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());
    if (reader_->restoring()) {
        complete_restore(*reader_);
    }
    QCOMPARE(reader_->chapter_index(), 1);

    QVERIFY(open_tool("readerGoToButton", "goTo"));
    panel = open_panel("readerLeadingPanel");
    QVERIFY(panel != nullptr);
    tabs = shown_named(panel, QStringLiteral("inspectorTab"));
    QCOMPARE(tabs.size(), 3);
    QVERIFY(QMetaObject::invokeMethod(tabs[2], "clicked"));
    QTRY_VERIFY((rows = shown_named(panel, QStringLiteral("goToHighlightRow"))).size() == 1);
    QQuickItem* row = rows.first();
    QTest::mouseClick(window_.get(), Qt::RightButton, Qt::NoModifier,
                      row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
    QQuickItem* remove = nullptr;
    QTRY_VERIFY((remove = shown_control(window_->contentItem(), "QQuickMenuItem",
                                        QStringLiteral("Delete"))) != nullptr);
    click(remove);
    QTRY_VERIFY(reader_->highlights().isEmpty());
    QCOMPARE(page_->property("activeTool").toString(), QStringLiteral("goTo"));
    QTRY_VERIFY(shown_texts(panel).contains(QStringLiteral("No Highlights")));
}

// nativeBottomControls' playback toggle: shown with enable, show and loaded
// audio; waveform while stopped and pause.fill while playing. The Sasayaki
// inspector shows its playback header with audio, and switching its tabs
// leaves playback alone. Hiding the toggle keeps the Sasayaki entry.
void ReaderPresentationTest::playback_toggle_follows_audio() {
    SasayakiViewModel* player = nullptr;
    for (auto* candidate : page_->findChildren<SasayakiViewModel*>()) {
        player = candidate;
    }
    QVERIFY(player != nullptr);
    const QString audio = data_root_.path() + QStringLiteral("/silence.wav");
    QVERIFY(write_silent_wav(audio, 60));
    QVERIFY(player->openAudio(QUrl::fromLocalFile(audio)));
    QTRY_VERIFY(player->has_audio());
    QVERIFY(!player->playing());

    QQuickItem* button = nullptr;
    QTRY_VERIFY(shown(button = find("readerPlaybackButton")));
    QCOMPARE(button->property("icon").toString(), QStringLiteral("waveform"));
    click(button);
    QTRY_VERIFY_WITH_TIMEOUT(player->playing(), 10000);
    QTRY_COMPARE(button->property("icon").toString(), QStringLiteral("pause.fill"));

    QVERIFY(open_tool("readerSasayakiButton", "sasayaki"));
    QQuickItem* panel = open_panel("readerTrailingPanel");
    QVERIFY(panel != nullptr);
    QVERIFY(shown(item_named(panel, QStringLiteral("sasayakiPlaybackHeader"))));
    const QList<QQuickItem*> tabs = shown_named(panel, QStringLiteral("inspectorTab"));
    QCOMPARE(tabs.size(), 3);
    for (QQuickItem* tab : tabs) {
        QVERIFY(QMetaObject::invokeMethod(tab, "clicked"));
        QTRY_VERIFY(tab->property("selected").toBool());
        QVERIFY(player->playing());
    }
    QTest::keyClick(window_.get(), Qt::Key_Escape);
    QTRY_COMPARE(page_->property("activeTool").toString(), QString());

    click(button);
    QTRY_VERIFY(!player->playing());
    QTRY_COMPARE(button->property("icon").toString(), QStringLiteral("waveform"));

    auto settings = base_settings();
    settings.show_sasayaki_toggle = false;
    QVERIFY(apply_settings(settings));
    QTest::qWait(50);
    QVERIFY(player->has_audio());
    QVERIFY(!shown(find("readerPlaybackButton")));
    QVERIFY(shown(find("readerSasayakiButton")));
}

// Statistics and Sasayaki controls disappear with their features.
void ReaderPresentationTest::disabled_tools_are_hidden() {
    object_.reset();
    window_.reset();
    page_ = nullptr;
    QVERIFY(write_global_settings(false, false));
    reader_->closeBook();
    QVERIFY(reader_->openBook(folder_));
    complete_restore(*reader_);
    QVERIFY(create_page(1100, 760));

    QVERIFY(!shown(find("readerTrackingButton")));
    QVERIFY(!shown(find("readerStatisticsButton")));
    QVERIFY(!shown(find("readerSasayakiButton")));
    QVERIFY(!shown(find("readerPlaybackButton")));
    QVERIFY(!shown(find("readerStatisticsLine")));
    QVERIFY(shown(find("readerGoToButton")));
    QVERIFY(shown(find("readerAppearanceButton")));
    QVERIFY(shown(find("readerGalleryButton")));
}

// jumpToLink wiring through the WebKit coordinator's link policy: anchors in
// the rendered document are clicked, so interception, lookup dismissal and the
// browser-side jump all run. A same-spine destination below the first viewport
// stores the DOM progress it reached; a missing fragment restores zero; a
// present-empty fragment is observed at the reader's jumpToFragment call.
// Cross-spine present-empty and missing fragments keep their initial restore
// expressions.
void ReaderPresentationTest::history_link_wiring() {
    const QString source = data_root_.path() + QStringLiteral("/links.epub");
    QVERIFY(write_link_book(source.toStdString()));

    object_.reset();
    window_.reset();
    page_ = nullptr;
    engine_.globalObject().setProperty(QStringLiteral("hoshiTestReceiver"),
                                       engine_.newQObject(&js_receiver_));
    auto* handler = new niratan::ui::ReaderSchemeHandler(QWebEngineProfile::defaultProfile());
    handler->set_view_model(reader_);
    QWebEngineProfile::defaultProfile()->installUrlSchemeHandler(QByteArrayLiteral("reader"),
                                                                 handler);
    QVERIFY(reader_->importAndOpen(QUrl::fromLocalFile(source)));
    const QString folder = reader_->book_folder();
    QVERIFY(create_page(1100, 760));
    const QString chapter_url = reader_->chapter_url();
    QVERIFY(!chapter_url.isEmpty());

    QQuickItem* web = find("readerWeb");
    QVERIFY(web != nullptr);

    // Runs a script in the chapter and returns the callback's value.
    const auto run_js = [this, web](const QString& script) {
        js_receiver_.value = QVariant();
        const int id = ++js_receiver_.request_id;
        const QJSValue callback = engine_.evaluate(
            QStringLiteral("(function(result) { hoshiTestReceiver.receive(%1, result); })")
                .arg(id));
        if (!QMetaObject::invokeMethod(web, "runJavaScript", Q_ARG(QString, script),
                                       Q_ARG(QJSValue, callback))) {
            return QVariant();
        }
        for (int waited = 0; waited < 5000; waited += 20) {
            QTest::qWait(20);
            if (js_receiver_.value.isValid()) {
                return js_receiver_.value;
            }
        }
        return QVariant();
    };

    // create_page can complete the model restore before Chromium loads the chapter.
    QTRY_VERIFY_WITH_TIMEOUT(
        run_js(QStringLiteral(
                   "window.hoshiReaderRestoreToken === '%1' && "
                   "['link-target', 'link-plain', 'link-empty'].every(id => "
                   "document.getElementById(id) instanceof HTMLAnchorElement) && "
                   "['calculateProgress', 'jumpToFragment', 'restoreProgress'].every(method => "
                   "typeof window.hoshiReader?.[method] === 'function')")
                   .arg(revision(*reader_)))
            .toBool(),
        10000);

    // A user click on an anchor, which the navigation policy sees as a link.
    const auto click_anchor = [this, web, &run_js](const QString& id) {
        const QVariant rect =
            run_js(QStringLiteral("(function() { var r = document.getElementById(\"%1\")"
                                  ".getBoundingClientRect();"
                                  " return [r.left + r.width / 2, r.top + r.height / 2]; })()")
                       .arg(id));
        const QVariantList point = rect.toList();
        QVERIFY2(point.size() == 2, qPrintable(id));
        const QPointF scene =
            web->mapToScene(QPointF(point.at(0).toDouble(), point.at(1).toDouble()));
        QTest::mouseClick(window_.get(), Qt::LeftButton, Qt::NoModifier, scene.toPoint());
        QTest::qWait(50);
    };

    // The anchors sit at the chapter start; a jump leaves them off-screen.
    const auto scroll_to_start = [&run_js] {
        const QVariant value = run_js(QStringLiteral(
            "window.hoshiReader.setScrollOffset(window.hoshiReader.getScrollContext(), 0); 1"));
        QVERIFY(value.isValid());
        QTest::qWait(50);
    };

    QQuickItem* surface = find("readerLookupSurface");
    QVERIFY(surface != nullptr);
    open_lookup();
    QVERIFY(surface->isVisible());

    click_anchor(QStringLiteral("link-target"));
    QTRY_VERIFY(!surface->isVisible());
    QCOMPARE(reader_->chapter_url(), chapter_url);
    QTRY_VERIFY_WITH_TIMEOUT(reader_->chapter_progress() > 0.0, 10000);
    QVERIFY(!reader_->property("internalJumpInProgress").toBool());
    // The stored position is the DOM's own progress after the jump, not the
    // origin the model recorded before it.
    const QVariant dom_result = run_js(QStringLiteral("window.hoshiReader.calculateProgress()"));
    QCOMPARE(dom_result.typeId(), QMetaType::Double);
    const double dom_progress = dom_result.toDouble();
    QVERIFY(dom_progress > 0.0);
    QCOMPARE(reader_->chapter_progress(), dom_progress);
    auto bookmark = niratan::books::BookStorage(niratan::storage::data_root())
                        .load_bookmark(folder.toStdString());
    QVERIFY(bookmark.has_value());
    QVERIFY(bookmark->progress > 0.0);
    QCOMPARE(bookmark->progress, dom_progress);

    scroll_to_start();
    click_anchor(QStringLiteral("link-plain"));
    QCOMPARE(reader_->chapter_url(), chapter_url);
    QTRY_COMPARE_WITH_TIMEOUT(reader_->chapter_progress(), 0.0, 10000);
    bookmark = niratan::books::BookStorage(niratan::storage::data_root())
                   .load_bookmark(folder.toStdString());
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->progress, 0.0);

    // Observe the jumpToFragment and restoreProgress calls the empty-fragment
    // click actually makes.
    QVERIFY(run_js(QStringLiteral("window.hoshiObserved = { jump: [], restore: 0 };"
                                  "var reader = window.hoshiReader;"
                                  "var jump = reader.jumpToFragment.bind(reader);"
                                  "reader.jumpToFragment = function(...args) {"
                                  "  window.hoshiObserved.jump.push(args);"
                                  "  return jump.apply(this, args); };"
                                  "var restore = reader.restoreProgress.bind(reader);"
                                  "reader.restoreProgress = function(...args) {"
                                  "  window.hoshiObserved.restore += 1;"
                                  "  return restore.apply(this, args); }; 1"))
                .isValid());
    click_anchor(QStringLiteral("link-empty"));
    QCOMPARE(reader_->chapter_url(), chapter_url);
    QJsonObject observed;
    for (int attempt = 0; attempt < 100; ++attempt) {
        observed =
            QJsonDocument::fromJson(
                run_js(QStringLiteral("JSON.stringify(window.hoshiObserved)")).toString().toUtf8())
                .object();
        if (!observed.value(QStringLiteral("jump")).toArray().isEmpty() ||
            observed.value(QStringLiteral("restore")).toInt() > 0) {
            break;
        }
        QTest::qWait(20);
    }
    const QJsonArray jumps = observed.value(QStringLiteral("jump")).toArray();
    QCOMPARE(jumps.size(), 1);
    QCOMPARE(jumps.at(0).toArray(), QJsonArray{QStringLiteral("")});
    QCOMPARE(observed.value(QStringLiteral("restore")).toInt(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(reader_->chapter_progress(), 0.0, 10000);

    QVERIFY(reader_->jumpToLink(QUrl(QStringLiteral("reader://book/second.xhtml#"))));
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    const QString empty_fragment_document =
        QString::fromStdString(reader_->document_for("second.xhtml"));
    QVERIFY(empty_fragment_document.contains(
        QStringLiteral("return window.hoshiReader.jumpToFragment(\"\")")));
    QVERIFY(!empty_fragment_document.contains(
        QStringLiteral("return window.hoshiReader.restoreProgress(")));

    QTRY_VERIFY_WITH_TIMEOUT(!reader_->restoring(), 10000);
    reader_->navigateBackwards();
    QTRY_VERIFY_WITH_TIMEOUT(!reader_->restoring(), 10000);
    QVERIFY(reader_->jumpToLink(QUrl(QStringLiteral("reader://book/second.xhtml"))));
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    const QString absent_fragment_document =
        QString::fromStdString(reader_->document_for("second.xhtml"));
    QVERIFY(absent_fragment_document.contains(
        QStringLiteral("return window.hoshiReader.restoreProgress(0)")));
    QVERIFY(!absent_fragment_document.contains(
        QStringLiteral("return window.hoshiReader.jumpToFragment(")));
}

int main(int argc, char* argv[]) {
    niratan::ui::register_popup_url_schemes();
    niratan::ui::register_reader_url_schemes();
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    ReaderPresentationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_reader_presentation.moc"

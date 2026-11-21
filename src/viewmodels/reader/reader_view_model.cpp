// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_view_model.h"

#include <QDateTime>
#include <QFile>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

#include "models/profiles/profile_index.h"
#include "models/profiles/reader_settings.h"
#include "models/statistics/statistics.h"
#include "services/books/book_storage.h"
#include "services/profiles/profile_repository.h"
#include "services/reader/reading_tracker.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

namespace niratan::viewmodels {
namespace {

using niratan::books::BookStorage;
using niratan::books::Bookmark;
using niratan::profiles::ProfileRepository;
using niratan::profiles::ReaderProfileSettings;
using niratan::reader::BookSession;
using niratan::reader::BookStatisticsStore;
using niratan::reader::PageSize;
using niratan::reader::ReaderScripts;
using niratan::reader::ReadingTracker;
using niratan::reader::ShellInput;
using niratan::statistics::ReadingTotal;
using niratan::statistics::StatisticsAutostartMode;

std::int64_t now_milliseconds() { return QDateTime::currentMSecsSinceEpoch(); }

std::string read_resource(const QString& path) {
    QFile file(path.startsWith(QStringLiteral(":/qtwebchannel/"))
                   ? path
                   : QStringLiteral(":/niratan/reader/") + path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll().toStdString();
}

double clamp_progress(double progress) { return std::min(std::max(progress, 0.0), 1.0); }

StatisticsAutostartMode parse_autostart_mode(const QString& raw) {
    if (raw == QString::fromStdString(std::string(
                   niratan::statistics::autostart_mode_id(StatisticsAutostartMode::page_turn)))) {
        return StatisticsAutostartMode::page_turn;
    }
    if (raw == QString::fromStdString(
                   std::string(niratan::statistics::autostart_mode_id(
                       StatisticsAutostartMode::on)))) {
        return StatisticsAutostartMode::on;
    }
    return StatisticsAutostartMode::off;
}

// The statistics keys of <data root>/settings.json. Reads never write: the
// statistics page owns the stored values. A missing or damaged file is the
// default configuration, as UserConfig's synthesized decode would fall back.
struct ReaderStatisticsSettings {
    bool enabled = false;
    StatisticsAutostartMode autostart_mode = StatisticsAutostartMode::off;
    int reset_minutes = 0;
};

ReaderStatisticsSettings load_statistics_settings(const std::filesystem::path& path) {
    ReaderStatisticsSettings settings;
    const storage::JsonReadResult read = storage::read_json_file(path);
    if (read.status != storage::JsonReadStatus::success || !read.document.isObject()) {
        return settings;
    }
    const QJsonObject object = read.document.object();
    settings.enabled = object.value(QStringLiteral("enableStatistics")).toBool(false);
    settings.autostart_mode = parse_autostart_mode(
        object.value(QStringLiteral("statisticsAutostartMode")).toString(QStringLiteral("Off")));
    // normalizedResetMinutes clamps the same range, so the stored value never
    // reaches the tracker out of range.
    settings.reset_minutes =
        std::clamp(object.value(QStringLiteral("statisticsResetTime")).toInt(0), 0,
                   niratan::statistics::minutes_per_day - 1);
    return settings;
}

std::string resolve_furigana(const ReaderProfileSettings& settings) {
    if (settings.furigana_mode.has_value()) {
        return *settings.furigana_mode;
    }
    return settings.hide_furigana ? "Hidden" : "Off";
}

}  // namespace

struct ReaderViewModel::State {
    State()
        : storage(storage::data_root()),
          profiles(storage::data_root()),
          assets(load_assets()) {}

    static ReaderScripts load_assets() {
        ReaderScripts scripts;
        scripts.selection = read_resource(QStringLiteral("selection.js"));
        scripts.reader = read_resource(QStringLiteral("reader.js"));
        scripts.scrollreader = read_resource(QStringLiteral("scrollreader.js"));
        scripts.highlights = read_resource(QStringLiteral("highlights.js"));
        scripts.paragraph = read_resource(QStringLiteral("paragraph.js"));
        scripts.webchannel = read_resource(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
        return scripts;
    }

    ReaderProfileSettings load_settings() {
        const std::string profile_id = profiles.active_profile().id;
        const std::filesystem::path path =
            storage::data_root() / "Profiles" / profile_id / "reader_settings.json";
        const storage::JsonReadResult read = storage::read_json_file(path);
        if (read.status == storage::JsonReadStatus::success) {
            if (const auto decoded = profiles::reader_settings_from_json(read.document)) {
                return *decoded;
            }
        }
        return profiles::default_reader_settings();
    }

    BookStorage storage;
    ProfileRepository profiles;
    ReaderScripts assets;
    ReaderProfileSettings settings = profiles::default_reader_settings();
    std::optional<BookSession> session;
    books::BookMetadata book;
    bool has_book = false;

    // The store must outlive the tracker that borrows it.
    std::unique_ptr<BookStatisticsStore> statistics_store;
    std::unique_ptr<ReadingTracker> tracker;
    bool statistics_enabled = false;
    int statistics_today_characters = 0;
    int statistics_today_seconds = 0;
    int statistics_session_speed = 0;
    int chapter_index = 0;
    double chapter_progress = 0.0;
    int page_width = 800;
    int page_height = 600;
    int revision = 0;
    QString error;
    QString chapter_url;
    bool dark = false;
    std::string background;
    std::string text;
};

ReaderViewModel* ReaderViewModel::instance() {
    static ReaderViewModel* model = new ReaderViewModel();
    return model;
}

ReaderViewModel* ReaderViewModel::create(QQmlEngine*, QJSEngine*) {
    ReaderViewModel* model = instance();
    // The singleton outlives every engine, so QML must not take ownership.
    QJSEngine::setObjectOwnership(model, QJSEngine::CppOwnership);
    return model;
}

ReaderViewModel::ReaderViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    state_->settings = state_->load_settings();
    setObjectName(QStringLiteral("readerBridge"));
}

ReaderViewModel::~ReaderViewModel() = default;

QString ReaderViewModel::title() const {
    return state_->has_book ? QString::fromStdString(state_->book.display_title()) : QString();
}

QString ReaderViewModel::chapter_title() const {
    if (!state_->session.has_value()) {
        return {};
    }
    return QString::fromStdString(state_->session->chapter_title(state_->chapter_index)).isEmpty()
               ? QString::fromStdString(state_->session->spine_path(state_->chapter_index))
               : QString::fromStdString(
                     state_->session->chapter_title(state_->chapter_index));
}

int ReaderViewModel::chapter_index() const { return state_->chapter_index; }

int ReaderViewModel::chapter_count() const {
    return state_->session.has_value() ? state_->session->spine_count() : 0;
}

QString ReaderViewModel::book_folder() const {
    return state_->has_book ? QString::fromStdString(state_->book.folder) : QString();
}

double ReaderViewModel::progress() const {
    if (!state_->session.has_value() || state_->session->info().character_count <= 0) {
        return 0.0;
    }
    const int characters =
        state_->session->character_offset(state_->chapter_index, state_->chapter_progress);
    return static_cast<double>(characters) / state_->session->info().character_count;
}

double ReaderViewModel::chapter_progress() const { return state_->chapter_progress; }

QString ReaderViewModel::chapter_url() const { return state_->chapter_url; }

QVariantList ReaderViewModel::chapters() const {
    QVariantList result;
    if (!state_->session.has_value()) {
        return result;
    }
    for (int index = 0; index < state_->session->spine_count(); ++index) {
        const QString title =
            QString::fromStdString(state_->session->chapter_title(index)).isEmpty()
                ? QString::fromStdString(state_->session->spine_path(index))
                : QString::fromStdString(state_->session->chapter_title(index));
        result.append(QVariantMap{{QStringLiteral("index"), index}, {QStringLiteral("title"), title}});
    }
    return result;
}

QString ReaderViewModel::error() const { return state_->error; }

bool ReaderViewModel::has_book() const { return state_->has_book; }

bool ReaderViewModel::dark() const { return state_->dark; }

void ReaderViewModel::setDark(bool dark) {
    if (state_->dark == dark) {
        return;
    }
    state_->dark = dark;
    emit paletteChanged();
    reload_document();
}

void ReaderViewModel::setReaderColors(const QString& background, const QString& text) {
    state_->background = background.toStdString();
    state_->text = text.toStdString();
    reload_document();
}

bool ReaderViewModel::vertical_writing() const { return state_->settings.vertical_writing; }

void ReaderViewModel::setVerticalWriting(bool vertical) {
    if (state_->settings.vertical_writing == vertical) {
        return;
    }
    state_->settings.vertical_writing = vertical;
    emit settingsChanged();
    reload_document();
}

int ReaderViewModel::font_size() const { return state_->settings.font_size; }

void ReaderViewModel::setFontSize(int size) {
    const int clamped = std::min(std::max(size, 8), 72);
    if (state_->settings.font_size == clamped) {
        return;
    }
    state_->settings.font_size = clamped;
    emit settingsChanged();
    reload_document();
}

int ReaderViewModel::horizontal_padding() const { return state_->settings.horizontal_padding; }

void ReaderViewModel::setHorizontalPadding(int padding) {
    const int clamped = std::min(std::max(padding, 0), 40);
    if (state_->settings.horizontal_padding == clamped) {
        return;
    }
    state_->settings.horizontal_padding = clamped;
    emit settingsChanged();
    reload_document();
}

int ReaderViewModel::vertical_padding() const { return state_->settings.vertical_padding; }

void ReaderViewModel::setVerticalPadding(int padding) {
    const int clamped = std::min(std::max(padding, 0), 40);
    if (state_->settings.vertical_padding == clamped) {
        return;
    }
    state_->settings.vertical_padding = clamped;
    emit settingsChanged();
    reload_document();
}

int ReaderViewModel::statistics_today_characters() const {
    return state_->statistics_today_characters;
}

int ReaderViewModel::statistics_today_seconds() const { return state_->statistics_today_seconds; }

int ReaderViewModel::statistics_session_speed() const {
    return state_->statistics_session_speed;
}

bool ReaderViewModel::statistics_enabled() const { return state_->statistics_enabled; }

void ReaderViewModel::setReaderActive(bool active) {
    if (state_->tracker) {
        state_->tracker->set_context_active(active, now_milliseconds());
    }
    refresh_statistics();
}

bool ReaderViewModel::importAndOpen(const QUrl& file) {
    const std::filesystem::path source = file.toLocalFile().toStdString();
    const books::ImportResult imported = state_->storage.import_book(source);
    if (!imported.book.has_value()) {
        state_->error =
            imported.already_exists
                ? tr("A book with this title is already in the library.")
                : QString::fromStdString(imported.error);
        emit errorChanged();
        return false;
    }
    return openBook(QString::fromStdString(imported.book->folder));
}

bool ReaderViewModel::openBook(const QString& folder) {
    const std::string folder_id = folder.toStdString();
    const auto metadata = state_->storage.load_metadata(folder_id);
    if (!metadata.has_value()) {
        state_->error = tr("The book could not be opened.");
        emit errorChanged();
        return false;
    }
    const auto epub = state_->storage.epub_path(*metadata);
    if (!epub.has_value()) {
        state_->error = tr("The book file is missing.");
        emit errorChanged();
        return false;
    }
    std::string error;
    auto session = BookSession::open(*epub, &error);
    if (!session.has_value()) {
        state_->error = QString::fromStdString(error);
        emit errorChanged();
        return false;
    }

    state_->book = *metadata;
    state_->session = std::move(session);
    state_->has_book = true;
    state_->error.clear();
    emit errorChanged();

    // Statistics is configured for this book, after the session is open so a
    // failed open leaves no tracker behind. The settings file is read on every
    // open; the reader never writes it.
    const ReaderStatisticsSettings statistics_settings =
        load_statistics_settings(state_->storage.app_directory() / "settings.json");
    state_->statistics_enabled = statistics_settings.enabled;
    // Replacing an open book must not leave a tracker borrowing a destroyed
    // store; the tracker goes first.
    state_->tracker.reset();
    state_->statistics_store.reset();
    if (const auto book_root = state_->storage.book_directory(folder_id);
        book_root.has_value()) {
        state_->statistics_store = std::make_unique<BookStatisticsStore>(*book_root);
        state_->tracker = std::make_unique<ReadingTracker>(*state_->statistics_store,
                                                           QTimeZone(QTimeZone::LocalTime));
        state_->tracker->configure(statistics_settings.enabled,
                                   statistics_settings.autostart_mode,
                                   statistics_settings.reset_minutes);
        if (statistics_settings.autostart_mode == StatisticsAutostartMode::on) {
            state_->tracker->start(now_milliseconds());
        }
    }

    int chapter = 0;
    double progress = 0.0;
    if (const auto bookmark = state_->storage.load_bookmark(folder_id)) {
        chapter = bookmark->chapter_index;
        progress = bookmark->progress;
    }
    state_->chapter_index = std::min(std::max(chapter, 0), state_->session->spine_count() - 1);
    state_->chapter_progress = clamp_progress(progress);

    emit bookChanged();
    reload_document();
    emit chapterChanged();
    emit progressChanged();
    refresh_statistics();
    return true;
}

void ReaderViewModel::closeBook() {
    if (!state_->has_book) {
        return;
    }
    persist_bookmark();
    if (state_->tracker) {
        const std::int64_t now = now_milliseconds();
        state_->tracker->stop(now);
        state_->tracker->save(now, state_->book.display_title());
    }
    state_->tracker.reset();
    state_->statistics_store.reset();
    state_->statistics_enabled = false;
    state_->session.reset();
    state_->book = books::BookMetadata{};
    state_->has_book = false;
    state_->chapter_index = 0;
    state_->chapter_progress = 0.0;
    state_->chapter_url.clear();
    emit bookChanged();
    emit chapterChanged();
    emit progressChanged();
    emit chapterUrlChanged();
    refresh_statistics();
}

void ReaderViewModel::setPageSize(int width, int height) {
    const int new_width = std::max(width, 1);
    const int new_height = std::max(height, 1);
    if (state_->page_width == new_width && state_->page_height == new_height) {
        return;
    }
    state_->page_width = new_width;
    state_->page_height = new_height;
    reload_document();
}

void ReaderViewModel::goToChapter(int index) {
    if (!state_->session.has_value()) {
        return;
    }
    if (index < 0 || index >= state_->session->spine_count() || index == state_->chapter_index) {
        return;
    }
    persist_bookmark();
    save_statistics();
    load_chapter(index, 0.0);
}

void ReaderViewModel::goToProgress(double progress) {
    if (!state_->session.has_value()) {
        return;
    }
    const auto position =
        state_->session->position_for_character(static_cast<int>(
            clamp_progress(progress) * state_->session->info().character_count));
    if (!position.has_value()) {
        return;
    }
    persist_bookmark();
    save_statistics();
    load_chapter(position->first, position->second);
}

QString ReaderViewModel::pageTurnScript(bool forward) const {
    if (!state_->has_book) {
        return {};
    }
    return QStringLiteral(
               "(function() { window.hoshiParagraph?.finishTextAnimation(); return "
               "window.hoshiReader.paginate('%1'); })()")
        .arg(forward ? QStringLiteral("forward") : QStringLiteral("backward"));
}

void ReaderViewModel::handlePageTurnResult(bool forward, const QVariant& result) {
    if (!state_->session.has_value()) {
        return;
    }
    // NativeReaderView's onPageTurn: Page Turn mode starts here, before the
    // result moves the position.
    if (state_->tracker) {
        state_->tracker->on_manual_navigation(now_milliseconds());
    }
    if (result.typeId() == QMetaType::Double || result.typeId() == QMetaType::Int) {
        updateChapterProgress(result.toDouble());
        return;
    }
    if (result.toString() == QStringLiteral("limit")) {
        if (forward) {
            if (state_->chapter_index + 1 < state_->session->spine_count()) {
                // Moving to the next chapter's start is the last page's
                // forward jump, so the characters read up to the boundary are
                // credited before the session is stored.
                if (state_->tracker) {
                    state_->tracker->update_position(
                        state_->session->character_offset(state_->chapter_index + 1, 0.0),
                        now_milliseconds());
                }
                persist_bookmark();
                save_statistics();
                load_chapter(state_->chapter_index + 1, 0.0);
            }
        } else if (state_->chapter_index > 0) {
            if (state_->tracker) {
                state_->tracker->update_position(
                    state_->session->character_offset(state_->chapter_index - 1, 1.0),
                    now_milliseconds());
            }
            persist_bookmark();
            save_statistics();
            load_chapter(state_->chapter_index - 1, 1.0);
        }
    }
}

void ReaderViewModel::updateChapterProgress(double progress) {
    const double clamped = clamp_progress(progress);
    if (std::abs(clamped - state_->chapter_progress) < 1e-6) {
        return;
    }
    state_->chapter_progress = clamped;
    if (state_->tracker) {
        // The tracker works on the book's global character offset, as
        // statisticsCharacterPosition does. The unchanged-progress guard above
        // is what keeps a chapter load or restore from crediting anything.
        state_->tracker->update_position(
            state_->session->character_offset(state_->chapter_index, clamped),
            now_milliseconds());
    }
    emit progressChanged();
    persist_bookmark();
    refresh_statistics();
}

void ReaderViewModel::saveBookmark() { persist_bookmark(); }

void ReaderViewModel::message(const QString& name, const QString& payload) {
    if (name == QStringLiteral("progressChanged")) {
        bool ok = false;
        const double progress = payload.toDouble(&ok);
        if (ok) {
            updateChapterProgress(progress);
        }
    } else if (name == QStringLiteral("pageChanged")) {
        emit pageChanged(payload.toInt());
    } else if (name == QStringLiteral("wheelNavigation")) {
        // The wheel is a manual page turn; Page Turn mode starts here, before
        // the page runs the navigation the signal asks for.
        if (state_->tracker) {
            state_->tracker->on_manual_navigation(now_milliseconds());
        }
        emit pageTurnRequested(payload);
    }
}

std::optional<niratan::reader::SessionResource> ReaderViewModel::resource(
    const std::string& path) {
    if (!state_->session.has_value()) {
        return std::nullopt;
    }
    return state_->session->resource(path);
}

bool ReaderViewModel::is_open() const { return state_->has_book; }

std::string ReaderViewModel::document_for(const std::string& path) {
    if (!state_->session.has_value()) {
        return {};
    }
    for (int index = 0; index < state_->session->spine_count(); ++index) {
        if (state_->session->spine_path(index) == path) {
            return shell_document(path);
        }
    }
    return {};
}

void ReaderViewModel::load_chapter(int index, double progress) {
    state_->chapter_index = index;
    state_->chapter_progress = clamp_progress(progress);
    reload_document();
    emit chapterChanged();
    emit progressChanged();
    refresh_statistics();
}

void ReaderViewModel::reload_document() {
    if (!state_->session.has_value()) {
        return;
    }
    ++state_->revision;
    const std::string path = state_->session->spine_path(state_->chapter_index);
    state_->chapter_url = QStringLiteral("reader://book/%1?v=%2")
                              .arg(QString::fromStdString(path))
                              .arg(state_->revision);
    emit chapterUrlChanged();
}

void ReaderViewModel::persist_bookmark() {
    if (!state_->session.has_value()) {
        return;
    }
    Bookmark bookmark;
    bookmark.chapter_index = state_->chapter_index;
    bookmark.progress = state_->chapter_progress;
    bookmark.character_count =
        state_->session->character_offset(state_->chapter_index, state_->chapter_progress);
    bookmark.last_modified = books::reference_now();
    state_->storage.save_bookmark(state_->book.folder, bookmark);
}

void ReaderViewModel::save_statistics() {
    if (!state_->tracker) {
        return;
    }
    // flushStats(): credit the elapsed time up to the position the bookmark
    // was just persisted at, then store the session. The tracker's save()
    // reloads the records, so the bookmark written above is the snapshot the
    // persistence policy compares against.
    const std::int64_t now = now_milliseconds();
    state_->tracker->flush(now);
    state_->tracker->save(now, state_->book.display_title());
}

void ReaderViewModel::refresh_statistics() {
    ReadingTotal today;
    ReadingTotal session;
    if (state_->tracker) {
        const std::int64_t now = now_milliseconds();
        today = state_->tracker->today_total(now);
        session = state_->tracker->session_total();
    }
    const int today_characters = std::max(today.characters_read, 0);
    const int today_seconds = static_cast<int>(std::max(today.reading_time, 0.0));
    const int session_speed = session.reading_speed();
    const bool enabled = state_->statistics_enabled;
    if (state_->statistics_today_characters == today_characters &&
        state_->statistics_today_seconds == today_seconds &&
        state_->statistics_session_speed == session_speed &&
        state_->statistics_enabled == enabled) {
        return;
    }
    state_->statistics_today_characters = today_characters;
    state_->statistics_today_seconds = today_seconds;
    state_->statistics_session_speed = session_speed;
    state_->statistics_enabled = enabled;
    emit statisticsChanged();
}

std::string ReaderViewModel::shell_document(const std::string& path) {
    if (!state_->session.has_value()) {
        return {};
    }
    ShellInput input;
    input.appearance.vertical_writing = state_->settings.vertical_writing;
    input.appearance.continuous = state_->settings.continuous_mode;
    input.appearance.paragraph_mode =
        state_->settings.paragraph_mode.value_or(false) && !input.appearance.continuous;
    input.appearance.two_column = state_->settings.two_column_horizontal_pages.value_or(false);
    input.appearance.avoid_page_break = state_->settings.avoid_page_break;
    input.appearance.justify_text = state_->settings.justify_text;
    input.appearance.layout_advanced = state_->settings.layout_advanced;
    input.appearance.blur_images = state_->settings.blur_images;
    input.appearance.selected_font = state_->settings.selected_font;
    input.appearance.font_size = state_->settings.font_size;
    input.appearance.horizontal_padding = state_->settings.horizontal_padding;
    input.appearance.vertical_padding = state_->settings.vertical_padding;
    input.appearance.line_height = state_->settings.line_height;
    input.appearance.character_spacing = state_->settings.character_spacing;
    input.appearance.paragraph_spacing = state_->settings.paragraph_spacing;
    input.appearance.furigana_mode = resolve_furigana(state_->settings);

    input.page = PageSize{state_->page_width, state_->page_height};
    input.colors.text = state_->text;
    input.colors.background = state_->background.empty() ? "#FFFFFF" : state_->background;
    input.colors.sasayaki_text = state_->text;
    input.colors.sasayaki_background = "#00000000";
    input.restore_token = std::to_string(state_->revision);
    if (state_->chapter_progress > 0.0) {
        input.initial_restore_expression =
            "window.hoshiReader.restoreProgress(" +
            std::to_string(state_->chapter_progress) + ")";
    }

    for (int index = 0; index < state_->session->spine_count(); ++index) {
        if (state_->session->spine_path(index) == path) {
            return state_->session->chapter_document(index, input, state_->assets);
        }
    }
    return {};
}

}  // namespace niratan::viewmodels

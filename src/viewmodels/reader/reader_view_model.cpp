// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_view_model.h"

#include <QDateTime>
#include <QFile>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonObject>
#include <QPromise>
#include <QThreadPool>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

#include "models/profiles/reader_settings.h"
#include "models/statistics/statistics.h"
#include "services/books/book_storage.h"
#include "services/reader/reading_tracker.h"
#include "services/reader/book_search.h"
#include "services/reader/theme.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

namespace niratan::viewmodels {
namespace {

using niratan::books::BookStorage;
using niratan::books::Bookmark;
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

std::string script_string(const QString& value) {
    QByteArray array = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    // An EPUB identifier is data even when embedded in an inline script.
    array.replace("<", "\\u003c");
    return array.mid(1, array.size() - 2).toStdString();
}

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

QColor qcolor_from_css(const std::string& css) {
    QString color = QString::fromStdString(css);
    if (color.size() == 9) {
        color = QLatin1Char('#') + color.right(2) + color.mid(1, 6);
    }
    return QColor::fromString(color);
}

}  // namespace

struct ReaderViewModel::State {
    explicit State(std::function<void()> on_settings_changed)
        : storage(storage::data_root()),
          settings_watch(storage.app_directory(), std::move(on_settings_changed)),
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

    BookStorage storage;
    reader::ReaderSettingsWatch settings_watch;
    ReaderScripts assets;
    ReaderProfileSettings settings = profiles::default_reader_settings();
    std::optional<ReaderProfileSettings> loaded_settings;
    std::string content_language = "ja";
    int scan_length = 16;
    bool scan_non_japanese = true;
    std::optional<BookSession> session;
    std::filesystem::path epub_path;
    std::size_t search_revision = 0;
    QVariantList search_results;
    int search_state = 0;
    std::vector<books::Highlight> highlights;
    books::BookMetadata book;
    bool has_book = false;

    // The store must outlive the tracker that borrows it.
    std::unique_ptr<BookStatisticsStore> statistics_store;
    std::unique_ptr<ReadingTracker> tracker;
    bool statistics_enabled = false;
    bool reader_active = false;
    bool jump_pending = false;
    int statistics_today_characters = 0;
    int statistics_today_seconds = 0;
    int statistics_session_speed = 0;
    int chapter_index = 0;
    double chapter_progress = 0.0;
    int page_width = 800;
    int page_height = 600;
    int page_count = 0;
    int page_index = 0;
    int revision = 0;
    bool restoring = false;
    bool reveal_cue_after_restore = false;
    QString fragment;
    QString error;
    QString chapter_url;
    bool dark = false;
    bool system_dark = false;
    std::string background;
    std::string text;
    std::string info_color;
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

ReaderViewModel::ReaderViewModel()
    : state_(std::make_unique<State>([this] { reload_settings(true); })) {
    reload_settings(false);
    update_palette();
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
    return clamp_progress(static_cast<double>(characters) / state_->session->info().character_count);
}

double ReaderViewModel::chapter_progress() const { return state_->chapter_progress; }

int ReaderViewModel::page_count() const { return state_->page_count; }

int ReaderViewModel::page_index() const { return state_->page_index; }

bool ReaderViewModel::shows_page_count() const {
    return !state_->settings.continuous_mode && state_->settings.progress_count == "Pages";
}

QString ReaderViewModel::chapter_url() const { return state_->chapter_url; }

QVariantList ReaderViewModel::chapters() const {
    QVariantList result;
    if (!state_->session.has_value()) {
        return result;
    }
    for (const auto& entry : state_->session->contents()) {
        result.append(QVariantMap{{QStringLiteral("index"), entry.spine_index},
                                  {QStringLiteral("level"), entry.level},
                                  {QStringLiteral("title"), QString::fromStdString(entry.title)},
                                  {QStringLiteral("fragment"), QString::fromStdString(entry.fragment)}});
    }
    return result;
}

QVariantList ReaderViewModel::search_results() const { return state_->search_results; }

int ReaderViewModel::search_state() const { return state_->search_state; }

bool ReaderViewModel::hasSearchableText(const QString& query) const {
    return reader::has_searchable_text(query);
}

void ReaderViewModel::clearBookSearch() {
    ++state_->search_revision;
    state_->search_results.clear();
    state_->search_state = 0;
    emit searchChanged();
}

void ReaderViewModel::searchBook(const QString& query) {
    if (!state_->session || !hasSearchableText(query)) {
        clearBookSearch();
        return;
    }
    using Results = std::optional<std::vector<reader::SearchHit>>;
    const auto revision = ++state_->search_revision;
    const auto path = state_->epub_path;
    state_->search_results.clear();
    state_->search_state = 1;
    emit searchChanged();
    auto* watcher = new QFutureWatcher<Results>(this);
    connect(watcher, &QFutureWatcher<Results>::finished, this, [this, watcher, revision] {
        watcher->deleteLater();
        if (revision != state_->search_revision) {
            return;
        }
        const auto hits = watcher->result();
        state_->search_state = hits ? 2 : 3;
        if (hits) {
            std::size_t group_end = 0;
            for (std::size_t i = 0; i < hits->size(); ++i) {
                const auto& hit = hits->at(i);
                int group_count = 0;
                if (i == group_end) {
                    do {
                        ++group_end;
                    } while (group_end < hits->size() && hits->at(group_end).spine_index == hit.spine_index);
                    group_count = static_cast<int>(group_end - i);
                }
                state_->search_results.append(QVariantMap{
                    {QStringLiteral("groupCount"), group_count},
                    {QStringLiteral("character"), hit.character},
                    {QStringLiteral("label"), hit.label},
                    {QStringLiteral("before"), hit.snippet.left(hit.match_start).toHtmlEscaped()},
                    {QStringLiteral("match"), hit.snippet.mid(hit.match_start, hit.match_end - hit.match_start).toHtmlEscaped()},
                    {QStringLiteral("after"), hit.snippet.mid(hit.match_end).toHtmlEscaped()},
                });
            }
        }
        emit searchChanged();
    });
    QPromise<Results> promise;
    watcher->setFuture(promise.future());
    QThreadPool::globalInstance()->start([work = std::move(promise), path, query]() mutable {
        work.start();
        work.addResult(reader::search_epub(path, query));
        work.finish();
    });
}

void ReaderViewModel::jumpToCharacter(int character) {
    if (!state_->session) {
        return;
    }
    if (const auto position = state_->session->position_for_character(character)) {
        persist_bookmark();
        save_statistics();
        state_->jump_pending = true;
        load_chapter(position->first, position->second);
    }
}

QVariantList ReaderViewModel::highlights() const {
    QVariantList rows;
    if (!state_->session) {
        return rows;
    }
    const auto entries = books::highlights_in_range(
        state_->highlights, 0, state_->session->info().character_count);
    for (const auto& hit : entries) {
        const auto position = state_->session->position_for_character(hit.character);
        if (!position) {
            continue;
        }
        rows.append(QVariantMap{
            {QStringLiteral("id"), QString::fromStdString(hit.id)},
            {QStringLiteral("spine"), position->first},
            {QStringLiteral("label"), QString::fromStdString(state_->session->chapter_title(position->first))},
            {QStringLiteral("text"), QString::fromStdString(hit.text).trimmed()},
            {QStringLiteral("character"), hit.character},
            {QStringLiteral("color"), QString::fromStdString(hit.color)},
        });
    }
    return rows;
}

QString ReaderViewModel::highlightScript(const QString& color) const {
    if (!state_->session || state_->restoring || !books::is_highlight_color(color.toStdString())) {
        return {};
    }
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return QStringLiteral("(function() { const id = '%1';"
                          "const mark = window.hoshiHighlights?.createHighlight('%2', id);"
                          "return mark ? { ...mark, id } : null; })()").arg(id, color);
}

void ReaderViewModel::addHighlight(const QString& script_url, const QString& color,
                                   const QVariant& creation) {
    if (!state_->session || script_url != chapter_url() || !books::is_highlight_color(color.toStdString())) {
        return;
    }
    const auto data = creation.toMap();
    bool valid_start = false;
    bool valid_offset = false;
    const int start = data.value(QStringLiteral("start")).toInt(&valid_start);
    const int offset = data.value(QStringLiteral("offset")).toInt(&valid_offset);
    const auto id = data.value(QStringLiteral("id")).toString();
    const auto text = data.value(QStringLiteral("text")).toString();
    const int chapter_start = state_->session->character_offset(state_->chapter_index, 0);
    const int chapter_end = state_->session->character_offset(state_->chapter_index, 1);
    if (!valid_start || !valid_offset || start < 0 || start >= chapter_end - chapter_start ||
        offset < 0 || QUuid(id).isNull() || text.isEmpty()) {
        return;
    }
    auto highlights = state_->highlights;
    highlights.push_back({id.toStdString(), chapter_start + start, offset, text.toStdString(),
                          color.toStdString(), books::reference_now()});
    if (state_->storage.save_highlights(state_->book.folder, highlights)) {
        state_->highlights = std::move(highlights);
        emit highlightsChanged();
    } else {
        reload_document();
    }
}

void ReaderViewModel::removeHighlight(const QString& id) {
    if (!state_->session) {
        return;
    }
    auto highlights = state_->highlights;
    if (std::erase_if(highlights, [&id](const auto& entry) {
            return QString::fromStdString(entry.id) == id;
        }) != 0 && state_->storage.save_highlights(state_->book.folder, highlights)) {
        state_->highlights = std::move(highlights);
        emit highlightsChanged();
        reload_document();
    }
}

QString ReaderViewModel::error() const { return state_->error; }

bool ReaderViewModel::has_book() const { return state_->has_book; }

bool ReaderViewModel::dark() const { return state_->dark; }

void ReaderViewModel::setDark(bool dark) {
    if (state_->system_dark == dark) {
        return;
    }
    state_->system_dark = dark;
    update_palette();
    reload_document();
}

void ReaderViewModel::setReaderColors(const QString& background, const QString& text) {
    state_->background = reader::profile_color(background.toStdString());
    state_->text = reader::profile_color(text.toStdString());
    emit paletteChanged();
    reload_document();
}

QColor ReaderViewModel::page_background() const { return qcolor_from_css(state_->background); }

QColor ReaderViewModel::info_color() const { return qcolor_from_css(state_->info_color); }

void ReaderViewModel::update_palette() {
    const auto& settings = state_->settings;
    reader::ThemeInput input;
    input.theme = reader::theme_from_id(settings.theme);
    input.ui_theme = reader::theme_from_id(settings.ui_theme);
    input.system_dark = state_->system_dark;
    input.system_light_sepia = settings.system_light_sepia;
    input.custom_background = reader::profile_color(settings.custom_background_color);
    input.custom_text = reader::profile_color(settings.custom_text_color);
    const auto palette = reader::resolve_theme(input);
    state_->dark = palette.dark;
    state_->background = palette.background;
    state_->text = palette.text && !palette.text->empty() ? *palette.text
                                                        : reader::default_reader_text(palette.dark);
    state_->info_color = input.theme == reader::Theme::custom
                             ? reader::profile_color(settings.custom_info_color) : state_->text;
    emit paletteChanged();
}

bool ReaderViewModel::restoring() const { return state_->restoring; }

void ReaderViewModel::revealAfterLoadFailure(const QString& url) {
    if (url != chapter_url() || !state_->restoring) {
        return;
    }
    state_->restoring = false;
    state_->jump_pending = false;
    state_->reveal_cue_after_restore = false;
    emit restoreStateChanged();
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

bool ReaderViewModel::tracking() const {
    return state_->tracker && state_->tracker->is_tracking();
}

QVariantMap ReaderViewModel::reading_statistics() const {
    if (!state_->tracker || !state_->session) {
        return {};
    }
    const auto now = now_milliseconds();
    const auto session = state_->tracker->session_total();
    const auto today = state_->tracker->today_total(now);
    const auto all_time = state_->tracker->all_time_total();
    const bool english = state_->content_language == "en";
    const auto present = [english](const ReadingTotal& total) {
        const auto count = [english](int raw) {
            return english ? static_cast<int>(std::ceil(raw / 5.0)) : raw;
        };
        return QVariantMap{{QStringLiteral("count"), count(total.characters_read)},
                           {QStringLiteral("speed"), count(total.reading_speed())},
                           {QStringLiteral("time"), total.reading_time}};
    };
    return {{QStringLiteral("session"), present(session)},
            {QStringLiteral("today"), present(today)},
            {QStringLiteral("allTime"), present(all_time)},
            {QStringLiteral("english"), english}};
}

void ReaderViewModel::toggleTracking() {
    if (!state_->tracker) {
        return;
    }
    const auto now = now_milliseconds();
    state_->tracker->toggle(now);
    if (state_->tracker->is_tracking()) {
        state_->tracker->reset_position(
            state_->session->character_offset(state_->chapter_index, state_->chapter_progress), now);
    } else {
        save_statistics();
    }
    refresh_statistics();
    emit statisticsChanged();
}

void ReaderViewModel::tickTracking() {
    if (tracking()) {
        state_->tracker->flush(now_milliseconds());
        refresh_statistics();
        emit statisticsChanged();
    }
}

void ReaderViewModel::setReaderActive(bool active) {
    state_->reader_active = active;
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
    state_->highlights = state_->storage.load_highlights(state_->book.folder)
                             .value_or(std::vector<books::Highlight>{});
    clearBookSearch();
    state_->epub_path = *epub;
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
        state_->tracker->set_context_active(state_->reader_active, now_milliseconds());
    }

    int chapter = 0;
    double progress = 0.0;
    if (const auto bookmark = state_->storage.load_bookmark(folder_id)) {
        chapter = bookmark->chapter_index;
        progress = bookmark->progress;
    }
    state_->chapter_index = std::min(std::max(chapter, 0), state_->session->spine_count() - 1);
    state_->chapter_progress = clamp_progress(progress);
    state_->fragment.clear();
    state_->reveal_cue_after_restore = true;
    reload_settings(false);

    emit bookChanged();
    emit highlightsChanged();
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
    clearBookSearch();
    if (state_->tracker) {
        const std::int64_t now = now_milliseconds();
        state_->tracker->stop(now);
        state_->tracker->save(now, state_->book.display_title());
    }
    state_->tracker.reset();
    state_->statistics_store.reset();
    state_->statistics_enabled = false;
    state_->session.reset();
    state_->highlights.clear();
    state_->book = books::BookMetadata{};
    state_->has_book = false;
    state_->chapter_index = 0;
    state_->chapter_progress = 0.0;
    state_->restoring = false;
    state_->jump_pending = false;
    emit restoreStateChanged();
    state_->fragment.clear();
    state_->chapter_url.clear();
    set_pagination(0, 0);
    emit bookChanged();
    emit highlightsChanged();
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

void ReaderViewModel::goToContentsEntry(int index, const QString& fragment) {
    if (!state_->session || index < 0 || index >= state_->session->spine_count()) {
        return;
    }
    persist_bookmark();
    save_statistics();
    state_->jump_pending = true;
    load_chapter(index, 0.0, fragment);
}

void ReaderViewModel::goToProgress(double progress) {
    if (!state_->session.has_value()) {
        return;
    }
    jumpToCharacter(static_cast<int>(clamp_progress(progress) *
                                     state_->session->info().character_count));
}

QString ReaderViewModel::pageTurnScript(bool forward) const {
    if (!state_->has_book || state_->restoring) {
        return {};
    }
    return QStringLiteral(
               "(function() { window.hoshiParagraph?.finishTextAnimation(); return "
                "window.hoshiReader?.paginate?.('%1'); })()")
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
    if (!state_->session || state_->restoring || !std::isfinite(progress)) {
        return;
    }
    const double clamped = clamp_progress(progress);
    if (std::abs(clamped - state_->chapter_progress) < 1e-6) {
        return;
    }
    state_->chapter_progress = clamped;
    if (state_->tracker) {
        // The tracker works on the book's global character offset, as
        // statisticsCharacterPosition does. Restores bypass this path.
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
    if (!state_->session) {
        return;
    }
    const auto data = QJsonDocument::fromJson(payload.toUtf8()).object();
    if (data.value(QStringLiteral("token")).toString() != QString::number(state_->revision)) {
        return;
    }
    if (name == QStringLiteral("textSelected") && !state_->restoring) {
        const QString text = data.value(QStringLiteral("text")).toString();
        const auto rect = data.value(QStringLiteral("rect")).toObject();
        const double x = rect.value(QStringLiteral("x")).toDouble();
        const double y = rect.value(QStringLiteral("y")).toDouble();
        const double width = rect.value(QStringLiteral("width")).toDouble();
        const double height = rect.value(QStringLiteral("height")).toDouble();
        if (!text.trimmed().isEmpty() && width > 0 && height > 0) {
            emit lookupRequested(text, QRectF(x, y, width, height));
        }
    } else if (name == QStringLiteral("tapOutside") && !state_->restoring) {
        emit readerTappedOutside();
    } else if (name == QStringLiteral("wheelNavigation")) {
        const QString direction = data.value(QStringLiteral("direction")).toString();
        if (state_->restoring ||
            (direction != QStringLiteral("forward") && direction != QStringLiteral("backward"))) {
            return;
        }
        // The wheel is a manual page turn; Page Turn mode starts here, before
        // the page runs the navigation the signal asks for.
        if (state_->tracker) {
            state_->tracker->on_manual_navigation(now_milliseconds());
        }
        emit pageTurnRequested(direction);
    } else if (name == QStringLiteral("restoreCompleted") && state_->restoring) {
        state_->restoring = false;
        emit restoreStateChanged();
        if (!state_->fragment.isEmpty()) {
            state_->fragment.clear();
            // An explicit jump records the destination without counting it as read.
            state_->chapter_progress =
                clamp_progress(data.value(QStringLiteral("fragmentProgress")).toDouble());
            emit progressChanged();
        }
        if (std::exchange(state_->jump_pending, false)) {
            persist_bookmark();
            if (state_->tracker) {
                state_->tracker->reset_position(
                    state_->session->character_offset(state_->chapter_index, state_->chapter_progress),
                    now_milliseconds());
            }
        }
        const bool reveal = std::exchange(state_->reveal_cue_after_restore, false);
        emit documentRestored(reveal);
    } else if (name == QStringLiteral("progressChanged")) {
        updateChapterProgress(
            data.value(QStringLiteral("progress")).toDouble(state_->chapter_progress));
    } else if (name == QStringLiteral("pageChanged") && !state_->restoring) {
        const int count = data.value(QStringLiteral("pageCount")).toInt();
        const int index = data.value(QStringLiteral("pageIndex")).toInt();
        if (count > 0 && index >= 1 && index <= count) {
            set_pagination(index, count);
        }
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

void ReaderViewModel::load_chapter(int index, double progress, const QString& fragment) {
    state_->chapter_index = index;
    state_->chapter_progress = clamp_progress(progress);
    state_->fragment = fragment;
    state_->reveal_cue_after_restore = fragment.isEmpty();
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
    state_->restoring = true;
    const std::string path = state_->session->spine_path(state_->chapter_index);
    QUrl url(QStringLiteral("reader://book"));
    url.setPath(QLatin1Char('/') + QString::fromStdString(path));
    url.setQuery(QStringLiteral("v=%1").arg(state_->revision));
    state_->chapter_url = QString::fromUtf8(url.toEncoded());
    set_pagination(0, 0);
    emit restoreStateChanged();
    emit chapterUrlChanged();
}

void ReaderViewModel::reload_settings(bool reflow) {
    const auto settings = state_->settings_watch.read();
    const auto& lookup = state_->settings_watch.lookup_settings;
    const bool lookup_changed = state_->content_language != state_->settings_watch.content_language ||
                                state_->scan_length != lookup.scan_length ||
                                state_->scan_non_japanese != lookup.scan_non_japanese_text;
    state_->content_language = state_->settings_watch.content_language;
    state_->scan_length = lookup.scan_length;
    state_->scan_non_japanese = lookup.scan_non_japanese_text;
    bool reader_changed = false;
    if (settings && (!reflow || settings != state_->loaded_settings)) {
        state_->loaded_settings = settings;
        reader_changed = *settings != state_->settings;
        if (reader_changed) {
            state_->settings = *settings;
            update_palette();
            emit settingsChanged();
        }
    }
    if (reflow && (reader_changed || lookup_changed)) {
        state_->reveal_cue_after_restore = false;
        reload_document();
    }
}

void ReaderViewModel::set_pagination(int index, int count) {
    if (index == state_->page_index && count == state_->page_count) {
        return;
    }
    state_->page_index = index;
    state_->page_count = count;
    emit paginationChanged();
    if (count > 0) {
        emit pageChanged(index - 1);
    }
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
    input.scan_length = state_->scan_length;
    input.scan_non_japanese_text = state_->scan_non_japanese;
    input.content_language = state_->content_language;
    input.hover_delay_ms = 45;

    input.page = PageSize{state_->page_width, state_->page_height};
    input.colors.text = state_->text;
    input.colors.background = state_->background;
    input.colors.sasayaki_text = state_->text;
    input.colors.sasayaki_background = "#00000000";
    input.restore_token = std::to_string(state_->revision);
    const auto highlights = books::highlights_in_range(
        state_->highlights, state_->session->character_offset(state_->chapter_index, 0),
        state_->session->character_offset(state_->chapter_index, 1));
    if (!highlights.empty()) {
        auto json = books::highlights_to_json(highlights).toJson(QJsonDocument::Compact);
        json.replace("<", "\\u003c");
        input.highlights_json = json.toStdString();
    }
    if (!state_->fragment.isEmpty()) {
        input.initial_restore_expression =
            "window.hoshiReader.jumpToFragment(" + script_string(state_->fragment) + ")";
    } else {
        input.initial_restore_expression =
            "window.hoshiReader.restoreProgress(" +
            QString::number(state_->chapter_progress, 'g', 17).toStdString() + ")";
    }

    for (int index = 0; index < state_->session->spine_count(); ++index) {
        if (state_->session->spine_path(index) == path) {
            return state_->session->chapter_document(index, input, state_->assets);
        }
    }
    return {};
}

}  // namespace niratan::viewmodels

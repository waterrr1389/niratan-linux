// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_view_model.h"

#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPromise>
#include <QThreadPool>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "reader_view_model_state.h"
#include "models/sasayaki/sasayaki_settings.h"
#include "services/reader/book_search.h"
#include "services/reader/theme.h"
#include "services/settings/global_settings.h"

namespace niratan::viewmodels {
namespace {

using niratan::books::Bookmark;
using niratan::reader::BookSession;
using niratan::reader::BookStatisticsStore;
using niratan::reader::PageSize;
using niratan::reader::ShellInput;
using niratan::statistics::StatisticsAutostartMode;

double clamp_progress(double progress) { return std::min(std::max(progress, 0.0), 1.0); }

std::string script_string(const QString& value) {
    QByteArray array = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    // An EPUB identifier is data even when embedded in an inline script.
    array.replace("<", "\\u003c");
    return array.mid(1, array.size() - 2).toStdString();
}

QColor qcolor_from_css(const std::string& css) {
    QString color = QString::fromStdString(css);
    if (color.size() == 9) {
        color = QLatin1Char('#') + color.right(2) + color.mid(1, 6);
    }
    return QColor::fromString(color);
}

}  // namespace

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
    // The inspector's text follows the tracker, the position and the profile.
    connect(this, &ReaderViewModel::statisticsChanged, this,
            &ReaderViewModel::statisticsInspectorChanged);
    connect(this, &ReaderViewModel::progressChanged, this,
            &ReaderViewModel::statisticsInspectorChanged);
    connect(this, &ReaderViewModel::settingsChanged, this,
            &ReaderViewModel::statisticsInspectorChanged);
    // The overlay text follows the position, the pages, the settings and the totals.
    for (const auto signal :
         {&ReaderViewModel::progressChanged, &ReaderViewModel::paginationChanged,
          &ReaderViewModel::settingsChanged, &ReaderViewModel::statisticsChanged,
          &ReaderViewModel::bookChanged, &ReaderViewModel::chapterChanged}) {
        connect(this, signal, this, &ReaderViewModel::infoTextChanged);
    }
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
    return clamp_progress(static_cast<double>(character_offset()) / state_->session->info().character_count);
}

double ReaderViewModel::chapter_progress() const { return state_->chapter_progress; }

double ReaderViewModel::logical_chapter_progress() const {
    return state_->session
               ? state_->session->logical_chapter_progress(state_->chapter_index, chapter_progress())
               : 0.0;
}

int ReaderViewModel::character_offset() const {
    return state_->session ? state_->session->character_offset(state_->chapter_index, chapter_progress()) : 0;
}

int ReaderViewModel::chapter_characters_remaining() const {
    return state_->session
               ? state_->session->chapter_characters_remaining(state_->chapter_index, chapter_progress())
               : 0;
}

bool ReaderViewModel::shows_page_count() const {
    return !state_->settings.continuous_mode && state_->settings.progress_count == "Pages";
}

QString ReaderViewModel::progress_count_mode() const {
    const auto& settings = state_->settings;
    if (settings.progress_count == "Off" || settings.progress_count == "Pages" ||
        settings.progress_count == "Characters") {
        return QString::fromStdString(*settings.progress_count);
    }
    return settings.show_characters ? QStringLiteral("Characters") : QStringLiteral("Off");
}

QString ReaderViewModel::chapter_url() const { return state_->chapter_url; }

bool ReaderViewModel::show_title() const { return state_->settings.show_title; }
bool ReaderViewModel::show_progress() const { return state_->settings.show_progress.value_or(true); }
bool ReaderViewModel::show_percentage() const { return state_->settings.show_percentage; }
bool ReaderViewModel::show_progress_top() const { return state_->settings.show_progress_top; }
bool ReaderViewModel::show_chapter_progress() const {
    return state_->settings.show_chapter_progress.value_or(false);
}
bool ReaderViewModel::show_reading_speed() const { return state_->settings.show_reading_speed; }
bool ReaderViewModel::show_reading_time() const { return state_->settings.show_reading_time; }
bool ReaderViewModel::show_sasayaki_toggle() const { return state_->settings.show_sasayaki_toggle; }
bool ReaderViewModel::show_statistics_toggle() const { return state_->settings.show_statistics_toggle; }
bool ReaderViewModel::popup_action_bar() const { return state_->settings.popup_action_bar; }
bool ReaderViewModel::popup_disable_transparency() const { return state_->settings.popup_disable_transparency; }
bool ReaderViewModel::popup_full_width() const { return state_->settings.popup_full_width; }
int ReaderViewModel::popup_height() const { return state_->settings.popup_height; }
double ReaderViewModel::popup_scale() const { return state_->settings.popup_scale; }
bool ReaderViewModel::popup_swipe_to_dismiss() const { return state_->settings.popup_swipe_to_dismiss; }
int ReaderViewModel::popup_swipe_threshold() const { return state_->settings.popup_swipe_threshold; }
int ReaderViewModel::popup_width() const { return state_->settings.popup_width; }
QString ReaderViewModel::interface_theme() const {
    const auto& settings = state_->settings;
    const std::string& scheme = settings.theme == "Custom" ? settings.ui_theme : settings.theme;
    if (scheme == "Light" || scheme == "Dark") {
        return QString::fromStdString(scheme);
    }
    return QStringLiteral("System");
}
bool ReaderViewModel::english_content() const { return state_->content_language == "en"; }

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
bool ReaderViewModel::uses_custom_info_color() const { return state_->settings.theme == "Custom"; }

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
    reload_document(true);
}

int ReaderViewModel::font_size() const { return state_->settings.font_size; }

void ReaderViewModel::setFontSize(int size) {
    const int clamped = std::clamp(size, 16, 40);
    if (state_->settings.font_size == clamped) {
        return;
    }
    state_->settings.font_size = clamped;
    emit settingsChanged();
    reload_document(true);
}

int ReaderViewModel::horizontal_padding() const { return state_->settings.horizontal_padding; }

void ReaderViewModel::setHorizontalPadding(int padding) {
    const int clamped = std::clamp(padding, 0, 80);
    if (state_->settings.horizontal_padding == clamped) {
        return;
    }
    state_->settings.horizontal_padding = clamped;
    emit settingsChanged();
    reload_document(true);
}

int ReaderViewModel::vertical_padding() const { return state_->settings.vertical_padding; }

void ReaderViewModel::setVerticalPadding(int padding) {
    const int clamped = std::clamp(padding, 0, 50);
    if (state_->settings.vertical_padding == clamped) {
        return;
    }
    state_->settings.vertical_padding = clamped;
    emit settingsChanged();
    reload_document(true);
}

bool ReaderViewModel::importAndOpen(const QUrl& file) {
    const books::ImportResult imported = state_->storage.import_book(
        file.toLocalFile().toStdString());
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
    clear_gallery();
    state_->epub_path = epub->string();
    state_->session = std::move(session);
    state_->has_book = true;
    state_->error.clear();
    emit errorChanged();

    // Statistics is configured for this book, after the session is open so a
    // failed open leaves no tracker behind. The settings file is read on every
    // open; the reader never writes it.
    const settings::StatisticsPreferences statistics_settings =
        settings::GlobalSettings(state_->storage.app_directory() / "settings.json")
            .statistics_preferences()
            .preferences;
    state_->statistics_enabled = statistics_settings.enabled;
    // Replacing an open book must not leave a tracker borrowing a destroyed
    // store; the tracker goes first.
    state_->tracker.reset();
    state_->statistics_store.reset();
    if (const auto book_root = state_->storage.book_directory(folder_id);
        book_root.has_value()) {
        state_->statistics_store = std::make_unique<BookStatisticsStore>(*book_root);
        state_->tracker = std::make_unique<reader::ReadingTracker>(
            *state_->statistics_store, QTimeZone(QTimeZone::LocalTime));
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
    if (state_->tracker) {
        // startTracking's resetTrackingBaseline anchors at the position the
        // book opens at, so the first report credits the jump from it.
        state_->tracker->update_position(character_offset(), now_milliseconds());
    }
    state_->fragment.clear();
    state_->reveal_cue_after_restore = true;
    reload_settings(false);

    emit bookChanged();
    emit highlightsChanged();
    reload_document(true);
    emit chapterChanged();
    emit progressChanged();
    refresh_statistics(true);
    return true;
}

void ReaderViewModel::closeBook() {
    if (!state_->has_book) {
        return;
    }
    clear_gallery();
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
    state_->spine_page_counts.clear();
    state_->spine_page_starts.clear();
    set_pagination(0, 0);
    emit bookChanged();
    emit highlightsChanged();
    emit chapterChanged();
    emit progressChanged();
    emit chapterUrlChanged();
    refresh_statistics(true);
}

void ReaderViewModel::setPageSize(int width, int height) {
    const int new_width = std::max(width, 1);
    const int new_height = std::max(height, 1);
    if (state_->page_width == new_width && state_->page_height == new_height) {
        return;
    }
    state_->page_width = new_width;
    state_->page_height = new_height;
    reload_document(true);
}

QString ReaderViewModel::pageTurnScript(bool forward) const {
    if (!state_->has_book || state_->restoring) {
        return {};
    }
    const auto& settings = state_->settings;
    QString reveal;
    if (forward && !settings.continuous_mode && settings.paragraph_mode.value_or(false) &&
        settings.text_animation.value_or(false)) {
        reveal = QStringLiteral("if (result === 'scrolled') window.hoshiParagraph?.animateText(%1);")
                     .arg(settings.text_speed.value_or(35));
    }
    return QStringLiteral("(function() { window.hoshiParagraph?.finishTextAnimation(); "
                          "const result = window.hoshiReader?.paginate?.('%1'); %2 return result; })()")
        .arg(forward ? QStringLiteral("forward") : QStringLiteral("backward"), reveal);
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
        handle_text_selected(data);
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
        start_tracking_on_navigation();
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
    } else if (name == QStringLiteral("progressChanged") && !state_->restoring &&
               state_->settings.continuous_mode) {
        const double progress = data.value(QStringLiteral("progress")).toDouble(state_->chapter_progress);
        if (std::abs(clamp_progress(progress) - state_->chapter_progress) >= 1e-6) {
            start_tracking_on_navigation();
            updateChapterProgress(progress);
        }
    } else if (name == QStringLiteral("pageChanged") && !state_->restoring) {
        const int count = data.value(QStringLiteral("pageCount")).toInt();
        const int index = data.value(QStringLiteral("pageIndex")).toInt();
        if (count > 0 && index >= 1 && index <= count) {
            set_pagination(index, count);
        }
    } else if (name == QStringLiteral("pageStarts")) {
        const QJsonArray values = data.value(QStringLiteral("starts")).toArray();
        std::vector<int> starts;
        starts.reserve(static_cast<std::size_t>(values.size()));
        for (const auto& value : values) {
            starts.push_back(value.toInt());
        }
        const int spine = state_->chapter_index;
        if (!starts.empty() && spine >= 0 &&
            static_cast<std::size_t>(spine) < state_->spine_page_starts.size()) {
            state_->spine_page_starts[static_cast<std::size_t>(spine)] = std::move(starts);
            emit paginationChanged();
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

void ReaderViewModel::reload_document(bool reflow) {
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
    if (reflow) {
        state_->spine_page_counts.assign(state_->session->spine_count(), 0);
        state_->spine_page_starts.assign(state_->session->spine_count(), {});
    }
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
        }
    }
    if (reader_changed || lookup_changed) {
        emit settingsChanged();
    }
    if (reflow && (reader_changed || lookup_changed)) {
        state_->reveal_cue_after_restore = false;
        reload_document(reader_changed);
    }
}

void ReaderViewModel::persist_bookmark() {
    if (!state_->session.has_value()) {
        return;
    }
    Bookmark bookmark;
    bookmark.chapter_index = state_->chapter_index;
    bookmark.progress = state_->chapter_progress;
    bookmark.character_count = character_offset();
    const auto previous = state_->storage.load_bookmark(state_->book.folder);
    bookmark.last_modified = previous && previous->character_count == bookmark.character_count
                                 ? previous->last_modified : std::optional{books::reference_now()};
    state_->storage.save_bookmark(state_->book.folder, bookmark);
}

std::string ReaderViewModel::shell_document(const std::string& path) {
    if (!state_->session.has_value()) {
        return {};
    }
    ShellInput input;
    input.measure_page_starts = !state_->settings.continuous_mode &&
        state_->settings.progress_count.value_or("Characters") == "Pages" &&
        (show_progress() || show_chapter_progress());
    input.appearance.vertical_writing = state_->settings.vertical_writing;
    input.appearance.continuous = state_->settings.continuous_mode;
    input.wheel_page_turn = state_->settings.wheel_page_turn;
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
    input.appearance.furigana_mode =
        state_->settings.furigana_mode.value_or(state_->settings.hide_furigana ? "Hidden" : "Off");
    input.scan_length = state_->scan_length;
    input.scan_non_japanese_text = state_->scan_non_japanese;
    input.content_language = state_->content_language;
    input.hover_delay_ms = state_->settings.desktop_lookup_hover_delay_ms;

    input.page = PageSize{state_->page_width, state_->page_height};
    // The cue colours are user preferences in the global settings document,
    // read per document build; the reader never writes the file.
    const sasayaki::SasayakiColors cue_colors = sasayaki::sasayaki_colors(
        settings::GlobalSettings(state_->storage.app_directory() / "settings.json")
            .sasayaki_settings(),
        state_->dark);
    input.colors.text = state_->text;
    input.colors.background = state_->background;
    input.colors.sasayaki_text = cue_colors.text;
    input.colors.sasayaki_background = cue_colors.background;
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

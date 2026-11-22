// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video library view model's state, persisted appearance settings,
// playback-history cache and the empty-state copy the page shows. The commands
// live in video_library_commands.cpp and the row projection in
// video_library_rows.cpp / video_library_sections.cpp.

#include "viewmodels/video/video_library_view_model.h"

#include "services/player/video_settings.h"

#include <algorithm>

namespace niratan::viewmodels {
namespace {

constexpr double kMinimumPosterWidth = 110;
constexpr double kMaximumPosterWidth = 240;
constexpr double kPosterAspect = 1.45;

double clamped_poster(double width) {
    return std::clamp(width, kMinimumPosterWidth, kMaximumPosterWidth);
}

bool is_filtering(const QString& search_text, bool unfinished_only) {
    return !search_text.trimmed().isEmpty() || unfinished_only;
}

}  // namespace

VideoLibraryViewModel::VideoLibraryViewModel(QObject* parent)
    : QObject(parent), store_(), history_(player::PlaybackHistory::at_data_root()) {
    const auto settings = player::load_video_library_settings();
    layout_mode_ = settings.value(QStringLiteral("videoLibraryLayoutMode")).toString();
    if (layout_mode_ != QStringLiteral("list")) {
        layout_mode_ = QStringLiteral("posters");
    }
    poster_size_ =
        clamped_poster(settings.value(QStringLiteral("videoLibraryPosterWidth")).toDouble());
    rebuildPlaybackHistoryCache();
}

VideoLibraryViewModel::~VideoLibraryViewModel() = default;

void VideoLibraryViewModel::setDisplayMode(const QString& mode) {
    if (display_mode_ == mode) {
        return;
    }
    display_mode_ = mode;
    emit displayModeChanged();
    notifyRows();
}

void VideoLibraryViewModel::setSortOption(const QString& option) {
    if (sort_option_ == option) {
        return;
    }
    sort_option_ = option;
    emit sortOptionChanged();
    notifyRows();
}

void VideoLibraryViewModel::setLayoutMode(const QString& mode) {
    const QString next = mode == QStringLiteral("list") ? mode : QStringLiteral("posters");
    if (layout_mode_ == next) {
        return;
    }
    layout_mode_ = next;
    player::store_video_library_settings({{QStringLiteral("videoLibraryLayoutMode"), next}});
    emit layoutModeChanged();
}

void VideoLibraryViewModel::setPosterSize(double width) {
    const double next = clamped_poster(width);
    if (poster_size_ == next) {
        return;
    }
    poster_size_ = next;
    player::store_video_library_settings({{QStringLiteral("videoLibraryPosterWidth"), next}});
    emit posterSizeChanged();
}

double VideoLibraryViewModel::posterWidth() const {
    return clamped_poster(poster_size_) * kPosterAspect;
}

void VideoLibraryViewModel::setSearchText(const QString& text) {
    if (search_text_ == text) {
        return;
    }
    search_text_ = text;
    emit searchTextChanged();
    notifyRows();
}

void VideoLibraryViewModel::setShowUnfinishedOnly(bool enabled) {
    if (show_unfinished_only_ == enabled) {
        return;
    }
    show_unfinished_only_ = enabled;
    emit showUnfinishedOnlyChanged();
    notifyRows();
}

bool VideoLibraryViewModel::hasSources() const {
    return !store_.catalog().sources.isEmpty();
}

QString VideoLibraryViewModel::emptyTitle() const {
    if (is_filtering(search_text_, show_unfinished_only_)) {
        return tr("No Matching Videos");
    }
    if (display_mode_ == QStringLiteral("continueWatching")) {
        return tr("No Videos in Progress");
    }
    if (display_mode_ == QStringLiteral("favorites")) {
        return tr("No Favorite Videos");
    }
    if (display_mode_ == QStringLiteral("unwatched")) {
        return tr("No Unwatched Videos");
    }
    if (display_mode_ == QStringLiteral("finished")) {
        return tr("No Finished Videos");
    }
    if (display_mode_ == QStringLiteral("missing")) {
        return tr("No Missing Videos");
    }
    if (display_mode_ == QStringLiteral("recent")) {
        return tr("No Recent Videos");
    }
    if (display_mode_ == QStringLiteral("needsReview")) {
        return tr("No Videos Need Review");
    }
    return tr("No Videos");
}

QString VideoLibraryViewModel::emptyDescription() const {
    if (is_filtering(search_text_, show_unfinished_only_)) {
        return tr("Try a different search or filter.");
    }
    if (display_mode_ == QStringLiteral("continueWatching")) {
        return tr("Partially watched videos will appear here.");
    }
    if (display_mode_ == QStringLiteral("favorites")) {
        return tr("Favorite videos will appear here.");
    }
    if (display_mode_ == QStringLiteral("unwatched")) {
        return tr("Videos without playback progress will appear here.");
    }
    if (display_mode_ == QStringLiteral("finished")) {
        return tr("Videos marked watched will appear here.");
    }
    if (display_mode_ == QStringLiteral("missing")) {
        return tr("Missing videos will appear here until their source is refreshed.");
    }
    if (display_mode_ == QStringLiteral("recent")) {
        return tr("Played videos will appear here.");
    }
    if (display_mode_ == QStringLiteral("needsReview")) {
        return tr("Videos outside manual and smart collections will appear here.");
    }
    return tr("Refresh your folders or add another source.");
}

QVariantList VideoLibraryViewModel::sections() const {
    return sectionsData();
}

QVariantMap VideoLibraryViewModel::modeCounts() const {
    return modeCountsData();
}

QVariantList VideoLibraryViewModel::sourceSummaries() const {
    return sourceSummariesData();
}

void VideoLibraryViewModel::load() {
    refreshPlaybackHistory();
    notifyRows();
}

void VideoLibraryViewModel::refreshPlaybackHistory() {
    const auto previous_states = playback_states_;
    const auto previous_keys = cached_playback_keys_;
    rebuildPlaybackHistoryCache();
    if (previous_states != playback_states_ || previous_keys != cached_playback_keys_) {
        ++playback_history_revision_;
        emit playbackHistoryRevisionChanged();
        notifyRows();
    }
}

void VideoLibraryViewModel::rebuildPlaybackHistoryCache() {
    QHash<QString, player::PlaybackState> states;
    QSet<QString> keys;
    for (const auto& item : store_.catalog().items) {
        keys.insert(item.path);
        if (const auto state = history_.state(item.path)) {
            states.insert(item.path, *state);
        }
    }
    playback_states_ = states;
    cached_playback_keys_ = keys;
}

void VideoLibraryViewModel::synchronizePlaybackHistoryCache(const QStringList& keys, bool force) {
    const QSet<QString> key_set(keys.begin(), keys.end());
    if (!force && key_set == cached_playback_keys_) {
        return;
    }
    QHash<QString, player::PlaybackState> states;
    for (const auto& key : keys) {
        if (const auto state = history_.state(key)) {
            states.insert(key, *state);
        }
    }
    playback_states_ = states;
    cached_playback_keys_ = key_set;
}

void VideoLibraryViewModel::notifyRows() {
    emit sectionsChanged();
    emit modeCountsChanged();
    emit sourceSummariesChanged();
    emit emptyTitleChanged();
    emit emptyDescriptionChanged();
    emit hasSourcesChanged();
}

void VideoLibraryViewModel::showError(const QString& message) {
    error_message_ = message;
    emit errorMessageChanged();
}

}  // namespace niratan::viewmodels

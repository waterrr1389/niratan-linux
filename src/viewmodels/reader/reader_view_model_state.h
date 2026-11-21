// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "reader_view_model.h"

#include <QDateTime>

#include <functional>
#include <set>
#include <utility>
#include <vector>

#include "models/profiles/reader_settings.h"
#include "services/audio/local_file_server.h"
#include "services/books/book_storage.h"
#include "services/reader/reading_tracker.h"
#include "services/storage/app_paths.h"

namespace niratan::viewmodels {

inline std::int64_t now_milliseconds() { return QDateTime::currentMSecsSinceEpoch(); }

struct ReaderViewModel::State {
    explicit State(std::function<void()> on_settings_changed)
        : storage(storage::data_root()),
          settings_watch(storage.app_directory(), std::move(on_settings_changed)),
          assets(reader::load_reader_scripts()) {}

    books::BookStorage storage;
    reader::ReaderSettingsWatch settings_watch;
    reader::ReaderScripts assets;
    profiles::ReaderProfileSettings settings = profiles::default_reader_settings();
    std::optional<profiles::ReaderProfileSettings> loaded_settings;
    std::string content_language = "ja";
    int scan_length = 16;
    bool scan_non_japanese = true;
    std::optional<reader::BookSession> session;
    std::string epub_path;
    std::size_t search_revision = 0;
    QVariantList search_results;
    int search_state = 0;
    QVariantList gallery_images;
    std::set<std::string, std::less<>> gallery_paths;
    bool gallery_loading = false;
    std::size_t gallery_revision = 0;
    audio::LocalFileServer gallery_server{{}, 0};
    std::vector<books::Highlight> highlights;
    books::BookMetadata book;
    bool has_book = false;

    // The store must outlive the tracker that borrows it.
    std::unique_ptr<reader::BookStatisticsStore> statistics_store;
    std::unique_ptr<reader::ReadingTracker> tracker;
    bool statistics_enabled = false;
    bool reader_active = false;
    bool jump_pending = false;
    int statistics_today_characters = 0;
    int statistics_today_seconds = 0;
    int statistics_session_speed = 0;
    int statistics_session_minutes = 0;
    bool tracking = false;
    int chapter_index = 0;
    double chapter_progress = 0.0;
    int page_width = 800;
    int page_height = 600;
    int page_count = 0;
    int page_index = 0;
    std::vector<int> spine_page_counts;
    int spine_page_offset = 0;
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

}  // namespace niratan::viewmodels

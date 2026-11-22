// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader overlays' text, following NativeMac/NativeReaderView.swift
// (progressString, statisticsString, NativeReaderModel.pageProgress) and
// Features/Reader/ReaderPageIndex.swift (progress). reader_presentation.h
// owns the wording; this file supplies the values.
//
// ReaderPageIndex locates the table-of-contents chapter by the character each
// measured page starts at. The web view measures those starts on the unscrolled
// chapter and the bridge keeps them per spine, so a chapter boundary inside a
// spine item is placed exactly. Until a spine's starts arrive, the count-only
// measurement still covers a chapter that begins and ends on spine boundaries;
// any other unmeasured range shows "…", as before measurement.

#include "reader_view_model_state.h"

#include <QVariantMap>

#include <algorithm>
#include <numeric>

#include "reader_presentation.h"

namespace niratan::viewmodels {

namespace presentation = reader_presentation;

namespace {

// ReaderPageIndex: the global first page of each spine from the measured page
// counts, or an empty vector when no page is measured.
std::vector<int> spine_first_pages(const std::vector<int>& counts) {
    std::vector<int> first_pages(counts.size(), 0);
    int running = 0;
    for (std::size_t spine = 0; spine < counts.size(); ++spine) {
        first_pages[spine] = running;
        running += counts[spine];
    }
    return first_pages;
}

// ReaderPageIndex.progress with the measured page starts: the chapter's first
// page is the page starting exactly at the chapter's first character, else the
// last page starting before it; its last page is the last page starting before
// the chapter end, and the book end includes the final page.
std::optional<presentation::PageProgress>
exact_page_progress(const reader::BookSession& session, const std::vector<std::vector<int>>& starts,
                    const std::vector<int>& counts, int page, int chapter_start,
                    int chapter_count) {
    const int total = std::accumulate(counts.begin(), counts.end(), 0);
    const int spines = std::min<int>(session.spine_count(), static_cast<int>(counts.size()));
    if (total <= 0 || page <= 0 || spines <= 0) {
        return std::nullopt;
    }
    const int chapter_end = chapter_start + chapter_count;
    const int book_end = session.info().character_count;
    const std::vector<int> first_pages = spine_first_pages(counts);
    const int current = std::clamp(page, 1, total) - 1;

    int first_spine = -1;
    int first_start = 0;
    for (int spine = 0; spine < spines; ++spine) {
        const int start = session.character_offset(spine, 0.0);
        const int end = session.character_offset(spine, 1.0);
        if (chapter_start >= start &&
            (chapter_start < end || chapter_start == start ||
             (spine == spines - 1 && chapter_start < book_end))) {
            first_spine = spine;
            first_start = start;
            break;
        }
    }
    if (first_spine < 0) {
        return std::nullopt;
    }
    const auto& local_starts = starts[static_cast<std::size_t>(first_spine)];
    if (local_starts.empty() ||
        local_starts.size() != static_cast<std::size_t>(counts[first_spine])) {
        return std::nullopt;
    }
    const int local_character = chapter_start - first_start;
    int first_local = -1;
    for (int index = 0; index < static_cast<int>(local_starts.size()); ++index) {
        if (local_starts[index] == local_character) {
            first_local = index;
            break;
        }
    }
    if (first_local < 0) {
        for (int index = 0; index < static_cast<int>(local_starts.size()); ++index) {
            if (local_starts[index] < local_character) {
                first_local = index;
            }
        }
    }
    if (first_local < 0) {
        first_local = 0;
    }
    const int chapter_first = first_pages[first_spine] + first_local;

    int chapter_last = chapter_first;
    if (chapter_end >= book_end) {
        chapter_last = total - 1;
    } else {
        bool ending_measured = false;
        for (int spine = 0; spine < spines; ++spine) {
            const int start = session.character_offset(spine, 0.0);
            if (start >= chapter_end || start >= book_end) {
                continue;
            }
            const auto& spine_starts = starts[static_cast<std::size_t>(spine)];
            if (spine_starts.empty() ||
                spine_starts.size() != static_cast<std::size_t>(counts[spine])) {
                continue;
            }
            const int limit = chapter_end - start;
            for (int index = 0; index < static_cast<int>(spine_starts.size()); ++index) {
                if (spine_starts[index] < limit) {
                    chapter_last = std::max(chapter_last, first_pages[spine] + index);
                }
            }
            if (start <= chapter_end - 1 &&
                chapter_end - 1 < session.character_offset(spine, 1.0)) {
                ending_measured = true;
            }
        }
        if (!ending_measured) {
            return std::nullopt;
        }
    }
    const int chapter_total = chapter_last - chapter_first + 1;
    if (chapter_total <= 0) {
        return std::nullopt;
    }
    return presentation::PageProgress{
        current + 1,
        total,
        std::clamp(current - chapter_first + 1, 1, chapter_total),
        chapter_total,
    };
}

// The count-only measurement for a chapter that begins and ends exactly on
// spine boundaries, kept for a bridge that has not reported page starts.
std::optional<presentation::PageProgress> count_page_progress(const reader::BookSession& session,
                                                              const std::vector<int>& counts,
                                                              int page, int chapter_start,
                                                              int chapter_count) {
    const int total = std::accumulate(counts.begin(), counts.end(), 0);
    if (total <= 0 || page <= 0) {
        return std::nullopt;
    }
    const int spines = std::min<int>(session.spine_count(), static_cast<int>(counts.size()));
    const int book_end = session.info().character_count;
    const int chapter_end = chapter_start + chapter_count;
    int first = -1;
    int last = -1;
    for (int spine = 0; spine < spines; ++spine) {
        const int start = session.character_offset(spine, 0.0);
        if (first < 0 && start == chapter_start) {
            first = spine;
        }
        if (first >= 0 && start < chapter_end) {
            last = spine;
        }
    }
    if (first < 0 || last < first) {
        return std::nullopt;
    }
    if (chapter_end < book_end && session.character_offset(last, 1.0) != chapter_end) {
        return std::nullopt;
    }
    if (chapter_end >= book_end) {
        last = spines - 1;
    }
    const int first_page = std::accumulate(counts.begin(), counts.begin() + first, 0);
    const int chapter_total = std::accumulate(counts.begin() + first, counts.begin() + last + 1, 0);
    if (chapter_total <= 0) {
        return std::nullopt;
    }
    const int clamped_page = std::clamp(page, 1, total);
    return presentation::PageProgress{
        clamped_page,
        total,
        std::clamp(clamped_page - first_page, 1, chapter_total),
        chapter_total,
    };
}

}  // namespace

QVariantList ReaderViewModel::chapters() const {
    QVariantList result;
    if (!state_->session) {
        return result;
    }
    for (const auto& entry : state_->session->contents()) {
        const int start = state_->session->contents_character_start(entry).value_or(-1);
        result.append(QVariantMap{
            {QStringLiteral("index"), entry.spine_index},
            {QStringLiteral("characterStart"), start},
            {QStringLiteral("level"), entry.level},
            {QStringLiteral("title"), QString::fromStdString(entry.title)},
            {QStringLiteral("fragment"), QString::fromStdString(entry.fragment)},
        });
    }
    return result;
}

QString ReaderViewModel::progress_text() const {
    if (!state_->session) {
        return {};
    }
    const auto& session = *state_->session;
    presentation::ProgressInput input;
    input.line.shows_page_count = shows_page_count();
    input.line.count_off = progress_count_mode() == QStringLiteral("Off");
    input.line.show_percentage = show_percentage();
    input.line.english = english_content();
    input.show_book = show_progress();
    input.show_chapter = show_chapter_progress();
    input.top = show_progress_top();
    input.book_current = character_offset();
    input.book_total = session.info().character_count;
    const auto [local, count] =
        session.logical_chapter_position(state_->chapter_index, state_->chapter_progress);
    input.chapter_current = local;
    input.chapter_total = count;
    if (input.line.shows_page_count && state_->page_count > 0) {
        const int chapter_start = input.book_current - local;
        input.pages =
            exact_page_progress(session, state_->spine_page_starts, state_->spine_page_counts,
                                page_index(), chapter_start, count);
        if (!input.pages) {
            input.pages = count_page_progress(session, state_->spine_page_counts, page_index(),
                                              chapter_start, count);
        }
    }
    return presentation::progress_string(input);
}

QString ReaderViewModel::statistics_text() const {
    if (!state_->session) {
        return {};
    }
    return presentation::statistics_string(state_->statistics_enabled, show_reading_speed(),
                                           show_reading_time(), state_->statistics_session_speed,
                                           state_->statistics_session_seconds, english_content());
}

QString ReaderViewModel::position_text() const {
    return state_->session ? positionTextAt(character_offset()) : QString();
}

QString ReaderViewModel::positionTextAt(int rawCharacter) const {
    if (!state_->session) {
        return {};
    }
    return presentation::position_text(rawCharacter, state_->session->info().character_count,
                                       english_content());
}

bool ReaderViewModel::jumpToDisplayCount(const QString& input) {
    const auto count = presentation::parse_jump_count(input);
    if (!count || !state_->session) {
        return false;
    }
    jumpToCharacter(presentation::raw_characters(*count, english_content()));
    return true;
}

}  // namespace niratan::viewmodels

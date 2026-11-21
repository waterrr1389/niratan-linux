// SPDX-License-Identifier: GPL-3.0-or-later
//
// The characters left in the table-of-contents chapter that holds the open
// position, following NativeMac/NativeReaderView.swift
// (displayedProgressChapterRange, displayedChapterCharactersRemaining) and
// Features/Reader/ReaderChapterIndex.swift (chapterStarts, chapterRange,
// ChapterRange.remaining).
//
// The method belongs to BookSession but lives here because the session's
// source file is at its size limit. Starts are 0 plus every contents entry,
// shifted by that entry's fragment offset and kept inside the chapter that
// owns the entry. The lookup is kept inside the open spine item, so the end of
// a chapter reports zero rather than borrowing the next chapter's count.

#include "book_session.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace niratan::reader {

std::optional<int> BookSession::contents_character_start(const ContentsEntry& entry) const {
    const auto chapter = info_.chapter_info.find(spine_path(entry.spine_index));
    if (chapter == info_.chapter_info.end()) {
        return std::nullopt;
    }
    int offset = 0;
    if (!entry.fragment.empty() && chapter->second.fragment_offsets.has_value()) {
        const auto found = chapter->second.fragment_offsets->find(entry.fragment);
        if (found != chapter->second.fragment_offsets->end()) {
            offset = found->second;
        }
    }
    return chapter->second.current_total + std::clamp(offset, 0, chapter->second.chapter_count);
}

std::pair<int, int> BookSession::logical_chapter_position(int spine_index, double progress) const {
    if (info_.character_count <= 0) {
        return {0, 0};
    }
    const int character = character_offset(spine_index, progress);

    std::set<int> starts{0};
    for (const ContentsEntry& entry : contents_) {
        if (const auto start = contents_character_start(entry);
            start && *start >= 0 && *start <= info_.character_count) {
            starts.insert(*start);
        }
    }
    std::vector<int> normalized;
    for (const int start : starts) {
        if (start < info_.character_count) {
            normalized.push_back(start);
        }
    }

    const auto spine_chapter = info_.chapter_info.find(spine_path(spine_index));
    const int spine_end = spine_chapter == info_.chapter_info.end()
                              ? 0
                              : spine_chapter->second.current_total +
                                    spine_chapter->second.chapter_count;
    const int lookup = spine_end > 0 ? std::min(character, spine_end - 1) : character;

    const std::size_t next_index = static_cast<std::size_t>(std::distance(
        normalized.begin(),
        std::upper_bound(normalized.begin(), normalized.end(),
                         std::min(std::max(lookup, 0), info_.character_count - 1))));
    const int start = normalized[next_index > 0 ? next_index - 1 : 0];
    const int end =
        next_index < normalized.size() ? normalized[next_index] : info_.character_count;
    const int count = std::max(end - start, 0);
    const int local = std::min(std::max(character - start, 0), count);
    return {local, count};
}

int BookSession::chapter_characters_remaining(int spine_index, double progress) const {
    const auto [local, count] = logical_chapter_position(spine_index, progress);
    return count - local;
}

double BookSession::logical_chapter_progress(int spine_index, double progress) const {
    const auto [local, count] = logical_chapter_position(spine_index, progress);
    return count > 0 ? static_cast<double>(local) / count : 0.0;
}

}  // namespace niratan::reader

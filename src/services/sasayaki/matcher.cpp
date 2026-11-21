// SPDX-License-Identifier: GPL-3.0-or-later

#include "matcher.h"

#include "services/reader/character_normalizer.h"

#include <QChar>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <optional>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace niratan::sasayaki {
namespace {

using Index = std::ptrdiff_t;
using Text = std::u32string_view;

struct Range {
    Index begin;
    Index end;

    Index size() const { return end - begin; }
    bool overlaps(Range other) const { return begin < other.end && other.begin < end; }
    bool operator==(const Range&) const = default;
};

struct Chapter {
    int index;
    Range range;
};

struct Alignment {
    Range range;
    Index edits;
};

struct Placement {
    Range range;
    bool inferred = false;
};

class Cancellation {
public:
    explicit Cancellation(const std::function<bool()>& callback) : callback_(callback) {}

    bool poll() {
        return stopped_ || (stopped_ = callback_ && callback_());
    }

    bool stopped() const { return stopped_; }

private:
    const std::function<bool()>& callback_;
    bool stopped_ = false;
};

std::u32string scalars(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())).toStdU32String();
}

void append_folded(std::u32string& result, char32_t scalar) {
    if (scalar < 0x80) {
        result.push_back(QChar::toCaseFolded(scalar));
        return;
    }
    // Width decomposition must not normalize ideographs or other compatibility forms.
    const auto tag = QChar::decompositionTag(scalar);
    const auto width_folded = tag == QChar::Wide || tag == QChar::Narrow
        ? QChar::decomposition(scalar) : QString::fromUcs4(&scalar, 1);
    for (char32_t c : width_folded.toCaseFolded().toStdU32String()) {
        result.push_back(c >= 0x30A1 && c <= 0x30F6 ? c - 0x60 : c);
    }
}

std::u32string folded_cue(const SasayakiCue& cue) {
    std::u32string result;
    for (const auto scalar : scalars(reader::filtered_text(cue.text))) {
        append_folded(result, scalar);
    }
    return result;
}

void append_pieces(std::vector<SasayakiCue>& pieces, const SasayakiCue& cue) {
    if (reader::readable_character_count(cue.text) <= 120) {
        pieces.push_back(cue);
        return;
    }

    const auto raw = scalars(cue.text);
    const Index size = raw.size();
    std::vector<Range> ranges;
    Index start = 0;
    int readable = 0;
    for (Index i = 0; i < size; ++i) {
        const char32_t c = raw[i];
        if (reader::readable_character_count(QString::fromUcs4(&c, 1).toStdString()) > 0) {
            ++readable;
        }
        const bool sentence_end = c == 0x3002 || c == 0xFF01 || c == 0xFF1F
            || c == U'!' || c == U'?' || c == U'\n'
            || (c == U'.' && (i + 1 == size || QChar::isSpace(raw[i + 1])));
        if ((sentence_end && readable >= 20) || readable >= 100) {
            ranges.push_back({start, i + 1});
            start = i + 1;
            readable = 0;
        }
    }
    if (start < size) {
        if (readable < 8 && !ranges.empty()) {
            ranges.back().end = size;
        } else {
            ranges.push_back({start, size});
        }
    }
    const auto time_at = [&](Index position) {
        return cue.start_time + (cue.end_time - cue.start_time)
            * static_cast<double>(position) / static_cast<double>(size);
    };
    for (std::size_t i = 0; i < ranges.size(); ++i) {
        const auto range = ranges[i];
        pieces.push_back({cue.id + "-part-" + std::to_string(i),
                          time_at(range.begin), time_at(range.end),
                          QString::fromUcs4(raw.data() + range.begin, range.size())
                              .trimmed().toStdString()});
    }
}

bool valid_timing(const SasayakiCue& cue) {
    return std::isfinite(cue.start_time) && std::isfinite(cue.end_time)
        && cue.start_time >= 0 && cue.end_time > cue.start_time;
}

bool has_marker(const SasayakiCue& cue) {
    return cue.text.starts_with("\xEF\xBC\x8A");  // U+FF0A
}

constexpr Index shingle_size = 5;

class Source {
public:
    void build(const std::vector<MatchChapter>& input, Cancellation& cancellation) {
        for (const auto& chapter : input) {
            if (cancellation.poll()) {
                return;
            }
            const Index begin = size();
            const auto raw = scalars(chapter.text);
            for (Index offset = 0; offset < std::ssize(raw); ++offset) {
                if (offset != 0 && offset % 1024 == 0 && cancellation.poll()) {
                    return;
                }
                append_folded(characters_, raw[offset]);
                reader_offsets.resize(characters_.size(), offset);
            }
            chapters.push_back({chapter.index, {begin, size()}});
        }
        // Anchor views reference the complete, immutable folded text.
        for (const auto& chapter : chapters) {
            for (Index pos = chapter.range.begin; pos <= chapter.range.end - shingle_size; ++pos) {
                if ((pos - chapter.range.begin) % 1024 == 0 && cancellation.poll()) {
                    return;
                }
                anchors_[text().substr(pos, shingle_size)].push_back(pos);
            }
        }
    }

    Text text() const { return characters_; }
    Index size() const { return std::ssize(characters_); }

    auto first_chapter(Index position) const {
        return std::ranges::upper_bound(chapters, position,
                                        {}, [](const Chapter& chapter) { return chapter.range.end; });
    }

    const std::vector<Index>* anchors(Text seed) const {
        const auto found = anchors_.find(seed);
        return found == anchors_.end() ? nullptr : &found->second;
    }

    // Exact search only needs the earliest hit or enough hits to decide uniqueness.
    std::vector<Range> exact(Text needle, Range window, std::size_t limit,
                             Cancellation& cancellation) const {
        const Index length = std::ssize(needle);
        if (length == 0 || window.size() < length) {
            return {};
        }
        std::vector<Range> matches;
        auto chapter = first_chapter(window.begin);
        Index checked = 0;
        const auto consider = [&](Index pos) {
            if (++checked % 1024 == 0 && cancellation.poll()) {
                return true;
            }
            if (chapter->range.end <= pos) {
                chapter = first_chapter(pos);
            }
            const Index end = pos + length;
            if (end <= chapter->range.end && text().substr(pos, length) == needle) {
                matches.push_back({pos, end});
            }
            return matches.size() == limit;
        };
        const Index last = window.end - length;
        if (length >= shingle_size) {
            if (const auto* positions = anchors(needle.substr(0, shingle_size))) {
                for (auto it = std::ranges::lower_bound(*positions, window.begin);
                     it != positions->end() && *it <= last; ++it) {
                    if (consider(*it)) {
                        break;
                    }
                }
            }
        } else {
            for (Index pos = window.begin; pos <= last; ++pos) {
                if (consider(pos)) {
                    break;
                }
            }
        }
        if (cancellation.stopped()) {
            return {};
        }
        return matches;
    }

    std::vector<Chapter> chapters;
    std::vector<Index> reader_offsets;

private:
    std::u32string characters_;
    std::unordered_map<Text, std::vector<Index>> anchors_;
};

Index edit_budget(Index length, double fraction) {
    return static_cast<Index>(static_cast<double>(length) * fraction);
}

std::optional<Alignment> align(Text needle, Text source, Range range, Index maximum_edits,
                               Cancellation& cancellation) {
    struct Cell {
        Index edits;
        Index start;
    };
    const auto passage = source.substr(range.begin, range.size());
    const Index width = std::ssize(passage);
    std::vector<Cell> previous(width + 1);
    std::vector<Cell> current(width + 1);
    // A source prefix is free; each cell carries the start of its chosen path.
    for (Index column = 0; column <= width; ++column) {
        previous[column] = {0, column};
    }
    for (Index row = 1; row <= std::ssize(needle); ++row) {
        if (cancellation.poll()) {
            return std::nullopt;
        }
        current[0] = {row, 0};
        for (Index column = 1; column <= width; ++column) {
            const auto diagonal = previous[column - 1].edits
                + (needle[row - 1] != passage[column - 1]);
            const auto deletion = previous[column].edits + 1;
            const auto insertion = current[column - 1].edits + 1;
            if (diagonal <= deletion && diagonal <= insertion) {
                current[column] = {diagonal, previous[column - 1].start};
            } else if (deletion <= insertion) {
                current[column] = {deletion, previous[column].start};
            } else {
                current[column] = {insertion, current[column - 1].start};
            }
        }
        current.swap(previous);
    }
    const auto best = std::ranges::min_element(previous.begin() + 1, previous.end(), {}, &Cell::edits);
    const Index end = best - previous.begin();
    if (best == previous.end() || best->edits > maximum_edits || best->start >= end) {
        return std::nullopt;
    }
    return Alignment{{range.begin + best->start, range.begin + end}, best->edits};
}

std::vector<Range> seeded_windows(Text needle, const Source& source, Range bounds,
                                 Index maximum_edits, Cancellation& cancellation) {
    struct Votes {
        Index last_offset = -1;
        int count = 0;
    };
    std::unordered_map<Index, Votes> buckets;
    const Index length = std::ssize(needle);
    const Index stride = std::max<Index>(1, (length - shingle_size) / 20);
    for (Index offset = 0; offset <= length - shingle_size; offset += stride) {
        if (cancellation.poll()) {
            return {};
        }
        const auto* positions = source.anchors(needle.substr(offset, shingle_size));
        if (!positions || positions->size() > 200) {
            continue;
        }
        for (const auto pos : *positions) {
            const Index proposed = pos - offset;
            if (pos < bounds.begin || pos >= bounds.end || proposed + maximum_edits < bounds.begin) {
                continue;
            }
            auto& votes = buckets[std::max<Index>(0, proposed) / 8];
            if (votes.last_offset != offset) {
                votes.last_offset = offset;
                ++votes.count;
            }
        }
    }
    std::vector<std::pair<Index, int>> candidates;
    for (const auto& [bucket, votes] : buckets) {
        if (votes.count >= 2) {
            candidates.emplace_back(bucket, votes.count);
        }
    }
    std::ranges::sort(candidates, [](const auto& a, const auto& b) {
        return a.second == b.second ? a.first < b.first : a.second > b.second;
    });
    std::vector<Range> windows;
    for (std::size_t i = 0; i < std::min<std::size_t>(12, candidates.size()); ++i) {
        const Index estimate = candidates[i].first * 8;
        const Range window{std::max(bounds.begin, estimate - maximum_edits - 8),
                           std::min(bounds.end, estimate + length + maximum_edits + 16)};
        if (window.begin < window.end) {
            windows.push_back(window);
        }
    }
    return windows;
}

std::optional<Range> approximate(Text needle, const Source& source, Range bounds, bool global,
                                 Cancellation& cancellation) {
    const Index length = std::ssize(needle);
    const Index maximum_edits = edit_budget(length, global ? 0.14 : 0.2);
    if (maximum_edits == 0 || bounds.size() < length - maximum_edits) {
        return std::nullopt;
    }
    const auto windows = global ? seeded_windows(needle, source, bounds, maximum_edits, cancellation)
                                : std::vector<Range>{bounds};
    std::vector<Alignment> results;
    for (const auto window : windows) {
        for (auto chapter = source.first_chapter(window.begin);
             chapter != source.chapters.end() && chapter->range.begin < window.end; ++chapter) {
            if (cancellation.poll()) {
                return std::nullopt;
            }
            const Range clip{std::max(window.begin, chapter->range.begin),
                             std::min(window.end, chapter->range.end)};
            if (clip.size() < length - maximum_edits) {
                continue;
            }
            if (const auto result = align(needle, source.text(), clip, maximum_edits, cancellation)) {
                const auto duplicate = std::ranges::find(results, result->range, &Alignment::range);
                if (duplicate == results.end()) {
                    results.push_back(*result);
                } else {
                    *duplicate = *result;
                }
            } else if (cancellation.stopped()) {
                return std::nullopt;
            }
        }
    }
    if (results.empty()) {
        return std::nullopt;
    }
    const auto best = std::ranges::min_element(results, {}, [](const Alignment& result) {
        return std::tie(result.edits, result.range.begin);
    });
    if (global && std::ranges::any_of(results, [&](const Alignment& other) {
            return !other.range.overlaps(best->range) && other.edits <= best->edits + 1;
        })) {
        return std::nullopt;
    }
    return best->range;
}

bool confirms_jump(Range candidate, Index index, const std::vector<std::u32string>& texts,
                   const Source& source, Cancellation& cancellation) {
    const Index count = std::ssize(texts);
    if (index + 1 == count) {
        return true;
    }
    Index cursor = candidate.end;
    const Index end = std::min(source.size(), cursor + 600);
    Index confirmed = 0;
    for (Index next = index + 1; next < std::min(count, index + 9); ++next) {
        if (cancellation.poll()) {
            return false;
        }
        const Text needle = texts[next];
        const Index length = std::ssize(needle);
        if (length < 6) {
            continue;
        }
        const auto exact = source.exact(needle, {cursor, end}, 1, cancellation);
        std::optional<Range> range;
        if (!exact.empty()) {
            range = exact.front();
        } else if (length >= 12 && !cancellation.stopped()) {
            range = approximate(needle, source, {cursor, end}, false, cancellation);
        }
        if (range) {
            confirmed += length;
            cursor = range->end;
            if (confirmed >= 16) {
                return true;
            }
        }
    }
    return false;
}

std::optional<Range> find_match(Text needle, const Source& source, Index cursor, bool anchored,
                                int search_window, Index index,
                                const std::vector<std::u32string>& texts, Cancellation& cancellation) {
    const Index length = std::ssize(needle);
    const Index window = length < 6 ? std::clamp(search_window, 0, 30) : std::max(0, search_window);
    const Range local{cursor, std::min(source.size(), cursor + length + window)};
    if (anchored || length >= 6) {
        const auto exact = source.exact(needle, local, 1, cancellation);
        if (!exact.empty()) {
            return exact.front();
        }
    }
    if (anchored && length >= 10 && !cancellation.stopped()) {
        if (const auto range = approximate(needle, source, local, false, cancellation)) {
            return range;
        }
    }
    if (length < 12 || cancellation.stopped()) {
        return std::nullopt;
    }

    const Range forward{cursor, source.size()};
    const auto exact = source.exact(needle, forward, 2, cancellation);
    std::optional<Range> candidate;
    if (exact.size() == 1) {
        candidate = exact.front();
    } else if (exact.empty() && length >= 16 && !cancellation.stopped()) {
        candidate = approximate(needle, source, forward, true, cancellation);
    }
    if (candidate && confirms_jump(*candidate, index, texts, source, cancellation)) {
        return candidate;
    }
    return std::nullopt;
}

bool japanese(Text text) {
    return std::ranges::any_of(text, [](char32_t c) { return c >= 0x3000 && c <= 0x9FFF; });
}

void infer_gaps(std::vector<std::optional<Placement>>& proposed, Range region, Index first,
                const std::vector<SasayakiCue>& pieces, const std::vector<std::u32string>& texts,
                Text source) {
    const Index count = std::ssize(proposed);
    Index index = 0;
    while (index < count) {
        if (proposed[index]) {
            ++index;
            continue;
        }
        const Index start = index;
        while (index < count && !proposed[index]) {
            ++index;
        }
        const Index lower = start == 0 ? region.begin : proposed[start - 1]->range.end;
        const Index upper = index == count ? region.end : proposed[index]->range.begin;
        const Index span = upper - lower;
        const Index missing = index - start;
        if (span < missing || span > 160 || missing > 8) {
            continue;
        }
        Index total = 0;
        bool japanese_cues = false;
        bool continuous = true;
        for (Index i = start; i < index; ++i) {
            total += std::ssize(texts[first + i]);
            japanese_cues |= japanese(texts[first + i]);
        }
        for (Index i = first + start; i <= first + index; ++i) {
            continuous &= pieces[i].start_time - pieces[i - 1].end_time <= 10;
        }
        const double ratio = static_cast<double>(span) / static_cast<double>(total);
        if (ratio < 0.3 || ratio > 2.5 || !continuous
            || japanese_cues != japanese(source.substr(lower, span))) {
            continue;
        }
        Index position = lower;
        Index weight = 0;
        for (Index i = start; i < index; ++i) {
            weight += std::ssize(texts[first + i]);
            const auto estimated = lower + static_cast<Index>(
                std::round(static_cast<double>(span) * static_cast<double>(weight)
                           / static_cast<double>(total)));
            // Reserve at least one source character for every following piece.
            const Index end = std::min(upper - (index - i - 1), std::max(position + 1, estimated));
            proposed[i] = Placement{{position, end}, true};
            position = end;
        }
    }
}

void fill_gaps(std::vector<std::optional<Placement>>& placements,
               const std::vector<std::u32string>& texts, const std::vector<SasayakiCue>& pieces,
               const Source& source, Cancellation& cancellation) {
    std::vector<Index> anchors;
    for (Index index = 0; index < std::ssize(placements); ++index) {
        if (placements[index] && texts[index].size() > 2) {
            anchors.push_back(index);
        }
    }
    Index remaining_work = 30'000'000;
    for (Index anchor = 1; anchor < std::ssize(anchors); ++anchor) {
        if (cancellation.poll()) {
            return;
        }
        const Index left = anchors[anchor - 1];
        const Index right = anchors[anchor];
        const Index first = left + 1;
        const Index count = right - first;
        if (count == 0 || count > 40) {
            continue;
        }
        const Range before = placements[left]->range;
        const Range after = placements[right]->range;
        const Range region{before.end, after.begin};
        const double time_gap = pieces[right].start_time - pieces[left].end_time;
        if (region.size() <= 0 || region.size() > 2000 || time_gap < 0 || time_gap > 180
            || source.first_chapter(before.begin) != source.first_chapter(after.begin)
            || pieces[right].start_time < pieces[right - 1].start_time) {
            continue;
        }
        Index previous_count = 0;
        Index characters = 0;
        bool valid = true;
        for (Index i = first; i < right; ++i) {
            previous_count += placements[i].has_value();
            characters += std::ssize(texts[i]);
            valid &= !texts[i].empty() && valid_timing(pieces[i]) && !has_marker(pieces[i])
                && pieces[i].start_time >= pieces[i - 1].start_time;
        }
        if (!valid || previous_count == count || characters > remaining_work / region.size()) {
            continue;
        }
        remaining_work -= region.size() * characters;

        std::vector<Index> order(count);
        std::iota(order.begin(), order.end(), Index{0});
        std::ranges::stable_sort(order, std::ranges::greater{}, [&](Index offset) {
            return texts[first + offset].size();
        });
        std::vector<std::optional<Placement>> proposed(count);
        for (const auto offset : order) {
            if (cancellation.poll()) {
                return;
            }
            Range available = region;
            for (Index i = offset; i > 0; --i) {
                if (proposed[i - 1]) {
                    available.begin = proposed[i - 1]->range.end;
                    break;
                }
            }
            for (Index i = offset + 1; i < count; ++i) {
                if (proposed[i]) {
                    available.end = proposed[i]->range.begin;
                    break;
                }
            }
            if (available.size() <= 0) {
                continue;
            }
            const Text needle = texts[first + offset];
            const Index edits = needle.size() >= 3 ? edit_budget(std::ssize(needle), 0.55) : 0;
            if (const auto aligned = align(needle, source.text(), available, edits, cancellation)) {
                proposed[offset] = Placement{aligned->range};
            } else if (cancellation.stopped()) {
                return;
            }
        }
        infer_gaps(proposed, region, first, pieces, texts, source.text());
        // A gap replaces existing placements only as a complete, improving proposal.
        if (std::ranges::count_if(proposed, [](const auto& item) { return item.has_value(); })
            > previous_count) {
            std::ranges::copy(proposed, placements.begin() + first);
        }
    }
}

}  // namespace

SasayakiMatchData match_cues(const std::vector<SasayakiCue>& cues,
                            const std::vector<MatchChapter>& chapters,
                            int search_window,
                            const std::function<bool()>& cancelled) {
    // Count every split piece even when cancellation prevents searching it.
    std::vector<SasayakiCue> pieces;
    for (const auto& cue : cues) {
        append_pieces(pieces, cue);
    }
    SasayakiMatchData result{{}, static_cast<int>(pieces.size())};
    Cancellation cancellation(cancelled);
    if (pieces.empty() || cancellation.poll() || chapters.empty()) {
        return result;
    }
    Source source;
    source.build(chapters, cancellation);
    if (cancellation.stopped() || source.size() == 0) {
        return result;
    }
    std::vector<std::u32string> texts;
    texts.reserve(pieces.size());
    for (const auto& piece : pieces) {
        if (cancellation.poll()) {
            return result;
        }
        texts.push_back(folded_cue(piece));
    }

    std::vector<std::optional<Placement>> placements(pieces.size());
    Index cursor = 0;
    for (Index index = 0; index < std::ssize(pieces); ++index) {
        if (cancellation.poll()) {
            break;
        }
        const auto& cue = pieces[index];
        const Text needle = texts[index];
        if (needle.empty() || !valid_timing(cue) || (needle.size() < 5 && has_marker(cue))) {
            continue;
        }
        if (const auto range = find_match(needle, source, cursor, cursor != 0, search_window,
                                          index, texts, cancellation)) {
            cursor = range->end;
            placements[index] = Placement{*range};
        }
    }
    if (!cancellation.stopped()) {
        fill_gaps(placements, texts, pieces, source, cancellation);
    }
    for (Index index = 0; index < std::ssize(pieces); ++index) {
        if (const auto& placement = placements[index]) {
            const Range range = placement->range;
            const auto& chapter = *source.first_chapter(range.begin);
            const Index start = source.reader_offsets[range.begin];
            const Index end = source.reader_offsets[range.end - 1] + 1;
            const auto& cue = pieces[index];
            result.matches.push_back({cue.id, cue.start_time, cue.end_time, cue.text, chapter.index,
                                      static_cast<int>(start), static_cast<int>(end - start),
                                      placement->inferred});
            --result.unmatched;
        }
    }
    return result;
}

}  // namespace niratan::sasayaki

// SPDX-License-Identifier: GPL-3.0-or-later

#include "match_chapters.h"

#include <QString>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "services/reader/character_normalizer.h"

namespace niratan::sasayaki {
namespace {

// Swift's href.split(separator: "#", maxSplits: 1).first ?? href, with
// omittingEmptySubsequences. A leading '#' drops the empty first part, so the
// text after it is kept; a lone "#" or an empty href falls back to the whole
// href.
std::string guide_toc_path(std::string_view href) {
    const std::size_t hash = href.find('#');
    if (hash == std::string_view::npos) {
        return std::string(href);
    }
    const std::string_view before = href.substr(0, hash);
    if (!before.empty()) {
        return std::string(before);
    }
    const std::string_view after = href.substr(hash + 1);
    if (!after.empty()) {
        return std::string(after);
    }
    return std::string(href);
}

bool is_toc_type(const std::string& type) {
    return QString::fromStdString(type).toLower() == QLatin1String("toc");
}

}  // namespace

std::vector<MatchChapter> match_chapters(const novels::EpubBook& book,
                                         const std::function<bool()>& cancelled) {
    std::unordered_set<std::string> toc_paths;
    for (const novels::EpubGuideReference& reference : book.guide()) {
        if (is_toc_type(reference.type)) {
            toc_paths.insert(guide_toc_path(reference.href));
        }
    }

    std::vector<MatchChapter> chapters;
    const std::vector<novels::EpubSpineItem>& spine = book.spine();
    for (std::size_t index = 0; index < spine.size(); ++index) {
        if (cancelled && cancelled()) {
            break;
        }
        const novels::EpubSpineItem& item = spine[index];
        if (!item.linear) {
            continue;
        }
        if (item.properties.find("nav") != std::string::npos) {
            continue;
        }
        if (toc_paths.contains(item.manifest_href)) {
            continue;
        }
        const std::optional<std::string> content = book.read(item.href);
        if (!content.has_value()) {
            continue;
        }
        chapters.push_back(MatchChapter{static_cast<int>(index), reader::filtered_text(*content)});
    }
    return chapters;
}

}  // namespace niratan::sasayaki

// SPDX-License-Identifier: GPL-3.0-or-later

#include "book_session.h"

#include <QFile>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "models/profiles/profile_index_json.h"
#include "models/profiles/profile_ops.h"
#include "services/profiles/profile_repository.h"
#include "services/reader/character_normalizer.h"
#include "services/reader/toc_target.h"
#include "services/storage/json_file.h"

namespace niratan::reader {
namespace {

std::string to_lower_ascii(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char character : text) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return lowered;
}

// A zip entry name may name any container path, but never one that climbs out
// of the archive.
bool is_container_path(std::string_view path) {
    if (path.empty()) {
        return false;
    }
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::size_t end = slash == std::string_view::npos ? path.size() : slash;
        if (path.substr(start, end - start) == "..") {
            return false;
        }
        if (slash == std::string_view::npos) {
            break;
        }
        start = slash + 1;
    }
    return true;
}

}  // namespace

std::optional<BookSession> BookSession::open(const std::filesystem::path& path,
                                             std::string* error) {
    std::optional<niratan::novels::EpubBook> book = niratan::novels::EpubBook::open(path, error);
    if (!book.has_value()) {
        return std::nullopt;
    }

    std::vector<std::string> spine_paths;
    spine_paths.reserve(book->spine().size());
    for (const niratan::novels::EpubSpineItem& item : book->spine()) {
        spine_paths.push_back(item.href);
    }

    std::map<std::string, std::size_t> first_spine_index;
    for (std::size_t index = 0; index < spine_paths.size(); ++index) {
        first_spine_index.try_emplace(spine_paths[index], index);
    }

    std::map<std::string, std::set<std::string>> fragments_by_path;
    std::vector<std::optional<std::string>> spine_titles(spine_paths.size());
    std::vector<ContentsEntry> contents;
    for (const niratan::novels::EpubTocEntry& entry : book->toc()) {
        const TocTarget target = split_target(entry.href);
        // Decode the fragment the same way ContentsEntry stores it, so the
        // fragment_offsets map keys match ContentsEntry.fragment in
        // chapter_characters_remaining.
        const std::string decoded_fragment =
            target.fragment
                ? decoded_character_references(percent_decoded(*target.fragment))
                : std::string();
        if (target.fragment.has_value()) {
            fragments_by_path[target.path].insert(decoded_fragment);
        }
        const auto spine_index = first_spine_index.find(target.path);
        if (spine_index == first_spine_index.end()) {
            continue;
        }
        contents.push_back(ContentsEntry{
            static_cast<int>(spine_index->second), entry.level, entry.title,
            decoded_fragment});
        // The first entry naming the chapter supplies its label.
        if (!spine_titles[spine_index->second].has_value()) {
            spine_titles[spine_index->second] = entry.title;
        }
    }
    // An empty TOC produces no contents entries. chapterStarts([]) in Swift
    // returns only [0], so the whole book is treated as one chapter;
    // chapter_characters_remaining seeds starts with {0} and reaches the same
    // result when contents_ is empty.

    niratan::books::BookInfo info;
    int running_total = 0;
    for (std::size_t index = 0; index < spine_paths.size(); ++index) {
        const std::string& chapter_path = spine_paths[index];
        const std::optional<std::string> bytes = book->read(chapter_path);
        if (!bytes.has_value()) {
            continue;
        }

        const int count = readable_character_count(*bytes);
        niratan::books::ChapterInfo chapter;
        chapter.spine_index = static_cast<int>(index);
        chapter.current_total = running_total;
        chapter.chapter_count = count;
        const auto fragments = fragments_by_path.find(chapter_path);
        if (fragments != fragments_by_path.end()) {
            chapter.fragment_offsets = fragment_offsets(*bytes, fragments->second);
        }

        info.chapter_info[chapter_path] = std::move(chapter);
        running_total += count;
    }
    info.character_count = running_total;

    return BookSession(std::move(*book), std::move(info), std::move(spine_paths),
                       std::move(spine_titles), std::move(contents));
}

BookSession::BookSession(niratan::novels::EpubBook book, niratan::books::BookInfo info,
                         std::vector<std::string> spine_paths,
                         std::vector<std::optional<std::string>> spine_titles,
                         std::vector<ContentsEntry> contents)
    : book_(std::move(book)),
      info_(std::move(info)),
      spine_paths_(std::move(spine_paths)),
      spine_titles_(std::move(spine_titles)),
      contents_(std::move(contents)) {}

const niratan::novels::EpubBook& BookSession::book() const { return book_; }
const niratan::books::BookInfo& BookSession::info() const { return info_; }

int BookSession::spine_count() const { return static_cast<int>(spine_paths_.size()); }

std::string BookSession::spine_path(int spine_index) const {
    if (spine_index < 0 || static_cast<std::size_t>(spine_index) >= spine_paths_.size()) {
        return {};
    }
    return spine_paths_[static_cast<std::size_t>(spine_index)];
}

std::string BookSession::chapter_title(int spine_index) const {
    if (spine_index < 0 || static_cast<std::size_t>(spine_index) >= spine_titles_.size()) {
        return {};
    }
    return spine_titles_[static_cast<std::size_t>(spine_index)].value_or(std::string{});
}

const std::vector<ContentsEntry>& BookSession::contents() const { return contents_; }

std::string BookSession::chapter_document(int spine_index, const ShellInput& input,
                                          const ReaderScripts& scripts) const {
    const std::string path = spine_path(spine_index);
    if (path.empty()) {
        return {};
    }
    const std::optional<std::string> bytes = book_.read(path);
    if (!bytes.has_value()) {
        return {};
    }
    return reader_document(*bytes, input, scripts);
}

std::optional<SessionResource> BookSession::resource(std::string_view path) const {
    if (!is_container_path(path)) {
        return std::nullopt;
    }
    const std::optional<std::string> bytes = book_.read(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    return SessionResource{*bytes, container_mime_type(path)};
}

int BookSession::character_offset(int spine_index, double progress) const {
    const std::string path = spine_path(spine_index);
    if (path.empty()) {
        return 0;
    }
    const auto chapter = info_.chapter_info.find(path);
    if (chapter == info_.chapter_info.end()) {
        return 0;
    }
    const double clamped = std::min(std::max(progress, 0.0), 1.0);
    return chapter->second.current_total +
           static_cast<int>(clamped * static_cast<double>(chapter->second.chapter_count));
}

std::optional<std::pair<int, double>> BookSession::position_for_character(int characters) const {
    return info_.resolve_character_position(characters);
}

ReaderSettingsWatch::ReaderSettingsWatch(std::filesystem::path root,
                                         std::function<void()> changed)
    : root_(std::move(root)) {
    if (!root_.empty()) {
        const profiles::ProfileRepository repository(root_);
    }
    QObject::connect(&watcher_, &QFileSystemWatcher::fileChanged, &watcher_, changed);
    QObject::connect(&watcher_, &QFileSystemWatcher::directoryChanged, &watcher_, changed);
}

std::optional<profiles::ReaderProfileSettings> ReaderSettingsWatch::read() {
    if (root_.empty()) {
        return std::nullopt;
    }
    const auto profiles_dir = root_ / "Profiles";
    const auto index_path = profiles_dir / "profiles.json";
    const auto index_read = storage::read_json_file(index_path);
    auto index = index_read.status == storage::JsonReadStatus::missing
                     ? std::optional(profiles::ProfileIndex::initial())
                     : profiles::profile_index_from_json(index_read.document);
    auto path = settings_path_;
    if (index) {
        const auto profile = profiles::resolve_profile(profiles::normalize_profile_index(*index));
        const std::string& id = profile.id;
        content_language = profiles::language_id(profile.language());
        if (profiles::is_safe_profile_id(id)) {
            path = profiles_dir / id / "reader_settings.json";
        }
    }
    const bool profile_changed = path != settings_path_;
    settings_path_ = path;

    QStringList wanted{QFile(profiles_dir).fileName(), QFile(index_path).fileName()};
    if (!path.empty()) {
        const auto lookup_path = path.parent_path() / "dictionary_settings.json";
        wanted.append(QFile(path.parent_path()).fileName());
        wanted.append(QFile(path).fileName());
        wanted.append(QFile(lookup_path).fileName());
        const auto lookup = storage::read_json_file(lookup_path);
        if (auto decoded = profiles::dictionary_settings_from_json(lookup.document)) {
            lookup_settings = profiles::resolved_dictionary_settings(*decoded);
        } else if (profile_changed || lookup.status == storage::JsonReadStatus::missing) {
            lookup_settings = profiles::default_dictionary_settings();
        }
    }
    const auto watched = watcher_.files() + watcher_.directories();
    for (const QString& old_path : watched) {
        if (!wanted.contains(old_path)) {
            watcher_.removePath(old_path);
        }
    }
    for (const QString& new_path : wanted) {
        if (!watched.contains(new_path) && QFile::exists(new_path)) {
            watcher_.addPath(new_path);
        }
    }
    if (path.empty()) {
        return std::nullopt;
    }
    const auto settings = storage::read_json_file(path);
    if (settings.status == storage::JsonReadStatus::missing && profile_changed) {
        return profiles::default_reader_settings();
    }
    return profiles::reader_settings_from_json(settings.document);
}

std::string container_mime_type(std::string_view path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos) {
        return "application/octet-stream";
    }
    const std::string extension = to_lower_ascii(path.substr(dot + 1));

    if (extension == "xhtml" || extension == "xht" || extension == "html" || extension == "htm") {
        return "application/xhtml+xml";
    }
    if (extension == "css") {
        return "text/css";
    }
    if (extension == "js") {
        return "text/javascript";
    }
    if (extension == "png") {
        return "image/png";
    }
    if (extension == "jpg" || extension == "jpeg") {
        return "image/jpeg";
    }
    if (extension == "gif") {
        return "image/gif";
    }
    if (extension == "svg") {
        return "image/svg+xml";
    }
    if (extension == "webp") {
        return "image/webp";
    }
    if (extension == "otf") {
        return "font/otf";
    }
    if (extension == "ttf") {
        return "font/ttf";
    }
    if (extension == "woff") {
        return "font/woff";
    }
    if (extension == "woff2") {
        return "font/woff2";
    }
    if (extension == "mp3") {
        return "audio/mpeg";
    }
    if (extension == "m4a") {
        return "audio/mp4";
    }
    if (extension == "mp4") {
        return "video/mp4";
    }
    if (extension == "ncx") {
        return "application/x-dtbncx+xml";
    }
    if (extension == "xml") {
        return "application/xml";
    }
    if (extension == "json") {
        return "application/json";
    }
    return "application/octet-stream";
}

}  // namespace niratan::reader

// SPDX-License-Identifier: GPL-3.0-or-later

#include "book_session.h"

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QUrl>

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
#include "services/storage/json_file.h"

namespace niratan::reader {
namespace {

QString from_utf8(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::string to_utf8(const QString& text) { return text.toUtf8().toStdString(); }

// An href split at its first '#'. Table of contents paths arrive resolved
// against the package directory, so only the fragment needs decoding before
// it is compared with an identifier.
struct TocTarget {
    std::string path;
    std::optional<std::string> fragment;
};

TocTarget split_target(const std::string& href) {
    const std::size_t fragment_at = href.find('#');
    if (fragment_at == std::string::npos) {
        return TocTarget{href, std::nullopt};
    }
    const std::string fragment = href.substr(fragment_at + 1);
    // Swift's target(from:) drops an empty fragment, so "#" alone is no target.
    return TocTarget{href.substr(0, fragment_at), fragment.empty()
                                                        ? std::nullopt
                                                        : std::optional<std::string>(fragment)};
}

std::string percent_decoded(std::string_view text) {
    const QByteArray encoded(text.data(), static_cast<qsizetype>(text.size()));
    return to_utf8(QUrl::fromPercentEncoding(encoded));
}

std::string to_lower_ascii(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char character : text) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return lowered;
}

// The value of the tag's id attribute, else xml:id, following
// ReaderChapterIndex.attributeValues: attribute names are lowercased and the
// first spelling of a name wins. Character references are still encoded.
std::optional<QString> tag_identifier(const QString& tag) {
    static const QRegularExpression attribute_regex(QStringLiteral(
        R"regex(\s([A-Za-z_:][A-Za-z0-9_.:-]*)\s*=\s*(?:"([^"]*)"|'([^']*)'))regex"));

    QHash<QString, QString> attributes;
    QRegularExpressionMatchIterator matches = attribute_regex.globalMatch(tag);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const QString name = match.captured(1).toLower();
        if (attributes.contains(name)) {
            continue;
        }
        if (match.capturedStart(2) >= 0) {
            attributes.insert(name, match.captured(2));
        } else if (match.capturedStart(3) >= 0) {
            attributes.insert(name, match.captured(3));
        }
    }

    const auto id = attributes.constFind(QStringLiteral("id"));
    if (id != attributes.cend()) {
        return *id;
    }
    const auto xml_id = attributes.constFind(QStringLiteral("xml:id"));
    if (xml_id != attributes.cend()) {
        return *xml_id;
    }
    return std::nullopt;
}

// The fragments' readable character offsets inside one chapter, following
// ReaderChapterIndex.fragmentOffsets. Every requested fragment starts at zero
// and keeps that offset when no tag carries its identifier.
//
// The counted span is the body content: everything after the first <body ...>
// tag up to the next </body>. A document without a body tag leaves every
// offset at zero, as ReaderChapterIndex does. Tags inside comments, script and
// style blocks, and CDATA never name a fragment. Every fragment reachable
// through an identifier is filled at the first tag carrying it.
std::map<std::string, int> fragment_offsets(std::string_view markup,
                                            const std::set<std::string>& fragments) {
    std::map<std::string, int> offsets;
    if (fragments.empty()) {
        return offsets;
    }

    // A fragment is reachable by its raw and its percent-decoded spelling.
    // Character references are decoded on both sides, as the tag's attribute is.
    std::map<std::string, std::vector<std::string>> pending;
    for (const std::string& fragment : fragments) {
        offsets.emplace(fragment, 0);
        const std::string decoded = decoded_character_references(fragment);
        const std::string decoded_percent = decoded_character_references(percent_decoded(fragment));
        pending[decoded].push_back(fragment);
        if (decoded_percent != decoded) {
            pending[decoded_percent].push_back(fragment);
        }
    }

    const QString text = from_utf8(markup);

    static const QRegularExpression body_regex(QStringLiteral(R"(<body\b[^>]*>)"),
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression closing_body_regex(QStringLiteral(R"(</body\s*>)"),
                                                       QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch body_match = body_regex.match(text);
    if (!body_match.hasMatch()) {
        return offsets;
    }
    const qsizetype content_start = body_match.capturedEnd();
    qsizetype content_end = text.size();
    const QRegularExpressionMatch closing_match = closing_body_regex.match(text, content_start);
    if (closing_match.hasMatch()) {
        content_end = closing_match.capturedStart();
    }
    const QString content = text.mid(content_start, content_end - content_start);

    static const QRegularExpression comment_regex(QStringLiteral(R"((?s)<!--.*?-->)"));
    static const QRegularExpression script_style_regex(
        QStringLiteral(R"((?s)<(script|style)\b[^>]*>.*?</\1\s*>)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cdata_regex(QStringLiteral(R"((?s)<!\[CDATA\[.*?\]\]>)"));
    static const QRegularExpression opening_tag_regex(
        QStringLiteral(R"(<\s*[A-Za-z][A-Za-z0-9:_-]*\b[^>]*>)"),
        QRegularExpression::CaseInsensitiveOption);

    std::vector<std::pair<qsizetype, qsizetype>> ignored;
    for (const QRegularExpression& regex : {comment_regex, script_style_regex, cdata_regex}) {
        QRegularExpressionMatchIterator matches = regex.globalMatch(content);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            ignored.emplace_back(match.capturedStart(), match.capturedEnd());
        }
    }
    std::sort(ignored.begin(), ignored.end(),
              [](const std::pair<qsizetype, qsizetype>& left,
                 const std::pair<qsizetype, qsizetype>& right) { return left.first < right.first; });

    std::size_t ignored_index = 0;
    QRegularExpressionMatchIterator tags = opening_tag_regex.globalMatch(content);
    while (tags.hasNext()) {
        const QRegularExpressionMatch tag = tags.next();
        const qsizetype tag_start = tag.capturedStart();
        while (ignored_index < ignored.size() && ignored[ignored_index].second <= tag_start) {
            ++ignored_index;
        }
        if (ignored_index < ignored.size() && ignored[ignored_index].first <= tag_start) {
            continue;
        }

        const std::optional<QString> raw_identifier = tag_identifier(tag.captured(0));
        if (!raw_identifier.has_value()) {
            continue;
        }
        const auto match = pending.find(decoded_character_references(to_utf8(*raw_identifier)));
        if (match == pending.end()) {
            continue;
        }
        const int offset = readable_character_count(to_utf8(content.left(tag_start)));
        for (const std::string& fragment : match->second) {
            offsets[fragment] = offset;
        }
        pending.erase(match);
        if (pending.empty()) {
            break;
        }
    }

    return offsets;
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
        if (target.fragment.has_value()) {
            fragments_by_path[target.path].insert(*target.fragment);
        }
        const auto spine_index = first_spine_index.find(target.path);
        if (spine_index == first_spine_index.end()) {
            continue;
        }
        contents.push_back(ContentsEntry{
            static_cast<int>(spine_index->second), entry.level, entry.title,
            target.fragment ? decoded_character_references(percent_decoded(*target.fragment))
                            : std::string()});
        // The first entry naming the chapter supplies its label.
        if (!spine_titles[spine_index->second].has_value()) {
            spine_titles[spine_index->second] = entry.title;
        }
    }
    if (book->toc().empty()) {
        for (std::size_t index = 0; index < spine_paths.size(); ++index) {
            contents.push_back(ContentsEntry{static_cast<int>(index), 0, spine_paths[index], {}});
        }
    }

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
        const std::string id = profiles::resolve_profile(profiles::normalize_profile_index(*index)).id;
        if (profiles::is_safe_profile_id(id)) {
            path = profiles_dir / id / "reader_settings.json";
        }
    }
    const bool profile_changed = path != settings_path_;
    settings_path_ = path;

    QStringList wanted{QFile(profiles_dir).fileName(), QFile(index_path).fileName()};
    if (!path.empty()) {
        wanted.append(QFile(path.parent_path()).fileName());
        wanted.append(QFile(path).fileName());
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

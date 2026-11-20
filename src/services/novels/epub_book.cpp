// SPDX-License-Identifier: GPL-3.0-or-later

#include "epub_book.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include <zip.h>

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace niratan::novels {
namespace {

// Entries are located by their container path, so names are kept in the
// archive's own encoding and never percent-encoded.
constexpr std::string_view kContainerEntry = "META-INF/container.xml";

struct ManifestItem {
    std::string id;
    std::string href;
    std::string media_type;
    std::string properties;
};

// The package contents before they are moved into EpubBook::Impl, which is
// private and therefore cannot be named by these helpers.
struct Package {
    EpubMetadata metadata;
    std::vector<EpubSpineItem> spine;
    std::vector<EpubTocEntry> toc;
};

void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

QString attribute_value(const QXmlStreamAttributes& attributes, const QString& name) {
    return attributes.value(name).toString();
}

bool has_property(std::string_view properties, std::string_view token) {
    std::size_t position = 0;
    while (position < properties.size()) {
        while (position < properties.size() &&
               std::isspace(static_cast<unsigned char>(properties[position])) != 0) {
            ++position;
        }
        const std::size_t begin = position;
        while (position < properties.size() &&
               std::isspace(static_cast<unsigned char>(properties[position])) == 0) {
            ++position;
        }
        if (properties.substr(begin, position - begin) == token) {
            return true;
        }
    }
    return false;
}

// Turns an href from the package into a container path: percent-decoded once,
// resolved against the package directory and normalized. A path that would
// leave the container is unusable, as is an href that names no document.
std::optional<std::string> resolve_href(std::string_view href, std::string_view package_dir) {
    const std::size_t fragment_at = href.find('#');
    const std::string_view path = href.substr(0, fragment_at);
    const std::string_view fragment = fragment_at == std::string_view::npos
                                          ? std::string_view{}
                                          : href.substr(fragment_at);
    if (path.empty()) {
        return std::nullopt;
    }

    const QString decoded = QUrl::fromPercentEncoding(
        QByteArray(path.data(), static_cast<qsizetype>(path.size())));
    std::string relative = decoded.toStdString();

    std::size_t start = 0;
    while (start < relative.size()) {
        if (relative[start] == '/') {
            ++start;
        } else if (relative.compare(start, 2, "./") == 0) {
            start += 2;
        } else {
            break;
        }
    }
    relative.erase(0, start);
    if (relative.empty()) {
        return std::nullopt;
    }

    std::filesystem::path combined(relative);
    if (!package_dir.empty()) {
        combined = std::filesystem::path(std::string(package_dir)) / combined;
    }
    combined = combined.lexically_normal();
    if (combined.empty()) {
        return std::nullopt;
    }
    if (*combined.begin() == "..") {
        return std::nullopt;
    }

    std::string result = combined.generic_string();
    result.append(fragment);
    return result;
}

bool read_entry(zip_t* zip, const std::string& name, std::string* data) {
    zip_stat_t stat{};
    if (zip_stat(zip, name.c_str(), 0, &stat) != 0 || (stat.valid & ZIP_STAT_SIZE) == 0) {
        return false;
    }
    zip_file_t* file = zip_fopen(zip, name.c_str(), 0);
    if (file == nullptr) {
        return false;
    }

    std::string buffer(static_cast<std::size_t>(stat.size), '\0');
    zip_uint64_t offset = 0;
    while (offset < stat.size) {
        const zip_int64_t read =
            zip_fread(file, buffer.data() + offset, stat.size - offset);
        if (read <= 0) {
            break;
        }
        offset += static_cast<zip_uint64_t>(read);
    }
    const bool complete = offset == stat.size;
    zip_fclose(file);
    if (!complete) {
        return false;
    }
    *data = std::move(buffer);
    return true;
}

bool parse_container(const std::string& xml, std::string* rootfile, std::string* error) {
    QXmlStreamReader reader(QByteArray::fromStdString(xml));
    while (!reader.atEnd()) {
        if (reader.readNext() != QXmlStreamReader::StartElement) {
            continue;
        }
        if (reader.name() != QLatin1String("rootfile")) {
            continue;
        }
        const QString full_path =
            attribute_value(reader.attributes(), QStringLiteral("full-path"));
        if (full_path.isEmpty()) {
            set_error(error, "META-INF/container.xml has a rootfile without a full-path");
            return false;
        }
        *rootfile = full_path.toStdString();
        return true;
    }
    if (reader.hasError()) {
        set_error(error,
                  "cannot parse META-INF/container.xml: " + reader.errorString().toStdString());
        return false;
    }
    set_error(error, "META-INF/container.xml has no rootfile");
    return false;
}

bool nav_is_toc(const QXmlStreamAttributes& attributes) {
    for (const QXmlStreamAttribute& attribute : attributes) {
        if (attribute.name() == QLatin1String("type") &&
            attribute.value() == QLatin1String("toc")) {
            return true;
        }
    }
    return false;
}

bool parse_nav(const std::string& xml, const std::string& package_dir,
               std::vector<EpubTocEntry>* toc, std::string* error) {
    QXmlStreamReader reader(QByteArray::fromStdString(xml));
    QStringList stack;
    // Stack size at the toc nav, negative while no toc nav is open.
    qsizetype toc_nav_depth = -1;
    int ol_depth = 0;
    bool finished = false;

    while (!reader.atEnd() && !finished) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: {
                const QString name = reader.name().toString();
                if (toc_nav_depth < 0) {
                    stack.push_back(name);
                    if (name == QLatin1String("nav") && nav_is_toc(reader.attributes())) {
                        toc_nav_depth = stack.size();
                    }
                    break;
                }
                if (name == QLatin1String("a")) {
                    const std::optional<std::string> href = resolve_href(
                        attribute_value(reader.attributes(), QStringLiteral("href"))
                            .toStdString(),
                        package_dir);
                    const int level = ol_depth > 0 ? ol_depth - 1 : 0;
                    const QString title =
                        reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    toc->push_back(EpubTocEntry{level, title.toStdString(),
                                                href.value_or(std::string())});
                    break;
                }
                if (name == QLatin1String("ol")) {
                    ++ol_depth;
                }
                stack.push_back(name);
                break;
            }
            case QXmlStreamReader::EndElement: {
                if (stack.isEmpty()) {
                    break;
                }
                const QString name = stack.last();
                stack.removeLast();
                if (toc_nav_depth < 0) {
                    break;
                }
                if (name == QLatin1String("ol") && ol_depth > 0) {
                    --ol_depth;
                }
                // The toc nav's own end element closes the collected list.
                if (stack.size() < toc_nav_depth) {
                    toc_nav_depth = -1;
                    ol_depth = 0;
                    finished = true;
                }
                break;
            }
            default:
                break;
        }
    }

    if (!finished && reader.hasError()) {
        set_error(error, "cannot parse the navigation document: " +
                             reader.errorString().toStdString());
        return false;
    }
    return true;
}

bool parse_ncx(const std::string& xml, const std::string& package_dir,
               std::vector<EpubTocEntry>* toc, std::string* error) {
    struct NavPoint {
        std::size_t entry = 0;
        bool in_label = false;
        bool has_title = false;
        bool has_href = false;
    };

    QXmlStreamReader reader(QByteArray::fromStdString(xml));
    QStringList stack;
    std::vector<NavPoint> points;
    bool in_nav_map = false;
    bool finished = false;

    while (!reader.atEnd() && !finished) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: {
                const QString name = reader.name().toString();
                if (!in_nav_map) {
                    if (name == QLatin1String("navMap")) {
                        in_nav_map = true;
                    }
                    stack.push_back(name);
                    break;
                }
                if (name == QLatin1String("navPoint")) {
                    const int level = static_cast<int>(points.size());
                    points.push_back(NavPoint{toc->size()});
                    toc->push_back(EpubTocEntry{level, std::string(), std::string()});
                    stack.push_back(name);
                    break;
                }
                if (!points.empty() && name == QLatin1String("navLabel")) {
                    points.back().in_label = true;
                    stack.push_back(name);
                    break;
                }
                if (!points.empty() && points.back().in_label && !points.back().has_title &&
                    name == QLatin1String("text")) {
                    const QString title =
                        reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    (*toc)[points.back().entry].title = title.toStdString();
                    points.back().has_title = true;
                    break;
                }
                if (!points.empty() && !points.back().has_href &&
                    name == QLatin1String("content")) {
                    const std::optional<std::string> href = resolve_href(
                        attribute_value(reader.attributes(), QStringLiteral("src"))
                            .toStdString(),
                        package_dir);
                    if (href.has_value()) {
                        (*toc)[points.back().entry].href = *href;
                        points.back().has_href = true;
                    }
                    stack.push_back(name);
                    break;
                }
                stack.push_back(name);
                break;
            }
            case QXmlStreamReader::EndElement: {
                const QString name = stack.isEmpty() ? QString() : stack.last();
                if (!stack.isEmpty()) {
                    stack.removeLast();
                }
                if (name == QLatin1String("navLabel") && !points.empty()) {
                    points.back().in_label = false;
                } else if (name == QLatin1String("navPoint") && !points.empty()) {
                    const NavPoint point = points.back();
                    points.pop_back();
                    // A navPoint without a usable content target is dropped
                    // together with everything nested after it.
                    if (!point.has_href) {
                        toc->resize(point.entry);
                    }
                } else if (name == QLatin1String("navMap") && in_nav_map) {
                    finished = true;
                }
                break;
            }
            default:
                break;
        }
    }

    if (!finished && reader.hasError()) {
        set_error(error, "cannot parse the NCX document: " + reader.errorString().toStdString());
        return false;
    }
    return true;
}

bool parse_opf(zip_t* zip, const std::string& xml, const std::string& package_dir,
               Package* package, std::string* error) {
    std::vector<ManifestItem> manifest;
    std::unordered_map<std::string, std::size_t> manifest_index;
    std::vector<std::string> spine_idrefs;
    std::string cover_id;
    std::string spine_toc_id;

    QXmlStreamReader reader(QByteArray::fromStdString(xml));
    QStringList stack;
    while (!reader.atEnd()) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: {
                const QString name = reader.name().toString();
                const QXmlStreamAttributes attributes = reader.attributes();
                const bool in_metadata =
                    !stack.isEmpty() && stack.last() == QLatin1String("metadata");

                if (in_metadata && (name == QLatin1String("title") ||
                                    name == QLatin1String("creator") ||
                                    name == QLatin1String("language"))) {
                    const QString text =
                        reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    if (name == QLatin1String("title")) {
                        if (package->metadata.title.empty() && !text.isEmpty()) {
                            package->metadata.title = text.toStdString();
                        }
                    } else if (name == QLatin1String("creator")) {
                        if (!package->metadata.author.has_value() && !text.isEmpty()) {
                            package->metadata.author = text.toStdString();
                        }
                    } else if (!package->metadata.language.has_value() && !text.isEmpty()) {
                        package->metadata.language = text.toStdString();
                    }
                    continue;
                }
                if (in_metadata && name == QLatin1String("meta") &&
                    attribute_value(attributes, QStringLiteral("name")) ==
                        QLatin1String("cover")) {
                    const QString content = attribute_value(attributes, QStringLiteral("content"));
                    if (cover_id.empty() && !content.isEmpty()) {
                        cover_id = content.toStdString();
                    }
                }
                if (!stack.isEmpty() && stack.last() == QLatin1String("manifest") &&
                    name == QLatin1String("item")) {
                    ManifestItem item;
                    item.id = attribute_value(attributes, QStringLiteral("id")).toStdString();
                    item.href = attribute_value(attributes, QStringLiteral("href")).toStdString();
                    item.media_type =
                        attribute_value(attributes, QStringLiteral("media-type")).toStdString();
                    item.properties =
                        attribute_value(attributes, QStringLiteral("properties")).toStdString();
                    if (!item.id.empty() && !item.href.empty()) {
                        manifest_index.emplace(item.id, manifest.size());
                        manifest.push_back(std::move(item));
                    }
                }
                if (name == QLatin1String("spine")) {
                    const QString toc = attribute_value(attributes, QStringLiteral("toc"));
                    if (spine_toc_id.empty() && !toc.isEmpty()) {
                        spine_toc_id = toc.toStdString();
                    }
                }
                if (!stack.isEmpty() && stack.last() == QLatin1String("spine") &&
                    name == QLatin1String("itemref")) {
                    const QString idref = attribute_value(attributes, QStringLiteral("idref"));
                    if (!idref.isEmpty()) {
                        spine_idrefs.push_back(idref.toStdString());
                    }
                }
                stack.push_back(name);
                break;
            }
            case QXmlStreamReader::EndElement:
                if (!stack.isEmpty()) {
                    stack.removeLast();
                }
                break;
            default:
                break;
        }
    }

    if (reader.hasError()) {
        set_error(error, "cannot parse the package document: " +
                             reader.errorString().toStdString());
        return false;
    }

    for (const std::string& idref : spine_idrefs) {
        const auto found = manifest_index.find(idref);
        if (found == manifest_index.end()) {
            continue;
        }
        const ManifestItem& item = manifest[found->second];
        const std::optional<std::string> href = resolve_href(item.href, package_dir);
        if (!href.has_value()) {
            continue;
        }
        package->spine.push_back(EpubSpineItem{idref, *href, item.media_type});
    }

    const ManifestItem* cover = nullptr;
    for (const ManifestItem& item : manifest) {
        if (has_property(item.properties, "cover-image")) {
            cover = &item;
            break;
        }
    }
    if (cover == nullptr && !cover_id.empty()) {
        const auto found = manifest_index.find(cover_id);
        if (found != manifest_index.end()) {
            cover = &manifest[found->second];
        }
    }
    if (cover != nullptr) {
        package->metadata.cover_href = resolve_href(cover->href, package_dir);
    }

    const ManifestItem* nav = nullptr;
    for (const ManifestItem& item : manifest) {
        if (has_property(item.properties, "nav")) {
            nav = &item;
            break;
        }
    }
    if (nav != nullptr) {
        const std::optional<std::string> href = resolve_href(nav->href, package_dir);
        if (href.has_value()) {
            std::string data;
            if (!read_entry(zip, *href, &data)) {
                set_error(error, "cannot read the navigation document " + *href);
                return false;
            }
            if (!parse_nav(data, package_dir, &package->toc, error)) {
                return false;
            }
        }
    } else if (!spine_toc_id.empty()) {
        const auto found = manifest_index.find(spine_toc_id);
        if (found != manifest_index.end()) {
            const std::optional<std::string> href =
                resolve_href(manifest[found->second].href, package_dir);
            if (href.has_value()) {
                std::string data;
                if (!read_entry(zip, *href, &data)) {
                    set_error(error, "cannot read the NCX document " + *href);
                    return false;
                }
                if (!parse_ncx(data, package_dir, &package->toc, error)) {
                    return false;
                }
            }
        }
    }

    return true;
}

}  // namespace

struct EpubBook::Impl {
    zip_t* zip = nullptr;
    EpubMetadata metadata;
    std::vector<EpubSpineItem> spine;
    std::vector<EpubTocEntry> toc;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    ~Impl() {
        if (zip != nullptr) {
            (void)zip_close(zip);
        }
    }
};

EpubBook::EpubBook(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

EpubBook::EpubBook(EpubBook&& other) noexcept = default;
EpubBook& EpubBook::operator=(EpubBook&& other) noexcept = default;
EpubBook::~EpubBook() = default;

std::optional<EpubBook> EpubBook::open(const std::filesystem::path& path, std::string* error) {
    const std::string file_name = path.string();
    int zip_error = 0;
    zip_t* zip = zip_open(file_name.c_str(), ZIP_RDONLY, &zip_error);
    if (zip == nullptr) {
        zip_error_t detail;
        zip_error_init_with_code(&detail, zip_error);
        set_error(error, std::string("cannot open EPUB: ") + zip_error_strerror(&detail));
        zip_error_fini(&detail);
        return std::nullopt;
    }

    std::unique_ptr<Impl> impl = std::make_unique<Impl>();
    impl->zip = zip;

    std::string container;
    if (!read_entry(impl->zip, std::string(kContainerEntry), &container)) {
        set_error(error, "EPUB is missing META-INF/container.xml");
        return std::nullopt;
    }

    std::string rootfile;
    if (!parse_container(container, &rootfile, error)) {
        return std::nullopt;
    }

    std::string package_xml;
    if (!read_entry(impl->zip, rootfile, &package_xml)) {
        set_error(error, "EPUB is missing the package document " + rootfile);
        return std::nullopt;
    }

    const std::string package_dir =
        std::filesystem::path(rootfile).parent_path().generic_string();
    Package package;
    if (!parse_opf(impl->zip, package_xml, package_dir, &package, error)) {
        return std::nullopt;
    }

    impl->metadata = std::move(package.metadata);
    impl->spine = std::move(package.spine);
    impl->toc = std::move(package.toc);
    return EpubBook(std::move(impl));
}

const EpubMetadata& EpubBook::metadata() const {
    return impl_->metadata;
}

const std::vector<EpubSpineItem>& EpubBook::spine() const {
    return impl_->spine;
}

const std::vector<EpubTocEntry>& EpubBook::toc() const {
    return impl_->toc;
}

std::optional<std::string> EpubBook::read(std::string_view href) const {
    const std::string name(href.substr(0, href.find('#')));
    if (name.empty()) {
        return std::nullopt;
    }
    std::string data;
    if (!read_entry(impl_->zip, name, &data)) {
        return std::nullopt;
    }
    return data;
}

}  // namespace niratan::novels

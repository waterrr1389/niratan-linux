// SPDX-License-Identifier: GPL-3.0-or-later

#include "ttu_convert.h"

#include "services/reader/book_session.h"
#include "ttu_convert_xhtml.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUuid>

#include <zip.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <memory>
#include <ranges>
#include <type_traits>

namespace niratan::sync {
namespace {

namespace fs = std::filesystem;
constexpr double reference_epoch = 978307200;
using Archive = std::unique_ptr<zip_t, decltype(&zip_discard)>;

QString text(std::string_view value) {
    return QString::fromUtf8(value.data(), value.size());
}

std::optional<QString> utf8(const std::optional<std::string>& bytes) {
    if (!bytes || !QByteArrayView(bytes->data(), bytes->size()).isValidUtf8()) {
        return std::nullopt;
    }
    return text(*bytes);
}

bool safe_path(std::string_view name) {
    const fs::path path(name);
    return !name.empty() && !name.starts_with('/') && name.find('\\') == name.npos &&
        name.find('\0') == name.npos && std::ranges::none_of(path, [](const auto& component) {
            return component == ".." || component == ".";
        });
}

class ZipWriter {
   public:
    explicit ZipWriter(const fs::path& path)
        : archive_(zip_open(path.c_str(), ZIP_CREATE | ZIP_EXCL, nullptr), zip_discard) {}

    bool bytes(const std::string& name, QByteArray bytes, int compression = ZIP_CM_DEFLATE) {
        if (!archive_ || !safe_path(name)) {
            return false;
        }
        const auto& data = buffers_.emplace_back(std::move(bytes));
        return add(name, zip_source_buffer(archive_.get(), data.constData(), data.size(), 0),
                   compression);
    }

    bool file(const std::string& name, const fs::path& path, int compression) {
        return archive_ && safe_path(name) &&
            add(name, zip_source_file(archive_.get(), path.c_str(), 0, -1), compression);
    }

    bool finish() {
        if (!archive_ || zip_close(archive_.get()) != 0) {
            return false;
        }
        archive_.release();
        return true;
    }

   private:
    bool add(const std::string& name, zip_source_t* source, int compression) {
        if (!source) {
            return false;
        }
        const auto index = zip_file_add(archive_.get(), name.c_str(), source, ZIP_FL_ENC_UTF_8);
        if (index < 0) {
            zip_source_free(source);
            return false;
        }
        return zip_set_file_compression(archive_.get(), index, compression, 0) == 0;
    }

    std::list<QByteArray> buffers_;
    Archive archive_;
};

bool unpack_file(zip_t* archive, zip_uint64_t index, const fs::path& path) {
    std::unique_ptr<zip_file_t, decltype(&zip_fclose)> source(zip_fopen_index(archive, index, 0),
                                                              zip_fclose);
    QFile target(text(path.string()));
    if (!source || !target.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        return false;
    }
    std::array<char, 32768> buffer;
    for (;;) {
        const auto count = zip_fread(source.get(), buffer.data(), buffer.size());
        if (count <= 0) {
            return count == 0 && zip_fclose(source.release()) == 0 && target.flush();
        }
        if (target.write(buffer.data(), count) != count) {
            return false;
        }
    }
}

bool unpack(const fs::path& zip, const fs::path& root) {
    Archive archive(zip_open(zip.c_str(), ZIP_RDONLY, nullptr), zip_discard);
    if (!archive) {
        return false;
    }
    const auto count = zip_get_num_entries(archive.get(), 0);
    for (zip_int64_t index = 0; index < count; ++index) {
        const auto name = zip_get_name(archive.get(), index, ZIP_FL_ENC_GUESS);
        if (!name || !safe_path(name)) {
            return false;
        }
        const bool directory = std::string_view(name).ends_with('/');
        const auto path = root / name;
        fs::create_directories(directory ? path : path.parent_path());
        if (!directory && !unpack_file(archive.get(), index, path)) {
            return false;
        }
    }
    return count >= 0;
}

struct StaticData {
    QString title;
    QString stylesheet;
    QString html;
    std::vector<detail::Section> sections;
};

std::optional<StaticData> load_static_data(const fs::path& path) {
    QFile file(text(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    if (!root.value("title").isString() || !root.value("styleSheet").isString() ||
        !root.value("elementHtml").isString() || !root.value("sections").isArray()) {
        return std::nullopt;
    }
    StaticData data{root.value("title").toString(),
                    root.value("styleSheet").toString(),
                    root.value("elementHtml").toString(),
                    {}};
    const auto sections = root.value("sections").toArray();
    for (const auto item : sections) {
        const auto object = item.toObject();
        const auto reference = object.value("reference");
        detail::Section section;
        const auto optional = [&](const char* key, auto& field) {
            const auto value = object.value(QLatin1StringView(key));
            if (value.isNull() || value.isUndefined()) {
                return true;
            }
            using Type = typename std::remove_reference_t<decltype(field)>::value_type;
            if constexpr (std::is_same_v<Type, QString>) {
                if (!value.isString()) {
                    return false;
                }
                field = value.toString();
            } else {
                const auto number = value.toInteger(0);
                if (number != value.toInteger(1)) {
                    return false;
                }
                field = number;
            }
            return true;
        };
        std::optional<qint64> weight;
        if (!reference.isString() || !optional("charactersWeight", weight) || !weight ||
            !optional("label", section.label) || !optional("startCharacter", section.start) ||
            !optional("characters", section.characters) ||
            !optional("parentChapter", section.parent)) {
            return std::nullopt;
        }
        section.reference = reference.toString();
        section.weight = *weight;
        data.sections.push_back(std::move(section));
    }
    return data;
}

QByteArray encode(const StaticData& data) {
    QJsonArray sections;
    for (const auto& section : data.sections) {
        QJsonObject value{{"reference", section.reference}, {"charactersWeight", section.weight}};
        const auto add = [&](const char* key, const auto& field) {
            if (field) {
                value.insert(QLatin1StringView(key), *field);
            }
        };
        add("startCharacter", section.start);
        add("characters", section.characters);
        add("label", section.label);
        add("parentChapter", section.parent);
        sections.append(value);
    }
    return QJsonDocument(QJsonObject{{"title", data.title},
                                     {"styleSheet", data.stylesheet},
                                     {"elementHtml", data.html},
                                     {"sections", sections}})
        .toJson(QJsonDocument::Compact);
}

std::optional<QString> label_for(const novels::EpubBook& book, std::string_view chapter) {
    for (const auto& item : book.toc()) {
        const auto href = std::string_view(item.href).substr(0, item.href.find('#'));
        if (!href.empty() && (href.ends_with(chapter) || chapter.ends_with(href))) {
            return text(item.title);
        }
    }
    return std::nullopt;
}

std::optional<StaticData> export_data(const novels::EpubBook& book, const books::BookInfo& info,
                                      const std::string& title) {
    StaticData data{text(title), {}, {}, {}};
    std::optional<QString> parent;
    for (const auto& chapter : book.spine()) {
        const auto manifest =
            std::ranges::find(book.manifest(), chapter.idref, &novels::EpubManifestItem::id);
        if (manifest == book.manifest().end()) {
            continue;
        }
        const auto content = utf8(book.read(manifest->href));
        if (!content) {
            return std::nullopt;
        }
        const auto found = info.chapter_info.find(chapter.href);
        const auto count = found == info.chapter_info.end() ? 0 : found->second.chapter_count;
        const auto start = found == info.chapter_info.end() ? 0 : found->second.current_total;
        const auto reference = "ttu-" + text(chapter.idref);
        auto label = label_for(book, chapter.href);
        if (!label && !parent) {
            label = QStringLiteral("Preface");
        }
        data.html += detail::export_element(reference, *content, text(manifest->path), count == 0);
        if (label) {
            parent = reference;
            data.sections.push_back({reference, std::max(count, 1), label, start, 0, {}});
        } else {
            data.sections.push_back({reference, std::max(count, 1), {}, {}, {}, parent});
        }
    }
    qint64 end = info.character_count;
    for (auto& section : data.sections | std::views::reverse) {
        if (section.start) {
            section.characters = end - *section.start;
            end = *section.start;
        }
    }
    for (const auto& item : book.manifest()) {
        if (item.media_type == "text/css") {
            const auto css = utf8(book.read(item.href));
            if (!css) {
                return std::nullopt;
            }
            data.stylesheet += *css;
        }
    }
    return data;
}

bool export_images(ZipWriter& archive, const novels::EpubBook& book) {
    for (const auto& item : book.manifest()) {
        if (item.media_type == "image/gif" || item.media_type == "image/jpeg" ||
            item.media_type == "image/png" || item.media_type == "image/svg+xml") {
            if (const auto bytes = book.read(item.href); bytes &&
                !archive.bytes("blobs/" + item.path, QByteArray::fromStdString(*bytes),
                               ZIP_CM_STORE)) {
                return false;
            }
        }
    }
    return true;
}

QStringList collect_images(const fs::path& blobs) {
    std::vector<fs::path> images;
    if (fs::exists(blobs)) {
        for (const auto& entry : fs::recursive_directory_iterator(blobs)) {
            const auto extension = text(entry.path().extension().string()).toLower();
            if (entry.is_regular_file() &&
                (extension == ".jpg" || extension == ".jpeg" || extension == ".png" ||
                 extension == ".gif" || extension == ".svg")) {
                images.push_back(entry.path().lexically_relative(blobs));
            }
        }
    }
    std::ranges::sort(images, [](const auto& left, const auto& right) {
        return left.filename() < right.filename();
    });
    QStringList result;
    for (const auto& image : images) {
        result.append(text(image.generic_string()));
    }
    return result;
}

bool make_epub(const fs::path& path, const fs::path& blobs, const StaticData& data,
               const QString& uuid) {
    constexpr auto container = R"(<?xml version="1.0"?>
<container
 version="1.0"
 xmlns="urn:oasis:names:tc:opendocument:xmlns:container"
>
<rootfiles>
<rootfile
 full-path="item/standard.opf"
 media-type="application/oebps-package+xml"
/>
</rootfiles>
</container>)";
    ZipWriter archive(path);
    if (!archive.bytes("mimetype", "application/epub+zip", ZIP_CM_STORE) ||
        !archive.bytes("item/stylesheet.css", data.stylesheet.toUtf8()) ||
        !archive.bytes("META-INF/container.xml", container)) {
        return false;
    }
    const auto images = collect_images(blobs);
    for (const auto& image : images) {
        const auto name = image.toStdString();
        if (!archive.file("item/" + name, blobs / name, ZIP_CM_DEFLATE)) {
            return false;
        }
    }
    const auto files = detail::split_elements(data.html, data.sections);
    for (const auto& file : files) {
        if (!archive.bytes("item/xhtml/" + file.name.toStdString(),
                           detail::xhtml(file, data.title).toUtf8())) {
            return false;
        }
    }
    return archive.bytes("item/navigation-documents.xhtml", detail::navigation(files).toUtf8()) &&
        archive.bytes("item/standard.opf", detail::opf(images, files, data.title, uuid).toUtf8()) &&
        archive.finish();
}

}  // namespace

TtuConverter::TtuConverter(books::BookStorage& storage, TtuConvertOptions options)
    : storage_(storage), options_(std::move(options)) {
    if (!options_.now_unix_ms) {
        options_.now_unix_ms = QDateTime::currentMSecsSinceEpoch;
    }
    if (!options_.new_uuid) {
        options_.new_uuid = [] {
            return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
        };
    }
}

std::optional<fs::path> TtuConverter::export_zip(const fs::path& book_folder,
                                                 const fs::path& output_dir) const {
    const auto folder = book_folder.filename().string();
    const auto metadata = storage_.load_metadata(folder);
    const auto info = storage_.load_book_info(folder);
    if (!metadata || !info || !metadata->epub) {
        return std::nullopt;
    }
    const auto epub = storage_.epub_path(*metadata);
    const auto book = epub ? novels::EpubBook::open(*epub) : std::nullopt;
    if (!book) {
        return std::nullopt;
    }
    const auto now = options_.now_unix_ms();
    const double access = (metadata->last_access + reference_epoch) * 1000;
    constexpr auto minimum = static_cast<double>(std::numeric_limits<std::int64_t>::min());
    if (!std::isfinite(access) || access < minimum || access >= -minimum) {
        return std::nullopt;
    }
    const auto path = output_dir /
        ("bookdata_1_6_" + std::to_string(info->character_count) + "_" + std::to_string(now) + "_" +
         std::to_string(static_cast<std::int64_t>(access)) + ".zip");
    const auto data = export_data(*book, *info, metadata->title);
    if (!data) {
        return std::nullopt;
    }
    ZipWriter archive(path);
    if (!archive.bytes("staticdata.json", encode(*data)) || !export_images(archive, *book)) {
        return std::nullopt;
    }
    if (metadata->cover) {
        const auto cover = storage_.cover_path(*metadata);
        if (!cover ||
            !archive.file("cover" + (cover->has_extension() ? cover->extension().string() : "."),
                          *cover, ZIP_CM_STORE)) {
            return std::nullopt;
        }
    }
    return archive.finish() ? std::optional{path} : std::nullopt;
}

std::optional<fs::path> TtuConverter::import_zip(const fs::path& zip,
                                                 const fs::path& books_dir) const {
    if (books_dir != storage_.books_directory()) {
        return std::nullopt;
    }
    try {
        const QTemporaryDir temporary;
        const fs::path root = temporary.path().toStdString();
        if (!temporary.isValid() || !unpack(zip, root)) {
            return std::nullopt;
        }
        const auto data = load_static_data(root / "staticdata.json");
        if (!data) {
            return std::nullopt;
        }
        const auto title = data->title.toStdString();
        const auto folder = books::sanitize_file_name(title);
        const auto destination = storage_.book_directory(folder);
        if (!destination) {
            return std::nullopt;
        }
        if (fs::exists(*destination)) {
            return destination;
        }
        fs::create_directories(*destination);
        const auto epub = folder + ".epub";
        if (!make_epub(*destination / epub, root / "blobs", *data, text(options_.new_uuid()))) {
            return std::nullopt;
        }
        std::optional<std::string> cover;
        for (const auto& entry : fs::directory_iterator(root)) {
            const auto name = entry.path().filename().string();
            if (name.starts_with("cover.")) {
                fs::copy(entry.path(), *destination / name, fs::copy_options::recursive);
                cover = "Books/" + folder + "/" + name;
                break;
            }
        }
        const auto session = reader::BookSession::open(*destination / epub);
        if (!session) {
            return std::nullopt;
        }
        books::BookMetadata metadata;
        metadata.id = options_.new_uuid();
        metadata.title = title;
        metadata.epub = epub;
        metadata.cover = std::move(cover);
        metadata.folder = folder;
        metadata.last_access = options_.now_unix_ms() / 1000.0 - reference_epoch;
        if (!storage_.save_metadata(metadata) ||
            !storage_.save_book_info(folder, session->info())) {
            return std::nullopt;
        }
        return destination;
    } catch (const fs::filesystem_error&) {
        return std::nullopt;
    }
}

}  // namespace niratan::sync

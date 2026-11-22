// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/video/library_catalog.h"

#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"
#include "services/video/library_catalog_internal.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>

#include <algorithm>
#include <utility>

namespace niratan::video {
namespace {

// standardizedFileURL: lexical absolute form, no symlink resolution.
QString standardized_path(const QString& path) {
    if (path.isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString source_path_text(const std::filesystem::path& path) {
    return QString::fromStdString(path.string());
}

}  // namespace

std::filesystem::path LibraryStore::default_file_url() {
    const auto root = storage::data_root();
    return root.empty() ? root : root / "video_library.json";
}

LibraryStore::LibraryStore(std::filesystem::path file_url) : file_url_(std::move(file_url)) {
    if (file_url_.empty()) {
        return;
    }
    const auto read = storage::read_json_file(file_url_);
    if (read.status != storage::JsonReadStatus::success || !read.document.isObject()) {
        return;
    }
    if (const auto catalog = detail::catalog_from_json(read.document.object())) {
        catalog_ = *catalog;
    }
}

LibrarySource LibraryStore::add_source(const std::filesystem::path& folder) {
    const QString path = standardized_path(source_path_text(folder));
    for (const auto& source : catalog_.sources) {
        if (source.path == path) {
            return source;
        }
    }
    LibrarySource source;
    source.id = QUuid::createUuid();
    source.name = QFileInfo(path).fileName();
    source.path = path;
    catalog_.sources.append(source);
    std::stable_sort(catalog_.sources.begin(), catalog_.sources.end(),
                     [](const LibrarySource& left, const LibrarySource& right) {
                         return localized_standard_compare(left.name, right.name) < 0;
                     });
    save();
    return source;
}

void LibraryStore::remove_source(const QUuid& id) {
    QSet<QString> removed;
    for (const auto& item : catalog_.items) {
        if (item.source_id == id) {
            removed.insert(item.path);
        }
    }
    catalog_.sources.removeIf([&id](const LibrarySource& source) { return source.id == id; });
    catalog_.items.removeIf([&id](const LibraryItem& item) { return item.source_id == id; });
    remove_metadata_and_collection_references(removed);
    save();
}

void LibraryStore::save() {
    if (file_url_.empty()) {
        return;
    }
    // Swift replaces the document atomically whatever it held before. The
    // port's writer refuses a damaged target, so the damaged regular file is
    // removed first; a directory in the target's place still fails the write.
    if (storage::read_json_file(file_url_).status == storage::JsonReadStatus::invalid) {
        std::error_code ignored;
        if (std::filesystem::is_regular_file(file_url_, ignored)) {
            std::filesystem::remove(file_url_, ignored);
        }
    }
    storage::write_json_file(file_url_, QJsonDocument(detail::catalog_to_json(catalog_)));
}

}  // namespace niratan::video

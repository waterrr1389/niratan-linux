// SPDX-License-Identifier: GPL-3.0-or-later
//
// Folder scanning for the local video library, following
// VideoLibraryStore.scanSource, scanItems, resolveSourceURL and
// VideoLibraryStoreError. Hidden entries are skipped whole, directories are
// walked recursively, and only regular files with a media extension become
// items. Directory symlinks are not followed, which keeps a cycle out of the
// walk; file symlinks count as their target, as `isRegularFile` resolves them.

#include "services/video/library_catalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace niratan::video {
namespace {

struct ScanOutcome {
    QList<LibraryItem> items;
    QString error;
};

QString source_not_found_message() {
    return QCoreApplication::translate("VideoLibraryStore", "Video folder not found.");
}

QString source_unavailable_message(const QString& path) {
    return QCoreApplication::translate("VideoLibraryStore",
                                       "Video folder is no longer available.") +
        u' ' + path;
}

// deletingPathExtension().lastPathComponent; an empty stem keeps the name.
QString display_title(const QString& file_name) {
    const qsizetype dot = file_name.lastIndexOf(u'.');
    const QString stem = dot >= 0 ? file_name.left(dot) : file_name;
    return stem.isEmpty() ? file_name : stem;
}

bool item_before(const LibraryItem& left, const LibraryItem& right) {
    const int title = localized_standard_compare(left.title, right.title);
    if (title != 0) {
        return title < 0;
    }
    return localized_standard_compare(left.path, right.path) < 0;
}

void collect(const QString& folder, const QUuid& source_id, const QDateTime& now,
             QHash<QString, LibraryItem>& items) {
    const QFileInfoList entries = QDir(folder).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::NoSort);
    for (const auto& entry : entries) {
        const QString name = entry.fileName();
        if (name.startsWith(u'.')) {
            continue;
        }
        if (entry.isDir()) {
            if (entry.isSymLink()) {
                continue;
            }
            collect(entry.absoluteFilePath(), source_id, now, items);
            continue;
        }
        if (!entry.isFile() || !is_media_file(entry.absoluteFilePath())) {
            continue;
        }
        LibraryItem item;
        item.path = QDir::cleanPath(entry.absoluteFilePath());
        item.source_id = source_id;
        item.title = display_title(name);
        item.parent_folder = entry.dir().dirName();
        item.file_size = entry.size();
        const QDateTime modified = entry.lastModified();
        if (modified.isValid()) {
            item.modified_at = modified.toUTC();
        }
        item.last_seen_at = now;
        items.insert(item.path, item);
    }
}

ScanOutcome scan_items(const LibrarySource& source, const QDateTime& now) {
    const QFileInfo root(source.path);
    if (!root.exists()) {
        return {{}, source_unavailable_message(source.path)};
    }
    if (!root.isDir()) {
        return {{}, source_unavailable_message(source.path)};
    }
    QHash<QString, LibraryItem> items;
    collect(source.path, source.id, now, items);
    QList<LibraryItem> sorted = items.values();
    std::stable_sort(sorted.begin(), sorted.end(), item_before);
    return {sorted, {}};
}

}  // namespace

bool LibraryStore::scan_source(const QUuid& id, QString* error, const QDateTime& now) {
    const auto index = std::find_if(catalog_.sources.begin(), catalog_.sources.end(),
                                    [&id](const LibrarySource& source) { return source.id == id; });
    if (index == catalog_.sources.end()) {
        if (error != nullptr) {
            *error = source_not_found_message();
        }
        return false;
    }

    const ScanOutcome outcome = scan_items(*index, now);
    if (!outcome.error.isEmpty()) {
        index->last_error = outcome.error;
        save();
        if (error != nullptr) {
            *error = outcome.error;
        }
        return false;
    }

    QSet<QString> scanned_paths;
    for (const auto& item : outcome.items) {
        scanned_paths.insert(item.path);
    }
    QSet<QString> old_source_paths;
    for (const auto& item : catalog_.items) {
        if (item.source_id == id) {
            old_source_paths.insert(item.path);
        }
    }
    catalog_.items.removeIf([&id, &scanned_paths](const LibraryItem& item) {
        return item.source_id == id || scanned_paths.contains(item.path);
    });
    catalog_.items.append(outcome.items);
    std::stable_sort(catalog_.items.begin(), catalog_.items.end(), item_before);
    remove_metadata_and_collection_references(old_source_paths - scanned_paths);

    index->path = QDir::cleanPath(QFileInfo(index->path).absoluteFilePath());
    index->name = QFileInfo(index->path).fileName();
    index->last_scanned_at = now;
    index->last_error.reset();
    save();
    return true;
}

bool LibraryStore::scan_all_sources(QString* error) {
    const QList<LibrarySource> sources = catalog_.sources;
    QString first_error;
    for (const auto& source : sources) {
        QString message;
        if (!scan_source(source.id, &message) && first_error.isEmpty()) {
            first_error = message;
        }
    }
    if (!first_error.isEmpty()) {
        if (error != nullptr) {
            *error = first_error;
        }
        return false;
    }
    return true;
}

}  // namespace niratan::video

// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/video/library_catalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace niratan::video {
namespace {

bool is_combining_mark(const QChar character) {
    return character.category() == QChar::Mark_NonSpacing;
}

// Case- and diacritic-insensitive form used for tag de-duplication, as
// `folding(options: [.caseInsensitive, .diacriticInsensitive])`.
QString folded(const QString& text) {
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString result;
    result.reserve(decomposed.size());
    for (const QChar character : decomposed) {
        if (!is_combining_mark(character)) {
            result.append(character);
        }
    }
    return result.toCaseFolded();
}

QString standardized_path(const QString& path) {
    if (path.isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

}  // namespace

bool ItemMetadata::empty() const {
    return !display_title.has_value() && !is_favorite && tags.isEmpty() &&
        collection_ids.isEmpty() && !bound_subtitle_path.has_value();
}

QStringList LibraryStore::normalized_tags(const QStringList& tags) {
    QStringList seen;
    QStringList result;
    for (const auto& tag : tags) {
        const QString cleaned = tag.trimmed();
        if (cleaned.isEmpty()) {
            continue;
        }
        const QString key = folded(cleaned);
        if (seen.contains(key)) {
            continue;
        }
        seen.append(key);
        result.append(cleaned);
    }
    return result;
}

QString LibraryStore::collection_name(const QString& name) {
    const QString cleaned = name.trimmed();
    return cleaned.isEmpty()
        ? QCoreApplication::translate("VideoLibraryStore", "Untitled Collection")
        : cleaned;
}

QSet<QString> LibraryStore::existing_item_paths() const {
    QSet<QString> paths;
    for (const auto& item : catalog_.items) {
        paths.insert(item.path);
    }
    return paths;
}

ItemMetadata LibraryStore::metadata_for_path(const QString& path) const {
    return catalog_.item_metadata_by_path.value(path);
}

void LibraryStore::update_metadata(const QString& path,
                                   const std::function<void(ItemMetadata&)>& mutate) {
    ItemMetadata metadata = metadata_for_path(path);
    mutate(metadata);
    if (metadata.empty()) {
        catalog_.item_metadata_by_path.remove(path);
    } else {
        catalog_.item_metadata_by_path.insert(path, metadata);
    }
    save();
}

void LibraryStore::set_display_title(const QString& path, const std::optional<QString>& title) {
    update_metadata(path, [&title](ItemMetadata& metadata) {
        const QString cleaned = title.value_or(QString()).trimmed();
        metadata.display_title = cleaned.isEmpty() ? std::nullopt : std::optional<QString>(cleaned);
    });
}

void LibraryStore::set_favorite(const QString& path, bool favorite) {
    update_metadata(path, [favorite](ItemMetadata& metadata) { metadata.is_favorite = favorite; });
}

void LibraryStore::set_tags(const QString& path, const QStringList& tags) {
    const QStringList normalized = normalized_tags(tags);
    update_metadata(path, [&normalized](ItemMetadata& metadata) { metadata.tags = normalized; });
}

void LibraryStore::bind_subtitle(const QString& path, const std::optional<QString>& subtitle_path) {
    update_metadata(path, [&subtitle_path](ItemMetadata& metadata) {
        metadata.bound_subtitle_path = subtitle_path.has_value()
            ? std::optional<QString>(standardized_path(*subtitle_path))
            : std::nullopt;
    });
}

void LibraryStore::remove_metadata_and_collection_references(const QSet<QString>& paths) {
    if (paths.isEmpty()) {
        return;
    }
    for (const auto& path : paths) {
        catalog_.item_metadata_by_path.remove(path);
    }
    for (auto& collection : catalog_.collections) {
        collection.item_paths.removeIf(
            [&paths](const QString& item_path) { return paths.contains(item_path); });
    }
    normalize_collection_membership();
}

void LibraryStore::normalize_collection_membership() {
    QHash<QString, QList<QUuid>> ids_by_path;
    for (const auto& collection : catalog_.collections) {
        for (const auto& path : collection.item_paths) {
            ids_by_path[path].append(collection.id);
        }
    }
    const QSet<QString> existing = existing_item_paths();
    const QStringList keys = catalog_.item_metadata_by_path.keys();
    for (const auto& path : keys) {
        auto entry = catalog_.item_metadata_by_path.find(path);
        if (entry == catalog_.item_metadata_by_path.end()) {
            continue;
        }
        if (!existing.contains(path)) {
            catalog_.item_metadata_by_path.erase(entry);
            continue;
        }
        entry->collection_ids = ids_by_path.value(path);
        if (entry->empty()) {
            catalog_.item_metadata_by_path.erase(entry);
        }
    }
    for (auto entry = ids_by_path.begin(); entry != ids_by_path.end(); ++entry) {
        if (!existing.contains(entry.key())) {
            continue;
        }
        ItemMetadata metadata = metadata_for_path(entry.key());
        metadata.collection_ids = entry.value();
        catalog_.item_metadata_by_path.insert(entry.key(), metadata);
    }
}

Collection LibraryStore::create_collection(const QString& name, const QStringList& item_paths) {
    Collection collection;
    collection.id = QUuid::createUuid();
    collection.name = collection_name(name);
    collection.kind = CollectionKind::manual;

    QStringList seen;
    const QSet<QString> existing = existing_item_paths();
    for (const auto& path : item_paths) {
        if (!existing.contains(path) || seen.contains(path)) {
            continue;
        }
        seen.append(path);
        collection.item_paths.append(path);
    }

    catalog_.collections.append(collection);
    std::stable_sort(catalog_.collections.begin(), catalog_.collections.end(),
                     [](const Collection& left, const Collection& right) {
                         return localized_standard_compare(left.name, right.name) < 0;
                     });
    for (const auto& path : collection.item_paths) {
        ItemMetadata metadata = metadata_for_path(path);
        if (!metadata.collection_ids.contains(collection.id)) {
            metadata.collection_ids.append(collection.id);
            catalog_.item_metadata_by_path.insert(path, metadata);
        }
    }
    save();
    return collection;
}

Collection LibraryStore::create_smart_collection(const QString& name,
                                                 const QList<SmartRule>& rules) {
    Collection collection;
    collection.id = QUuid::createUuid();
    collection.name = collection_name(name);
    collection.kind = CollectionKind::smart;
    for (const auto& rule : rules) {
        SmartRule normalized = rule;
        normalized.value = rule.value.trimmed();
        collection.smart_rules.append(normalized);
    }
    catalog_.collections.append(collection);
    std::stable_sort(catalog_.collections.begin(), catalog_.collections.end(),
                     [](const Collection& left, const Collection& right) {
                         return localized_standard_compare(left.name, right.name) < 0;
                     });
    save();
    return collection;
}

void LibraryStore::update_collection(const QUuid& id, const QString& name,
                                     const QStringList& item_paths) {
    const auto entry =
        std::find_if(catalog_.collections.begin(), catalog_.collections.end(),
                     [&id](const Collection& collection) { return collection.id == id; });
    if (entry == catalog_.collections.end()) {
        return;
    }
    QStringList paths;
    QSet<QString> seen;
    const QSet<QString> existing = existing_item_paths();
    for (const auto& path : item_paths) {
        if (!existing.contains(path) || seen.contains(path)) {
            continue;
        }
        seen.insert(path);
        paths.append(path);
    }
    entry->name = collection_name(name);
    entry->kind = CollectionKind::manual;
    entry->item_paths = paths;
    entry->smart_rules.clear();
    normalize_collection_membership();
    save();
}

void LibraryStore::update_smart_collection(const QUuid& id, const QString& name,
                                           const QList<SmartRule>& rules) {
    const auto entry =
        std::find_if(catalog_.collections.begin(), catalog_.collections.end(),
                     [&id](const Collection& collection) { return collection.id == id; });
    if (entry == catalog_.collections.end()) {
        return;
    }
    entry->name = collection_name(name);
    entry->kind = CollectionKind::smart;
    entry->item_paths.clear();
    entry->smart_rules.clear();
    for (const auto& rule : rules) {
        SmartRule normalized = rule;
        normalized.value = rule.value.trimmed();
        entry->smart_rules.append(normalized);
    }
    normalize_collection_membership();
    save();
}

void LibraryStore::remove_collection(const QUuid& id) {
    catalog_.collections.removeIf(
        [&id](const Collection& collection) { return collection.id == id; });
    const QStringList keys = catalog_.item_metadata_by_path.keys();
    for (const auto& path : keys) {
        auto entry = catalog_.item_metadata_by_path.find(path);
        if (entry == catalog_.item_metadata_by_path.end()) {
            continue;
        }
        entry->collection_ids.removeAll(id);
        if (entry->empty()) {
            catalog_.item_metadata_by_path.erase(entry);
        }
    }
    save();
}

int LibraryStore::remove_missing_items(std::optional<QUuid> source_id) {
    QSet<QString> missing;
    for (const auto& item : catalog_.items) {
        if (source_id && item.source_id != *source_id) {
            continue;
        }
        if (!QFileInfo::exists(item.path)) {
            missing.insert(item.path);
        }
    }
    if (missing.isEmpty()) {
        return 0;
    }
    catalog_.items.removeIf(
        [&missing](const LibraryItem& item) { return missing.contains(item.path); });
    remove_metadata_and_collection_references(missing);
    save();
    return int(missing.size());
}

}  // namespace niratan::video

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The local video library document and its folder scanning, following
// Features/Video/VideoLibraryStore.swift: `VideoLibrarySource`,
// `VideoLibraryItem`, `VideoLibraryItemMetadata`,
// `VideoLibraryCollectionKind`, `VideoLibrarySmartRuleField`,
// `VideoLibrarySmartRuleMatch`, `VideoLibrarySmartRule`,
// `VideoLibraryCollection`, `VideoLibraryCatalog`, and the store methods
// `addSource`, `removeSource`, `scanAllSources`, `scanSource`, `scanItems`,
// `displayTitle(for:)`, `itemSort`, `save`, `loadCatalog`, `defaultFileURL`,
// `metadata(forPath:)`, `setDisplayTitle`, `setFavorite`, `setTags`,
// `bindSubtitle`, `createCollection`, `createSmartCollection`,
// `updateCollection`, `updateSmartCollection`, `removeCollection`,
// `removeMissingItems`, `normalizedTags`, `collectionName`,
// `uniqueExistingPaths`, `normalizeCollectionMembership`,
// `removeMetadataAndCollectionReferences`, and `VideoLibraryStoreError`.
//
// The document is <data root>/video_library.json. It holds the members
// `sources`, `items`, `remoteItems`, `itemMetadataByPath` and `collections`;
// `remoteItems` is ignored on read and always written as an empty array
// because media servers and remote links are out of scope. A missing or
// damaged document reads as the empty catalog and is rewritten only by the
// first successful change. Writes are atomic and a failed write is ignored:
// the in-memory catalog keeps the change, as `save()` swallows its error.
//
// Dates are seconds since 2001-01-01 UTC, the default Foundation date
// encoding. UUIDs are uppercase strings without braces. The macOS
// security-scoped `bookmark` has no Linux meaning and is neither read nor
// written; a source is resolved from its stored path.
//
// Sources are keyed by id, items by their absolute standardized path. Scan
// failures are reported as the localized message Swift throws, and also
// stored in the source's `last_error` after the document was saved, so a
// caller can surface either.

#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <functional>
#include <filesystem>
#include <optional>

#include "services/video/media_types.h"

namespace niratan::video {

struct LibrarySource {
    QUuid id;
    QString name;
    QString path;
    std::optional<QDateTime> last_scanned_at;
    std::optional<QString> last_error;

    bool operator==(const LibrarySource&) const = default;
};

struct LibraryItem {
    QString path;
    QUuid source_id;
    QString title;
    QString parent_folder;
    qint64 file_size = 0;
    std::optional<QDateTime> modified_at;
    QDateTime last_seen_at;

    bool operator==(const LibraryItem&) const = default;
};

struct ItemMetadata {
    std::optional<QString> display_title;
    bool is_favorite = false;
    QStringList tags;
    QList<QUuid> collection_ids;
    std::optional<QString> bound_subtitle_path;

    // Foundation's synthesized `isEmpty`: every member at its default.
    bool empty() const;

    bool operator==(const ItemMetadata&) const = default;
};

enum class CollectionKind { manual, smart };

enum class SmartRuleField {
    file_name,
    parent_folder,
    path,
    tag,
    has_bound_subtitle,
    playback_state
};

enum class SmartRuleMatch { contains, equals, is_true };

struct SmartRule {
    QUuid id;
    SmartRuleField field = SmartRuleField::file_name;
    SmartRuleMatch match = SmartRuleMatch::contains;
    QString value;

    bool operator==(const SmartRule&) const = default;
};

struct Collection {
    QUuid id;
    QString name;
    CollectionKind kind = CollectionKind::manual;
    QStringList item_paths;
    QList<SmartRule> smart_rules;

    bool operator==(const Collection&) const = default;
};

struct LibraryCatalog {
    QList<LibrarySource> sources;
    QList<LibraryItem> items;
    QHash<QString, ItemMetadata> item_metadata_by_path;
    QList<Collection> collections;
};

class LibraryStore {
   public:
    // <data root>/video_library.json.
    static std::filesystem::path default_file_url();

    explicit LibraryStore(std::filesystem::path file_url = default_file_url());

    const LibraryCatalog& catalog() const { return catalog_; }

    // addSource: standardizes the folder, returns the existing source for a
    // known path, otherwise appends and saves one named after the folder.
    // Sources stay sorted by name.
    LibrarySource add_source(const std::filesystem::path& folder);

    // removeSource: drops the source, its items, their metadata and their
    // collection item paths. Video files are never touched.
    void remove_source(const QUuid& id);

    // scanSource: false when the source is unknown or its folder is gone;
    // `error` then receives the message Swift throws. Both cases store the
    // message in the source's last_error and save before returning. A
    // successful scan records `now` as last_scanned_at and clears the error.
    bool scan_source(const QUuid& id, QString* error = nullptr,
                     const QDateTime& now = QDateTime::currentDateTimeUtc());
    // scanAllSources: scans every source, keeps the first error and reports it
    // after the loop, as Swift's scanAllSources does.
    bool scan_all_sources(QString* error = nullptr);

    ItemMetadata metadata_for_path(const QString& path) const;
    void set_display_title(const QString& path, const std::optional<QString>& title);
    void set_favorite(const QString& path, bool favorite);
    void set_tags(const QString& path, const QStringList& tags);
    void bind_subtitle(const QString& path, const std::optional<QString>& subtitle_path);

    // createCollection / createSmartCollection: the name is trimmed, an empty
    // name becomes "Untitled Collection", paths are limited to catalog items
    // and de-duplicated, rules are trimmed, and the saved collection is
    // returned. Collections stay sorted by name.
    Collection create_collection(const QString& name, const QStringList& item_paths);
    Collection create_smart_collection(const QString& name, const QList<SmartRule>& rules);
    void update_collection(const QUuid& id, const QString& name, const QStringList& item_paths);
    void update_smart_collection(const QUuid& id, const QString& name,
                                 const QList<SmartRule>& rules);
    void remove_collection(const QUuid& id);

    // removeMissingItems: removes catalog items whose path is no longer a
    // file, from one source or all, and returns the count.
    int remove_missing_items(std::optional<QUuid> source_id = std::nullopt);

    // normalizedTags and collectionName are public because the view model
    // and the inspector both write through them.
    static QStringList normalized_tags(const QStringList& tags);
    static QString collection_name(const QString& name);

   private:
    void save();
    void update_metadata(const QString& path, const std::function<void(ItemMetadata&)>& mutate);
    void remove_metadata_and_collection_references(const QSet<QString>& paths);
    void normalize_collection_membership();
    QSet<QString> existing_item_paths() const;

    std::filesystem::path file_url_;
    LibraryCatalog catalog_;
};

}  // namespace niratan::video

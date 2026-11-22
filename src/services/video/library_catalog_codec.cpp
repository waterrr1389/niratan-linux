// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/video/library_catalog_internal.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QTimeZone>

#include <cmath>

namespace niratan::video::detail {
namespace {

// Seconds between 1970-01-01 and Foundation's 2001-01-01 reference date.
constexpr double kReferenceOffset = 978'307'200;

double reference_seconds(const QDateTime& date) {
    return double(date.toMSecsSinceEpoch()) / 1000.0 - kReferenceOffset;
}

QDateTime date_from_reference(double seconds) {
    return QDateTime::fromMSecsSinceEpoch(std::llround((seconds + kReferenceOffset) * 1000.0),
                                          QTimeZone::UTC);
}

QString uuid_text(const QUuid& id) {
    return id.toString(QUuid::WithoutBraces).toUpper();
}

std::optional<QUuid> parse_uuid(const QJsonValue& value) {
    if (!value.isString()) {
        return std::nullopt;
    }
    QString text = value.toString();
    if (text.startsWith(u'{') && text.endsWith(u'}')) {
        text = text.mid(1, text.size() - 2);
    }
    const QUuid id = QUuid::fromString(text);
    // All-zero is a legitimate id in Swift; only text that does not round-trip
    // as the canonical form is a decode failure.
    if (id.toString(QUuid::WithoutBraces).compare(text, Qt::CaseInsensitive) != 0) {
        return std::nullopt;
    }
    return id;
}

QJsonObject source_to_json(const LibrarySource& source) {
    QJsonObject object{{QStringLiteral("id"), uuid_text(source.id)},
                       {QStringLiteral("name"), source.name},
                       {QStringLiteral("path"), source.path}};
    if (source.last_scanned_at) {
        object.insert(QStringLiteral("lastScannedAt"), reference_seconds(*source.last_scanned_at));
    }
    if (source.last_error) {
        object.insert(QStringLiteral("lastError"), *source.last_error);
    }
    return object;
}

std::optional<LibrarySource> source_from_json(const QJsonObject& object) {
    const auto id = parse_uuid(object.value(QStringLiteral("id")));
    const auto name = object.value(QStringLiteral("name"));
    const auto path = object.value(QStringLiteral("path"));
    if (!id || !name.isString() || !path.isString()) {
        return std::nullopt;
    }
    LibrarySource source;
    source.id = *id;
    source.name = name.toString();
    source.path = path.toString();
    const auto scanned = object.value(QStringLiteral("lastScannedAt"));
    if (scanned.isDouble()) {
        source.last_scanned_at = date_from_reference(scanned.toDouble());
    } else if (!scanned.isUndefined() && !scanned.isNull()) {
        return std::nullopt;
    }
    const auto error = object.value(QStringLiteral("lastError"));
    if (error.isString()) {
        source.last_error = error.toString();
    } else if (!error.isUndefined() && !error.isNull()) {
        return std::nullopt;
    }
    return source;
}

QJsonObject item_to_json(const LibraryItem& item) {
    QJsonObject object{{QStringLiteral("path"), item.path},
                       {QStringLiteral("sourceID"), uuid_text(item.source_id)},
                       {QStringLiteral("title"), item.title},
                       {QStringLiteral("parentFolder"), item.parent_folder},
                       {QStringLiteral("fileSize"), item.file_size},
                       {QStringLiteral("lastSeenAt"), reference_seconds(item.last_seen_at)}};
    if (item.modified_at) {
        object.insert(QStringLiteral("modifiedAt"), reference_seconds(*item.modified_at));
    }
    return object;
}

std::optional<LibraryItem> item_from_json(const QJsonObject& object) {
    const auto path = object.value(QStringLiteral("path"));
    const auto source_id = parse_uuid(object.value(QStringLiteral("sourceID")));
    const auto title = object.value(QStringLiteral("title"));
    const auto parent = object.value(QStringLiteral("parentFolder"));
    const auto size = object.value(QStringLiteral("fileSize"));
    const auto seen = object.value(QStringLiteral("lastSeenAt"));
    if (!path.isString() || !source_id || !title.isString() || !parent.isString() ||
        !size.isDouble() || !seen.isDouble()) {
        return std::nullopt;
    }
    LibraryItem item;
    item.path = path.toString();
    item.source_id = *source_id;
    item.title = title.toString();
    item.parent_folder = parent.toString();
    item.file_size = size.toInteger();
    item.last_seen_at = date_from_reference(seen.toDouble());
    const auto modified = object.value(QStringLiteral("modifiedAt"));
    if (modified.isDouble()) {
        item.modified_at = date_from_reference(modified.toDouble());
    } else if (!modified.isUndefined() && !modified.isNull()) {
        return std::nullopt;
    }
    return item;
}

QJsonObject metadata_to_json(const ItemMetadata& metadata) {
    QJsonObject object;
    if (metadata.display_title) {
        object.insert(QStringLiteral("displayTitle"), *metadata.display_title);
    }
    object.insert(QStringLiteral("isFavorite"), metadata.is_favorite);
    object.insert(QStringLiteral("tags"), QJsonArray::fromStringList(metadata.tags));
    QJsonArray ids;
    for (const auto& id : metadata.collection_ids) {
        ids.append(uuid_text(id));
    }
    object.insert(QStringLiteral("collectionIDs"), ids);
    if (metadata.bound_subtitle_path) {
        object.insert(QStringLiteral("boundSubtitlePath"), *metadata.bound_subtitle_path);
    }
    return object;
}

std::optional<ItemMetadata> metadata_from_json(const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const auto object = value.toObject();
    ItemMetadata metadata;
    const auto title = object.value(QStringLiteral("displayTitle"));
    if (title.isString()) {
        metadata.display_title = title.toString();
    } else if (!title.isUndefined() && !title.isNull()) {
        return std::nullopt;
    }
    const auto favorite = object.value(QStringLiteral("isFavorite"));
    if (favorite.isBool()) {
        metadata.is_favorite = favorite.toBool();
    } else if (!favorite.isUndefined() && !favorite.isNull()) {
        return std::nullopt;
    }
    const auto tags = object.value(QStringLiteral("tags"));
    if (tags.isArray()) {
        for (const auto& tag : tags.toArray()) {
            if (!tag.isString()) {
                return std::nullopt;
            }
            metadata.tags.append(tag.toString());
        }
    } else if (!tags.isUndefined() && !tags.isNull()) {
        return std::nullopt;
    }
    const auto ids = object.value(QStringLiteral("collectionIDs"));
    if (ids.isArray()) {
        for (const auto& id : ids.toArray()) {
            const auto parsed = parse_uuid(id);
            if (!parsed) {
                return std::nullopt;
            }
            metadata.collection_ids.append(*parsed);
        }
    } else if (!ids.isUndefined() && !ids.isNull()) {
        return std::nullopt;
    }
    const auto subtitle = object.value(QStringLiteral("boundSubtitlePath"));
    if (subtitle.isString()) {
        metadata.bound_subtitle_path = subtitle.toString();
    } else if (!subtitle.isUndefined() && !subtitle.isNull()) {
        return std::nullopt;
    }
    return metadata;
}

QString rule_field_text(SmartRuleField field) {
    switch (field) {
        case SmartRuleField::file_name:
            return QStringLiteral("fileName");
        case SmartRuleField::parent_folder:
            return QStringLiteral("parentFolder");
        case SmartRuleField::path:
            return QStringLiteral("path");
        case SmartRuleField::tag:
            return QStringLiteral("tag");
        case SmartRuleField::has_bound_subtitle:
            return QStringLiteral("hasBoundSubtitle");
        case SmartRuleField::playback_state:
            return QStringLiteral("playbackState");
    }
    return QStringLiteral("fileName");
}

std::optional<SmartRuleField> rule_field_from_text(const QString& text) {
    if (text == QStringLiteral("fileName")) {
        return SmartRuleField::file_name;
    }
    if (text == QStringLiteral("parentFolder")) {
        return SmartRuleField::parent_folder;
    }
    if (text == QStringLiteral("path")) {
        return SmartRuleField::path;
    }
    if (text == QStringLiteral("tag")) {
        return SmartRuleField::tag;
    }
    if (text == QStringLiteral("hasBoundSubtitle")) {
        return SmartRuleField::has_bound_subtitle;
    }
    if (text == QStringLiteral("playbackState")) {
        return SmartRuleField::playback_state;
    }
    return std::nullopt;
}

QString rule_match_text(SmartRuleMatch match) {
    switch (match) {
        case SmartRuleMatch::contains:
            return QStringLiteral("contains");
        case SmartRuleMatch::equals:
            return QStringLiteral("equals");
        case SmartRuleMatch::is_true:
            return QStringLiteral("isTrue");
    }
    return QStringLiteral("contains");
}

std::optional<SmartRuleMatch> rule_match_from_text(const QString& text) {
    if (text == QStringLiteral("contains")) {
        return SmartRuleMatch::contains;
    }
    if (text == QStringLiteral("equals")) {
        return SmartRuleMatch::equals;
    }
    if (text == QStringLiteral("isTrue")) {
        return SmartRuleMatch::is_true;
    }
    return std::nullopt;
}

QJsonObject rule_to_json(const SmartRule& rule) {
    return {{QStringLiteral("id"), uuid_text(rule.id)},
            {QStringLiteral("field"), rule_field_text(rule.field)},
            {QStringLiteral("match"), rule_match_text(rule.match)},
            {QStringLiteral("value"), rule.value}};
}

std::optional<SmartRule> rule_from_json(const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const auto object = value.toObject();
    const auto id = parse_uuid(object.value(QStringLiteral("id")));
    const auto field = object.value(QStringLiteral("field"));
    const auto match = object.value(QStringLiteral("match"));
    const auto rule_value = object.value(QStringLiteral("value"));
    if (!id || !field.isString() || !match.isString() || !rule_value.isString()) {
        return std::nullopt;
    }
    const auto parsed_field = rule_field_from_text(field.toString());
    const auto parsed_match = rule_match_from_text(match.toString());
    if (!parsed_field || !parsed_match) {
        return std::nullopt;
    }
    SmartRule rule;
    rule.id = *id;
    rule.field = *parsed_field;
    rule.match = *parsed_match;
    rule.value = rule_value.toString();
    return rule;
}

QJsonObject collection_to_json(const Collection& collection) {
    QJsonArray paths;
    for (const auto& path : collection.item_paths) {
        paths.append(path);
    }
    QJsonArray rules;
    for (const auto& rule : collection.smart_rules) {
        rules.append(rule_to_json(rule));
    }
    return {{QStringLiteral("id"), uuid_text(collection.id)},
            {QStringLiteral("name"), collection.name},
            {QStringLiteral("kind"),
             collection.kind == CollectionKind::smart ? QStringLiteral("smart")
                                                      : QStringLiteral("manual")},
            {QStringLiteral("itemPaths"), paths},
            {QStringLiteral("smartRules"), rules}};
}

std::optional<Collection> collection_from_json(const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const auto object = value.toObject();
    const auto id = parse_uuid(object.value(QStringLiteral("id")));
    const auto name = object.value(QStringLiteral("name"));
    if (!id || !name.isString()) {
        return std::nullopt;
    }
    Collection collection;
    collection.id = *id;
    collection.name = name.toString();
    const auto kind = object.value(QStringLiteral("kind"));
    if (kind.isString()) {
        const auto text = kind.toString();
        if (text == QStringLiteral("smart")) {
            collection.kind = CollectionKind::smart;
        } else if (text == QStringLiteral("manual")) {
            collection.kind = CollectionKind::manual;
        } else {
            return std::nullopt;
        }
    } else if (!kind.isUndefined() && !kind.isNull()) {
        return std::nullopt;
    }
    const auto paths = object.value(QStringLiteral("itemPaths"));
    if (paths.isArray()) {
        for (const auto& path : paths.toArray()) {
            if (!path.isString()) {
                return std::nullopt;
            }
            collection.item_paths.append(path.toString());
        }
    } else if (!paths.isUndefined() && !paths.isNull()) {
        return std::nullopt;
    }
    const auto rules = object.value(QStringLiteral("smartRules"));
    if (rules.isArray()) {
        for (const auto& rule : rules.toArray()) {
            const auto parsed = rule_from_json(rule);
            if (!parsed) {
                return std::nullopt;
            }
            collection.smart_rules.append(*parsed);
        }
    } else if (!rules.isUndefined() && !rules.isNull()) {
        return std::nullopt;
    }
    return collection;
}

}  // namespace

QJsonObject catalog_to_json(const LibraryCatalog& catalog) {
    QJsonArray sources;
    for (const auto& source : catalog.sources) {
        sources.append(source_to_json(source));
    }
    QJsonArray items;
    for (const auto& item : catalog.items) {
        items.append(item_to_json(item));
    }
    QJsonObject metadata;
    for (auto entry = catalog.item_metadata_by_path.begin();
         entry != catalog.item_metadata_by_path.end(); ++entry) {
        metadata.insert(entry.key(), metadata_to_json(entry.value()));
    }
    QJsonArray collections;
    for (const auto& collection : catalog.collections) {
        collections.append(collection_to_json(collection));
    }
    return {{QStringLiteral("sources"), sources},
            {QStringLiteral("items"), items},
            {QStringLiteral("remoteItems"), QJsonArray()},
            {QStringLiteral("itemMetadataByPath"), metadata},
            {QStringLiteral("collections"), collections}};
}

std::optional<LibraryCatalog> catalog_from_json(const QJsonObject& object) {
    LibraryCatalog catalog;
    const auto sources = object.value(QStringLiteral("sources"));
    const auto items = object.value(QStringLiteral("items"));
    if (!sources.isArray() || !items.isArray()) {
        return std::nullopt;
    }
    for (const auto& value : sources.toArray()) {
        const auto source = value.isObject() ? source_from_json(value.toObject()) : std::nullopt;
        if (!source) {
            return std::nullopt;
        }
        catalog.sources.append(*source);
    }
    for (const auto& value : items.toArray()) {
        const auto item = value.isObject() ? item_from_json(value.toObject()) : std::nullopt;
        if (!item) {
            return std::nullopt;
        }
        catalog.items.append(*item);
    }
    const auto remote = object.value(QStringLiteral("remoteItems"));
    if (!remote.isUndefined() && !remote.isNull() && !remote.isArray()) {
        return std::nullopt;
    }
    const auto metadata = object.value(QStringLiteral("itemMetadataByPath"));
    if (metadata.isObject()) {
        const auto entries = metadata.toObject();
        for (auto entry = entries.begin(); entry != entries.end(); ++entry) {
            const auto parsed = metadata_from_json(entry.value());
            if (!parsed) {
                return std::nullopt;
            }
            catalog.item_metadata_by_path.insert(entry.key(), *parsed);
        }
    } else if (!metadata.isUndefined() && !metadata.isNull()) {
        return std::nullopt;
    }
    const auto collections = object.value(QStringLiteral("collections"));
    if (collections.isArray()) {
        for (const auto& value : collections.toArray()) {
            const auto parsed = collection_from_json(value);
            if (!parsed) {
                return std::nullopt;
            }
            catalog.collections.append(*parsed);
        }
    } else if (!collections.isUndefined() && !collections.isNull()) {
        return std::nullopt;
    }
    return catalog;
}

}  // namespace niratan::video::detail

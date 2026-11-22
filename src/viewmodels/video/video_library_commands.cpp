// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video library's commands: scanning, playback actions, metadata and
// collection writes, opening and selection cleanup. The state and property
// side lives in video_library_view_model.cpp.

#include "viewmodels/video/video_library_view_model.h"

#include <QFileInfo>

#include <algorithm>
#include <optional>

namespace niratan::viewmodels {
namespace {

QString uuid_text(const QUuid& id) {
    return id.toString(QUuid::WithoutBraces).toUpper();
}

QString collection_kind_text(video::CollectionKind kind) {
    return kind == video::CollectionKind::smart ? QStringLiteral("smart")
                                                : QStringLiteral("manual");
}

QString rule_field_text(video::SmartRuleField field) {
    switch (field) {
        case video::SmartRuleField::file_name:
            return QStringLiteral("fileName");
        case video::SmartRuleField::parent_folder:
            return QStringLiteral("parentFolder");
        case video::SmartRuleField::path:
            return QStringLiteral("path");
        case video::SmartRuleField::tag:
            return QStringLiteral("tag");
        case video::SmartRuleField::has_bound_subtitle:
            return QStringLiteral("hasBoundSubtitle");
        case video::SmartRuleField::playback_state:
            return QStringLiteral("playbackState");
    }
    return QStringLiteral("fileName");
}

QString rule_match_text(video::SmartRuleMatch match) {
    switch (match) {
        case video::SmartRuleMatch::contains:
            return QStringLiteral("contains");
        case video::SmartRuleMatch::equals:
            return QStringLiteral("equals");
        case video::SmartRuleMatch::is_true:
            return QStringLiteral("isTrue");
    }
    return QStringLiteral("contains");
}

video::SmartRule rule_from_variant(const QVariantMap& map) {
    video::SmartRule rule;
    const QString id = map.value(QStringLiteral("id")).toString();
    rule.id = id.isEmpty() ? QUuid::createUuid() : QUuid::fromString(id);
    const QString field = map.value(QStringLiteral("field")).toString();
    if (field == QStringLiteral("parentFolder")) {
        rule.field = video::SmartRuleField::parent_folder;
    } else if (field == QStringLiteral("path")) {
        rule.field = video::SmartRuleField::path;
    } else if (field == QStringLiteral("tag")) {
        rule.field = video::SmartRuleField::tag;
    } else if (field == QStringLiteral("hasBoundSubtitle")) {
        rule.field = video::SmartRuleField::has_bound_subtitle;
    } else if (field == QStringLiteral("playbackState")) {
        rule.field = video::SmartRuleField::playback_state;
    } else {
        rule.field = video::SmartRuleField::file_name;
    }
    const QString match = map.value(QStringLiteral("match")).toString();
    if (match == QStringLiteral("equals")) {
        rule.match = video::SmartRuleMatch::equals;
    } else if (match == QStringLiteral("isTrue")) {
        rule.match = video::SmartRuleMatch::is_true;
    } else {
        rule.match = video::SmartRuleMatch::contains;
    }
    rule.value = map.value(QStringLiteral("value")).toString();
    return rule;
}

QList<video::SmartRule> rules_from_variant(const QVariantList& rules) {
    QList<video::SmartRule> result;
    for (const auto& rule : rules) {
        result.append(rule_from_variant(rule.toMap()));
    }
    return result;
}

QVariantMap collection_map(const video::Collection& collection) {
    QVariantMap map;
    map.insert(QStringLiteral("id"), uuid_text(collection.id));
    map.insert(QStringLiteral("name"), collection.name);
    map.insert(QStringLiteral("kind"), collection_kind_text(collection.kind));
    map.insert(QStringLiteral("itemPaths"), collection.item_paths);
    QVariantList rules;
    for (const auto& rule : collection.smart_rules) {
        rules.append(QVariantMap{{QStringLiteral("id"), uuid_text(rule.id)},
                                 {QStringLiteral("field"), rule_field_text(rule.field)},
                                 {QStringLiteral("match"), rule_match_text(rule.match)},
                                 {QStringLiteral("value"), rule.value}});
    }
    map.insert(QStringLiteral("smartRules"), rules);
    return map;
}

// Drops selection ids that no longer exist after a catalog change.
bool prune_selection(const video::LibraryCatalog& catalog, QString& selected_id,
                     QStringList& selected_ids) {
    QSet<QString> existing;
    for (const auto& item : catalog.items) {
        existing.insert(item.path);
    }
    const QStringList previous_ids = selected_ids;
    selected_ids.removeIf([&existing](const QString& id) { return !existing.contains(id); });
    if (!selected_id.isEmpty() && !existing.contains(selected_id)) {
        selected_id.clear();
    }
    return previous_ids != selected_ids;
}

}  // namespace

void VideoLibraryViewModel::addFolders(const QStringList& folders) {
    if (folders.isEmpty()) {
        return;
    }
    scanning_ = true;
    emit scanningChanged();
    QString failure;
    for (const auto& folder : folders) {
        const auto source = store_.add_source(QFileInfo(folder).filesystemAbsoluteFilePath());
        QString error;
        if (!store_.scan_source(source.id, &error)) {
            failure = error;
            break;
        }
    }
    scanning_ = false;
    emit scanningChanged();
    refreshPlaybackHistory();
    notifyRows();
    if (!failure.isEmpty()) {
        showError(failure);
    }
}

void VideoLibraryViewModel::refreshAllSources() {
    if (store_.catalog().sources.isEmpty()) {
        return;
    }
    scanning_ = true;
    emit scanningChanged();
    QString error;
    const bool ok = store_.scan_all_sources(&error);
    scanning_ = false;
    emit scanningChanged();
    refreshPlaybackHistory();
    notifyRows();
    if (!ok) {
        showError(error);
    }
}

void VideoLibraryViewModel::refreshSource(const QString& id) {
    scanning_ = true;
    emit scanningChanged();
    QString error;
    const bool ok = store_.scan_source(QUuid::fromString(id), &error);
    scanning_ = false;
    emit scanningChanged();
    refreshPlaybackHistory();
    notifyRows();
    if (!ok) {
        showError(error);
    }
}

void VideoLibraryViewModel::removeSource(const QString& id) {
    store_.remove_source(QUuid::fromString(id));
    if (prune_selection(store_.catalog(), selected_item_id_, selected_item_ids_)) {
        emit selectionChanged();
    }
    refreshPlaybackHistory();
    notifyRows();
}

void VideoLibraryViewModel::select(const QString& id) {
    selected_item_id_ = id;
    selected_item_ids_ = {id};
    emit selectionChanged();
}

void VideoLibraryViewModel::clearSelection() {
    if (selected_item_id_.isEmpty() && selected_item_ids_.isEmpty()) {
        return;
    }
    selected_item_id_.clear();
    selected_item_ids_.clear();
    emit selectionChanged();
}

void VideoLibraryViewModel::setSelectedItemIDs(const QStringList& ids) {
    if (selected_item_ids_ == ids) {
        return;
    }
    selected_item_ids_ = ids;
    emit selectionChanged();
}

QString VideoLibraryViewModel::openUrl(const QString& id) {
    const auto& items = store_.catalog().items;
    const auto entry =
        std::find_if(items.begin(), items.end(),
                     [&id](const video::LibraryItem& item) { return item.path == id; });
    if (entry == items.end()) {
        return {};
    }
    const QString path = entry->path;
    const QUuid source_id = entry->source_id;
    if (!QFileInfo::exists(path)) {
        QString ignored;
        store_.scan_source(source_id, &ignored);
        notifyRows();
        showError(tr("The saved video file is no longer available."));
        return {};
    }
    return path;
}

QString VideoLibraryViewModel::openFromBeginningUrl(const QString& id) {
    const QString url = openUrl(id);
    if (url.isEmpty()) {
        return {};
    }
    history_.clear(url);
    refreshPlaybackHistory();
    return url;
}

QString VideoLibraryViewModel::subtitleUrlForOpening(const QString& id) {
    const auto metadata = store_.metadata_for_path(id);
    const auto& subtitle = metadata.bound_subtitle_path;
    if (!subtitle || subtitle->isEmpty() || !QFileInfo::exists(*subtitle)) {
        return {};
    }
    return *subtitle;
}

void VideoLibraryViewModel::markWatched(const QString& id) {
    const auto state = history_.state(id);
    history_.mark_watched(id, state ? state->duration : std::nullopt);
    refreshPlaybackHistory();
}

void VideoLibraryViewModel::clearProgress(const QString& id) {
    history_.clear(id);
    refreshPlaybackHistory();
}

void VideoLibraryViewModel::setDisplayTitle(const QString& id, const QString& title) {
    store_.set_display_title(id, std::optional<QString>(title));
    notifyRows();
}

void VideoLibraryViewModel::setFavorite(const QString& id, bool favorite) {
    store_.set_favorite(id, favorite);
    notifyRows();
}

void VideoLibraryViewModel::setTags(const QString& id, const QStringList& tags) {
    store_.set_tags(id, tags);
    notifyRows();
}

void VideoLibraryViewModel::bindSubtitle(const QString& id, const QString& path) {
    store_.bind_subtitle(id, path.isEmpty() ? std::nullopt : std::optional<QString>(path));
    notifyRows();
}

QVariantMap VideoLibraryViewModel::createCollection(const QString& name, const QStringList& ids) {
    const auto collection = store_.create_collection(name, ids);
    notifyRows();
    return collection_map(collection);
}

QVariantMap VideoLibraryViewModel::createSmartCollection(const QString& name,
                                                         const QVariantList& rules) {
    const auto collection = store_.create_smart_collection(name, rules_from_variant(rules));
    notifyRows();
    return collection_map(collection);
}

void VideoLibraryViewModel::updateSmartCollection(const QString& id, const QString& name,
                                                  const QVariantList& rules) {
    store_.update_smart_collection(QUuid::fromString(id), name, rules_from_variant(rules));
    notifyRows();
}

void VideoLibraryViewModel::setCollectionMembership(const QString& id, const QString& collection_id,
                                                    bool included) {
    const QUuid collection_uuid = QUuid::fromString(collection_id);
    const auto entry =
        std::find_if(store_.catalog().collections.begin(), store_.catalog().collections.end(),
                     [&collection_uuid](const video::Collection& collection) {
                         return collection.id == collection_uuid;
                     });
    if (entry == store_.catalog().collections.end() ||
        entry->kind != video::CollectionKind::manual) {
        return;
    }
    const QString name = entry->name;
    QStringList paths = entry->item_paths;
    if (included) {
        if (!paths.contains(id)) {
            paths.append(id);
        }
    } else {
        paths.removeAll(id);
    }
    store_.update_collection(collection_uuid, name, paths);
    notifyRows();
}

void VideoLibraryViewModel::removeCollection(const QString& collection_id) {
    store_.remove_collection(QUuid::fromString(collection_id));
    notifyRows();
}

void VideoLibraryViewModel::markSelectedWatched() {
    const QSet<QString> ids(selected_item_ids_.begin(), selected_item_ids_.end());
    for (const auto& item : store_.catalog().items) {
        if (ids.contains(item.path)) {
            markWatched(item.path);
        }
    }
}

void VideoLibraryViewModel::clearSelectedProgress() {
    const QSet<QString> ids(selected_item_ids_.begin(), selected_item_ids_.end());
    for (const auto& item : store_.catalog().items) {
        if (ids.contains(item.path)) {
            clearProgress(item.path);
        }
    }
}

int VideoLibraryViewModel::removeMissingItems(const QString& source_id) {
    std::optional<QUuid> source;
    if (!source_id.isEmpty()) {
        source = QUuid::fromString(source_id);
    }
    const int removed = store_.remove_missing_items(source);
    if (prune_selection(store_.catalog(), selected_item_id_, selected_item_ids_)) {
        emit selectionChanged();
    }
    refreshPlaybackHistory();
    notifyRows();
    return removed;
}

}  // namespace niratan::viewmodels

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Row projection for the video library: source-relative organization, the
// row map QML binds to, the sort comparators, search matching and smart-rule
// evaluation, following VideoLibraryViewModel.swift `rows(for:)`,
// `organization`, `looksLikeSeason`, `sortRows`, `recentPlaybackSort`,
// `titleSort`, `seriesSort`, `VideoLibraryRow.matches` and
// `smartRules`/`smartRule`/`matchesText`/`matchesBool`/`playbackStateTokens`.

#include "viewmodels/video/video_library_rows.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <functional>

namespace niratan::viewmodels::detail {
namespace {

// Date.distantPast, the value Swift substitutes for a missing date.
constexpr qint64 kDistantPastSeconds = -62'135'596'800;

QString folded(const QString& text) {
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString result;
    result.reserve(decomposed.size());
    for (const QChar character : decomposed) {
        if (character.category() != QChar::Mark_NonSpacing) {
            result.append(character);
        }
    }
    return result.toCaseFolded();
}

QDateTime distant_past() {
    return QDateTime::fromSecsSinceEpoch(kDistantPastSeconds, QTimeZone::UTC);
}

bool looks_like_season(const QString& component) {
    static const QRegularExpression pattern(
        QStringLiteral("\\b(season|saison|temporada|s)\\s*\\d+\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern.match(component).hasMatch();
}

Organization organization_for(const video::LibraryItem& item, const video::LibrarySource* source) {
    Organization organization;
    if (source == nullptr) {
        organization.series_name = item.parent_folder;
        organization.folder_path = item.parent_folder;
        return organization;
    }
    const QString root = source->path;
    const QString parent = QFileInfo(item.path).absolutePath();
    QStringList relative;
    if (parent != root && parent.startsWith(root + u'/')) {
        relative = parent.mid(root.size() + 1).split(u'/');
    }
    organization.folder_path =
        relative.isEmpty() ? source->name : relative.join(QStringLiteral(" / "));
    if (relative.isEmpty()) {
        organization.series_name = source->name;
        return organization;
    }
    if (relative.size() >= 2) {
        organization.series_name = relative[0];
        organization.season_name = relative[1];
        return organization;
    }
    if (looks_like_season(relative[0])) {
        organization.series_name = source->name;
        organization.season_name = relative[0];
        return organization;
    }
    organization.series_name = relative[0];
    return organization;
}

int title_compare(const Row& left, const Row& right) {
    const int title =
        video::localized_standard_compare(left.display_title(), right.display_title());
    if (title != 0) {
        return title;
    }
    return video::localized_standard_compare(left.item.path, right.item.path);
}

bool title_before(const Row& left, const Row& right) {
    return title_compare(left, right) < 0;
}

bool recent_before(const Row& left, const Row& right) {
    const QDateTime left_date = left.playback ? left.playback->updated_at : distant_past();
    const QDateTime right_date = right.playback ? right.playback->updated_at : distant_past();
    if (left_date != right_date) {
        return left_date > right_date;
    }
    return title_before(left, right);
}

bool modified_before(const Row& left, const Row& right) {
    const QDateTime left_date = left.item.modified_at.value_or(distant_past());
    const QDateTime right_date = right.item.modified_at.value_or(distant_past());
    if (left_date != right_date) {
        return left_date > right_date;
    }
    return title_before(left, right);
}

bool folder_before(const Row& left, const Row& right) {
    const int folder =
        video::localized_standard_compare(left.item.parent_folder, right.item.parent_folder);
    if (folder != 0) {
        return folder < 0;
    }
    return title_before(left, right);
}

bool series_before(const Row& left, const Row& right) {
    const QString left_season = left.organization.season_name.value_or(QString());
    const QString right_season = right.organization.season_name.value_or(QString());
    const int season = video::localized_standard_compare(left_season, right_season);
    if (season != 0) {
        return season < 0;
    }
    return title_before(left, right);
}

QVariantMap playback_map(const player::PlaybackState& state) {
    QVariantMap map;
    map.insert(QStringLiteral("position"), state.position);
    if (state.duration) {
        map.insert(QStringLiteral("duration"), *state.duration);
    }
    map.insert(QStringLiteral("updatedAt"), iso_date(state.updated_at));
    map.insert(QStringLiteral("finished"), state.finished);
    if (const auto progress = state.progress()) {
        map.insert(QStringLiteral("progress"), *progress);
    }
    if (!state.finished && state.duration && *state.duration > state.position) {
        map.insert(QStringLiteral("remaining"), *state.duration - state.position);
    }
    map.insert(QStringLiteral("resumable"), state.resumable());
    return map;
}

QVariantMap metadata_map(const video::ItemMetadata& metadata) {
    QVariantMap map;
    if (metadata.display_title) {
        map.insert(QStringLiteral("displayTitle"), *metadata.display_title);
    }
    map.insert(QStringLiteral("isFavorite"), metadata.is_favorite);
    map.insert(QStringLiteral("tags"), metadata.tags);
    QStringList ids;
    for (const auto& id : metadata.collection_ids) {
        ids.append(uuid_text(id));
    }
    map.insert(QStringLiteral("collectionIDs"), ids);
    if (metadata.bound_subtitle_path) {
        map.insert(QStringLiteral("boundSubtitlePath"), *metadata.bound_subtitle_path);
    }
    return map;
}

QVariantMap organization_map(const Organization& organization) {
    QVariantMap map;
    map.insert(QStringLiteral("seriesName"), organization.series_name);
    if (organization.season_name) {
        map.insert(QStringLiteral("seasonName"), *organization.season_name);
    }
    map.insert(QStringLiteral("folderPath"), organization.folder_path);
    return map;
}

bool matches_text(const video::SmartRule& rule, const QStringList& values) {
    const QString needle = rule.value.trimmed();
    if (needle.isEmpty()) {
        return false;
    }
    if (rule.match == video::SmartRuleMatch::is_true) {
        return false;
    }
    const QString folded_needle = folded(needle);
    for (const auto& value : values) {
        if (rule.match == video::SmartRuleMatch::equals) {
            if (folded(value) == folded_needle) {
                return true;
            }
        } else if (folded(value).contains(folded_needle)) {
            return true;
        }
    }
    return false;
}

bool matches_bool(const video::SmartRule& rule, bool value) {
    if (rule.match == video::SmartRuleMatch::is_true) {
        return value;
    }
    if (rule.match == video::SmartRuleMatch::equals) {
        const QString expected = rule.value.trimmed().toLower();
        return value ==
            (expected == QStringLiteral("true") || expected == QStringLiteral("yes") ||
             expected == QStringLiteral("1"));
    }
    return false;
}

QStringList playback_tokens(const Row& row) {
    if (!row.playback) {
        return {QStringLiteral("unwatched")};
    }
    if (row.playback->finished) {
        return {QStringLiteral("finished"), QStringLiteral("watched"), QStringLiteral("played")};
    }
    if (row.playback->resumable()) {
        return {QStringLiteral("inProgress"), QStringLiteral("in progress"),
                QStringLiteral("resumable"), QStringLiteral("started")};
    }
    return {QStringLiteral("played")};
}

bool rule_matches(const video::SmartRule& rule, const Row& row) {
    switch (rule.field) {
        case video::SmartRuleField::file_name:
            return matches_text(rule, {row.display_title(), row.item.title});
        case video::SmartRuleField::parent_folder:
            return matches_text(rule, {row.item.parent_folder, row.organization.folder_path});
        case video::SmartRuleField::path:
            return matches_text(rule, {row.item.path});
        case video::SmartRuleField::tag:
            return matches_text(rule, row.metadata.tags);
        case video::SmartRuleField::has_bound_subtitle:
            return matches_bool(rule, row.metadata.bound_subtitle_path.has_value());
        case video::SmartRuleField::playback_state:
            return matches_text(rule, playback_tokens(row));
    }
    return false;
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

}  // namespace

QList<Row> build_rows(const video::LibraryCatalog& catalog,
                      const QHash<QString, player::PlaybackState>& states) {
    QHash<QUuid, video::LibrarySource> sources;
    for (const auto& source : catalog.sources) {
        sources.insert(source.id, source);
    }
    QList<Row> rows;
    rows.reserve(catalog.items.size());
    for (const auto& item : catalog.items) {
        Row row;
        row.item = item;
        const auto source = sources.constFind(item.source_id);
        const video::LibrarySource* source_ptr =
            source == sources.constEnd() ? nullptr : &source.value();
        row.source_name = source_ptr != nullptr ? source_ptr->name : item.parent_folder;
        const auto state = states.constFind(item.path);
        if (state != states.constEnd()) {
            row.playback = state.value();
        }
        row.metadata = catalog.item_metadata_by_path.value(item.path);
        row.organization = organization_for(item, source_ptr);
        row.subtitle_candidate_path = row.metadata.bound_subtitle_path;
        rows.append(row);
    }
    return rows;
}

bool missing(const Row& row) {
    return !QFileInfo::exists(row.item.path);
}

Compare comparator_for(const QString& option) {
    if (option == QStringLiteral("recentPlayback")) {
        return recent_before;
    }
    if (option == QStringLiteral("modifiedDate")) {
        return modified_before;
    }
    if (option == QStringLiteral("folder")) {
        return folder_before;
    }
    return title_before;
}

Compare series_comparator() {
    return series_before;
}

QList<Row> sorted_rows(QList<Row> rows, Compare compare) {
    std::stable_sort(rows.begin(), rows.end(), compare);
    return rows;
}

QString iso_date(const QDateTime& date) {
    return date.toUTC().toString(Qt::ISODate);
}

QString uuid_text(const QUuid& id) {
    return id.toString(QUuid::WithoutBraces).toUpper();
}

QVariantMap row_map(const Row& row) {
    QVariantMap map;
    map.insert(QStringLiteral("id"), row.item.path);
    map.insert(QStringLiteral("path"), row.item.path);
    map.insert(QStringLiteral("title"), row.item.title);
    map.insert(QStringLiteral("displayTitle"), row.display_title());
    map.insert(QStringLiteral("sourceName"), row.source_name);
    map.insert(QStringLiteral("parentFolder"), row.item.parent_folder);
    map.insert(QStringLiteral("fileSize"), row.item.file_size);
    map.insert(QStringLiteral("modifiedAt"),
               row.item.modified_at ? QVariant(iso_date(*row.item.modified_at)) : QVariant());
    map.insert(QStringLiteral("playback"),
               row.playback ? QVariant(playback_map(*row.playback)) : QVariant());
    map.insert(QStringLiteral("metadata"), metadata_map(row.metadata));
    map.insert(QStringLiteral("organization"), organization_map(row.organization));
    map.insert(QStringLiteral("subtitleCandidatePath"),
               row.subtitle_candidate_path ? QVariant(*row.subtitle_candidate_path) : QVariant());
    return map;
}

QVariantList row_list(const QList<Row>& rows) {
    QVariantList list;
    for (const auto& row : rows) {
        list.append(row_map(row));
    }
    return list;
}

QVariantMap make_section(const QString& id, const QString& title, const QList<Row>& rows,
                         const QString& collection_id) {
    return {{QStringLiteral("id"), id},
            {QStringLiteral("title"), title},
            {QStringLiteral("rows"), row_list(rows)},
            {QStringLiteral("collectionId"), collection_id}};
}

bool row_matches(const Row& row, const QString& query) {
    const QString folded_query = folded(query);
    const auto field_matches = [&folded_query](const QString& field) {
        return folded(field).contains(folded_query);
    };
    return field_matches(row.display_title()) || field_matches(row.item.title) ||
        field_matches(row.item.parent_folder) || field_matches(row.source_name) ||
        field_matches(row.organization.series_name) ||
        field_matches(row.organization.season_name.value_or(QString())) ||
        field_matches(row.organization.folder_path) ||
        field_matches(row.metadata.tags.join(u' ')) ||
        field_matches(row.metadata.bound_subtitle_path.value_or(QString())) ||
        field_matches(row.item.path);
}

QList<video::SmartRule> rules_from_variant(const QVariantList& rules) {
    QList<video::SmartRule> result;
    for (const auto& rule : rules) {
        result.append(rule_from_variant(rule.toMap()));
    }
    return result;
}

bool rules_match(const QList<video::SmartRule>& rules, const Row& row) {
    if (rules.isEmpty()) {
        return false;
    }
    for (const auto& rule : rules) {
        if (!rule_matches(rule, row)) {
            return false;
        }
    }
    return true;
}

bool covered_by_collection(const video::LibraryCatalog& catalog, const Row& row) {
    for (const auto& collection : catalog.collections) {
        if (collection.kind == video::CollectionKind::manual) {
            if (collection.item_paths.contains(row.item.path)) {
                return true;
            }
        } else if (rules_match(collection.smart_rules, row)) {
            return true;
        }
    }
    return false;
}

}  // namespace niratan::viewmodels::detail

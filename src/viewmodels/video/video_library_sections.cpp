// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sections, mode badge counts, source summaries and smart-collection preview
// for the video library, following VideoLibraryViewModel.swift `sections`,
// `queriedRows`, `groupedSections`, `modeCounts`, `sourceSummaries` and
// `smartCollectionPreviewRows`.

#include "viewmodels/video/video_library_view_model.h"

#include "viewmodels/video/video_library_rows.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace niratan::viewmodels {
namespace {

using detail::Compare;
using detail::Row;

template <typename Predicate> QList<Row> subset(const QList<Row>& rows, Predicate predicate) {
    QList<Row> result;
    for (const auto& row : rows) {
        if (predicate(row)) {
            result.append(row);
        }
    }
    return result;
}

QVariantList grouped_sections(const QList<Row>& rows, const QString& prefix,
                              const std::function<QString(const Row&)>& key, Compare compare) {
    QHash<QString, QList<Row>> groups;
    for (const auto& row : rows) {
        groups[key(row)].append(row);
    }
    QList<QPair<QString, QList<Row>>> entries;
    for (auto entry = groups.begin(); entry != groups.end(); ++entry) {
        entries.append({entry.key(), entry.value()});
    }
    std::stable_sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        return video::localized_standard_compare(left.first, right.first) < 0;
    });
    QVariantList sections;
    for (const auto& entry : entries) {
        sections.append(detail::make_section(prefix + u'-' + entry.first, entry.first,
                                             detail::sorted_rows(entry.second, compare)));
    }
    return sections;
}

}  // namespace

QVariantList VideoLibraryViewModel::sectionsData() const {
    const auto& catalog = store_.catalog();
    QList<Row> rows = detail::build_rows(catalog, playback_states_);
    const QString query = search_text_.trimmed();
    if (!query.isEmpty()) {
        rows = subset(rows, [&query](const Row& row) { return detail::row_matches(row, query); });
    }
    if (show_unfinished_only_) {
        rows =
            subset(rows, [](const Row& row) { return row.playback && row.playback->resumable(); });
    }
    rows = detail::sorted_rows(rows, detail::comparator_for(sort_option_));

    const auto single = [](const QString& id, const QString& title, const QList<Row>& content) {
        QVariantList sections;
        if (!content.isEmpty()) {
            sections.append(detail::make_section(id, title, content));
        }
        return sections;
    };

    if (display_mode_ == QStringLiteral("continueWatching")) {
        return single(
            QStringLiteral("continue"), tr("Continue Watching"),
            detail::sorted_rows(
                subset(rows,
                       [](const Row& row) { return row.playback && row.playback->resumable(); }),
                detail::comparator_for(QStringLiteral("recentPlayback"))));
    }
    if (display_mode_ == QStringLiteral("favorites")) {
        return single(QStringLiteral("favorites"), tr("Favorites"),
                      subset(rows, [](const Row& row) { return row.metadata.is_favorite; }));
    }
    if (display_mode_ == QStringLiteral("unwatched")) {
        return single(
            QStringLiteral("unwatched"), tr("Unwatched"),
            subset(rows, [](const Row& row) { return !row.playback && !detail::missing(row); }));
    }
    if (display_mode_ == QStringLiteral("finished")) {
        return single(
            QStringLiteral("finished"), tr("Finished"),
            detail::sorted_rows(
                subset(rows, [](const Row& row) { return row.playback && row.playback->finished; }),
                detail::comparator_for(QStringLiteral("recentPlayback"))));
    }
    if (display_mode_ == QStringLiteral("missing")) {
        return single(QStringLiteral("missing"), tr("Missing"),
                      subset(rows, [](const Row& row) { return detail::missing(row); }));
    }
    if (display_mode_ == QStringLiteral("recent")) {
        return single(QStringLiteral("recent"), tr("Recent"),
                      detail::sorted_rows(
                          subset(rows, [](const Row& row) { return row.playback.has_value(); }),
                          detail::comparator_for(QStringLiteral("recentPlayback"))));
    }
    if (display_mode_ == QStringLiteral("needsReview")) {
        return single(QStringLiteral("needs-review"), tr("Needs Review"),
                      subset(rows, [&catalog](const Row& row) {
                          return !detail::covered_by_collection(catalog, row);
                      }));
    }
    if (display_mode_ == QStringLiteral("series")) {
        return grouped_sections(
            rows, QStringLiteral("series"),
            [](const Row& row) { return row.organization.series_name; },
            detail::series_comparator());
    }
    if (display_mode_ == QStringLiteral("folders")) {
        return grouped_sections(
            rows, QStringLiteral("folder"),
            [](const Row& row) { return row.organization.folder_path; },
            detail::comparator_for(sort_option_));
    }
    if (display_mode_ == QStringLiteral("collections")) {
        QList<QPair<QString, QVariantMap>> sections;
        for (const auto& collection : catalog.collections) {
            QList<Row> content;
            if (collection.kind == video::CollectionKind::manual) {
                QHash<QString, Row> by_path;
                for (const auto& row : rows) {
                    by_path.insert(row.item.path, row);
                }
                for (const auto& path : collection.item_paths) {
                    if (by_path.contains(path)) {
                        content.append(by_path.value(path));
                    }
                }
            } else {
                content = subset(rows, [&collection](const Row& row) {
                    return detail::rules_match(collection.smart_rules, row);
                });
            }
            content = detail::sorted_rows(content, detail::comparator_for(sort_option_));
            const QString id = QStringLiteral("collection-") + detail::uuid_text(collection.id);
            sections.append({collection.name,
                             detail::make_section(id, collection.name, content,
                                                  detail::uuid_text(collection.id))});
        }
        std::stable_sort(sections.begin(), sections.end(), [](const auto& left, const auto& right) {
            return video::localized_standard_compare(left.first, right.first) < 0;
        });
        QVariantList result;
        for (const auto& section : sections) {
            result.append(section.second);
        }
        return result;
    }
    return single(QStringLiteral("all"), tr("All Videos"), rows);
}

QVariantMap VideoLibraryViewModel::modeCountsData() const {
    const QList<Row> rows = detail::build_rows(store_.catalog(), playback_states_);
    int all = 0;
    int continue_watching = 0;
    int unwatched = 0;
    int finished = 0;
    int recent = 0;
    int missing_count = 0;
    int favorites = 0;
    QSet<QString> series;
    QSet<QString> folders;
    for (const auto& row : rows) {
        const bool is_missing = detail::missing(row);
        ++all;
        if (row.playback && row.playback->resumable()) {
            ++continue_watching;
        }
        if (!row.playback && !is_missing) {
            ++unwatched;
        }
        if (row.playback && row.playback->finished) {
            ++finished;
        }
        if (row.playback) {
            ++recent;
        }
        if (is_missing) {
            ++missing_count;
        }
        if (row.metadata.is_favorite) {
            ++favorites;
        }
        series.insert(row.organization.series_name);
        folders.insert(row.organization.folder_path);
    }
    QVariantMap counts;
    const auto put = [&counts](const QString& key, int value) {
        if (value > 0) {
            counts.insert(key, value);
        }
    };
    put(QStringLiteral("all"), all);
    put(QStringLiteral("continueWatching"), continue_watching);
    put(QStringLiteral("unwatched"), unwatched);
    put(QStringLiteral("finished"), finished);
    put(QStringLiteral("recent"), recent);
    put(QStringLiteral("missing"), missing_count);
    put(QStringLiteral("favorites"), favorites);
    counts.insert(QStringLiteral("series"), int(series.size()));
    counts.insert(QStringLiteral("folders"), int(folders.size()));
    counts.insert(QStringLiteral("collections"), int(store_.catalog().collections.size()));
    return counts;
}

QVariantList VideoLibraryViewModel::sourceSummariesData() const {
    const QList<Row> rows = detail::build_rows(store_.catalog(), playback_states_);
    QHash<QUuid, QList<Row>> by_source;
    for (const auto& row : rows) {
        by_source[row.item.source_id].append(row);
    }
    QVariantList summaries;
    for (const auto& source : store_.catalog().sources) {
        const QList<Row> source_rows = by_source.value(source.id);
        int in_progress = 0;
        int missing_count = 0;
        for (const auto& row : source_rows) {
            if (row.playback && row.playback->resumable()) {
                ++in_progress;
            }
            if (detail::missing(row)) {
                ++missing_count;
            }
        }
        QVariantMap summary;
        summary.insert(QStringLiteral("id"), detail::uuid_text(source.id));
        summary.insert(QStringLiteral("name"), source.name);
        summary.insert(QStringLiteral("path"), source.path);
        summary.insert(QStringLiteral("itemCount"), int(source_rows.size()));
        summary.insert(QStringLiteral("inProgressCount"), in_progress);
        summary.insert(QStringLiteral("missingCount"), missing_count);
        summary.insert(QStringLiteral("lastScannedAt"),
                       source.last_scanned_at ? QVariant(detail::iso_date(*source.last_scanned_at))
                                              : QVariant());
        summary.insert(QStringLiteral("lastError"),
                       source.last_error ? QVariant(*source.last_error) : QVariant());
        summaries.append(summary);
    }
    return summaries;
}

QVariantList VideoLibraryViewModel::smartCollectionPreviewRows(const QVariantList& rules,
                                                               int limit) const {
    return previewRows(rules, limit);
}

QVariantList VideoLibraryViewModel::previewRows(const QVariantList& rules, int limit) const {
    const QList<video::SmartRule> parsed = detail::rules_from_variant(rules);
    QList<Row> rows = detail::sorted_rows(detail::build_rows(store_.catalog(), playback_states_),
                                          detail::comparator_for(sort_option_));
    rows = subset(rows, [&parsed](const Row& row) { return detail::rules_match(parsed, row); });
    const int maximum = std::max(limit, 0);
    if (rows.size() > maximum) {
        rows = rows.mid(0, maximum);
    }
    return detail::row_list(rows);
}

}  // namespace niratan::viewmodels

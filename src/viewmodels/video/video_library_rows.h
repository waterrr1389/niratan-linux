// SPDX-License-Identifier: GPL-3.0-or-later
//
// Row projection shared by the video library's row builder and its section
// builders. Module-private: only video_library_rows.cpp and
// video_library_sections.cpp include it.
//
// A Row carries the catalog item, the source name (parent folder when the
// source is unknown), the cached playback state, the metadata and the
// source-relative organization Swift's `organization(for:source:)` derives.

#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

#include "services/player/playback_history.h"
#include "services/video/library_catalog.h"

namespace niratan::viewmodels::detail {

struct Organization {
    QString series_name;
    std::optional<QString> season_name;
    QString folder_path;
};

struct Row {
    video::LibraryItem item;
    QString source_name;
    std::optional<player::PlaybackState> playback;
    video::ItemMetadata metadata;
    Organization organization;
    std::optional<QString> subtitle_candidate_path;

    QString display_title() const { return metadata.display_title.value_or(item.title); }
};

using Compare = bool (*)(const Row&, const Row&);

QList<Row> build_rows(const video::LibraryCatalog& catalog,
                      const QHash<QString, player::PlaybackState>& states);
bool missing(const Row& row);
Compare comparator_for(const QString& option);
Compare series_comparator();
QList<Row> sorted_rows(QList<Row> rows, Compare compare);
QString iso_date(const QDateTime& date);
QString uuid_text(const QUuid& id);
QVariantList row_list(const QList<Row>& rows);
QVariantMap row_map(const Row& row);
QVariantMap make_section(const QString& id, const QString& title, const QList<Row>& rows,
                         const QString& collection_id = {});
bool row_matches(const Row& row, const QString& query);
QList<video::SmartRule> rules_from_variant(const QVariantList& rules);
bool rules_match(const QList<video::SmartRule>& rules, const Row& row);
bool covered_by_collection(const video::LibraryCatalog& catalog, const Row& row);

}  // namespace niratan::viewmodels::detail

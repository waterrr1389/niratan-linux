// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shared state for PlayerStudyViewModel translation units. This header is
// private to the player study module.

#pragma once

#include "player_study_view_model.h"

#include "services/player/mining_history_store.h"
#include "services/player/subtitle_document.h"

#include <QList>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <optional>
#include <string>

namespace niratan::viewmodels {

struct Cue {
    double start;
    double end;
    QString text;
};

struct PlayerStudyViewModel::State {
    player::MiningHistoryStore history_store = player::MiningHistoryStore::at_data_root();
    QUrl video;
    QUrl subtitle_file;
    QString subtitle_format;
    QString transcript_error;
    QString notice;
    QString search;
    QString history_search;
    QList<Cue> cues;
    QList<double> end_prefix;
    QList<int> filtered;
    int first = 0;
    int last = 0;
    int current = -1;
    int focus = -1;
    int current_chapter = -1;
    double position = 0.0;
    double delay = 0.0;
    double chapter_progress = -1.0;
    Cue native{-1.0, -1.0, {}};
    bool ready = false;
    bool visible = true;
    bool can_mine = false;
    bool following = true;
    bool can_align_previous = false;
    bool can_align_next = false;
    QVariantList transcript;
    QVariantList history;
    QVariantList history_sections;
    QVariantList chapters;
    QVariantMap track;
    player::MiningHistoryStore::Snapshot snapshot;
    std::optional<player::SubtitleCueStore> alignment_store;
    std::optional<player::MiningHistoryDestination> pending_jump;

    auto first_after(double time) const {
        return std::upper_bound(cues.cbegin(), cues.cend(), time,
                                [](double time, const Cue& cue) { return time < cue.start; });
    }

    int active_row(double time) const {
        int index = int(first_after(time) - cues.cbegin()) - 1;
        while (index >= 0) {
            const Cue& cue = cues[index];
            if (cue.start <= time && time <= cue.end) {
                return index;
            }
            if (cue.end < time) {
                return -1;
            }
            --index;
        }
        return -1;
    }

    int nearest_row(double time) const {
        return current >= 0 || cues.isEmpty()
            ? current
            : int(std::min(first_after(time) - cues.cbegin(), cues.size() - 1));
    }

    auto find_item(const QString& id) const {
        const std::string key = id.toStdString();
        return std::ranges::find(snapshot.items, key, &player::MiningHistoryItem::id);
    }
};

}  // namespace niratan::viewmodels

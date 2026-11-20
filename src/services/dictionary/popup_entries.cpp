// SPDX-License-Identifier: GPL-3.0-or-later
//
// The model keeps the library's raw order: trace candidates stay in library
// order with their steps innermost-first, and frequency and pitch entries are
// not deduplicated. This builder applies the reversal, the split and the
// deduplication, so the JSON matches Features/Popup/PopupView.swift
// (buildLookupEntries) and popup.js reads the same shape either way.

#include "popup_entries.h"

#include <QJsonObject>
#include <QString>
#include <Qt>

#include <string>
#include <vector>

namespace niratan::dictionary {
namespace {

QString text(const std::string& value) {
    return QString::fromStdString(value);
}

QJsonObject step_json(const TraceStep& step) {
    QJsonObject object;
    object.insert(QStringLiteral("name"), text(step.name));
    object.insert(QStringLiteral("description"), text(step.description));
    return object;
}

}  // namespace

QJsonArray lookup_entries_json(const std::vector<LookupEntry>& entries) {
    QJsonArray result;

    for (const LookupEntry& entry : entries) {
        // Reversing each candidate turns the innermost-first steps into
        // outermost-first, the order the popup shows.
        QJsonArray traces;
        for (const std::vector<TraceStep>& candidate : entry.traces) {
            QJsonArray steps;
            for (auto step = candidate.rbegin(); step != candidate.rend(); ++step) {
                steps.append(step_json(*step));
            }
            traces.append(steps);
        }

        QJsonArray glossaries;
        for (const GlossaryEntry& glossary : entry.glossaries) {
            QJsonObject object;
            object.insert(QStringLiteral("dictionary"), text(glossary.dictionary));
            object.insert(QStringLiteral("content"), text(glossary.content));
            object.insert(QStringLiteral("definitionTags"), text(glossary.definition_tags));
            object.insert(QStringLiteral("termTags"), text(glossary.term_tags));
            glossaries.append(object);
        }

        QJsonArray frequencies;
        for (const FrequencyGroup& group : entry.frequencies) {
            QJsonArray values;
            for (const FrequencyValue& value : group.values) {
                QJsonObject object;
                object.insert(QStringLiteral("value"), value.value);
                object.insert(QStringLiteral("displayValue"), text(value.display_value));
                values.append(object);
            }
            QJsonObject object;
            object.insert(QStringLiteral("dictionary"), text(group.dictionary));
            object.insert(QStringLiteral("frequencies"), values);
            frequencies.append(object);
        }

        QJsonArray pitches;
        for (const PitchGroup& group : entry.pitches) {
            // Positions and transcriptions are deduplicated independently,
            // each keeping the first occurrence, as the Swift builder does.
            QJsonArray positions;
            for (const int position : group.positions) {
                if (!positions.contains(position)) {
                    positions.append(position);
                }
            }
            QJsonArray transcriptions;
            for (const std::string& transcription : group.transcriptions) {
                const QString value = text(transcription);
                if (!transcriptions.contains(value)) {
                    transcriptions.append(value);
                }
            }
            QJsonObject object;
            object.insert(QStringLiteral("dictionary"), text(group.dictionary));
            object.insert(QStringLiteral("pitchPositions"), positions);
            object.insert(QStringLiteral("transcriptions"), transcriptions);
            pitches.append(object);
        }

        // Splitting on single spaces with empty pieces dropped keeps the
        // library's separators from producing empty rules.
        QJsonArray rules;
        for (const QString& rule : text(entry.rules).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            rules.append(rule);
        }

        QJsonObject object;
        object.insert(QStringLiteral("expression"), text(entry.expression));
        object.insert(QStringLiteral("reading"), text(entry.reading));
        object.insert(QStringLiteral("matched"), text(entry.matched));
        object.insert(QStringLiteral("deinflectionTrace"),
                      traces.isEmpty() ? QJsonArray() : traces.first().toArray());
        object.insert(QStringLiteral("deinflectionTraces"), traces);
        object.insert(QStringLiteral("glossaries"), glossaries);
        object.insert(QStringLiteral("frequencies"), frequencies);
        object.insert(QStringLiteral("pitches"), pitches);
        object.insert(QStringLiteral("rules"), rules);
        result.append(object);
    }

    return result;
}

}  // namespace niratan::dictionary

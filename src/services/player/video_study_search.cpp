// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/player/video_study_search.h"

#include <QRegularExpression>

#include <algorithm>
#include <utility>

namespace niratan::player {
namespace {

// The module's shared comparison form: NFKD (which also folds widths), case
// folding and no combining marks. Both sides of a comparison use it.
QString search_key(QStringView text) {
    static const QRegularExpression marks(QStringLiteral(R"(\p{M}+)"));
    QString key =
        text.toString().normalized(QString::NormalizationForm_KD).toCaseFolded().remove(marks);
    // Qt folds case simply, which keeps the sharp s; Unicode full case folding
    // doubles it, as Swift's case-insensitive comparison does.
    key.replace(QStringLiteral("ß"), QStringLiteral("ss"));
    return key;
}

struct MappedCharacter {
    qsizetype normalized_start = 0;
    qsizetype normalized_length = 0;
    qsizetype original_start = 0;
    qsizetype original_length = 0;
};

}  // namespace

QString normalized_study_query(QStringView query) {
    return query.trimmed().toString();
}

bool study_search_matches(QStringView text, QStringView query) {
    const QString needle = search_key(normalized_study_query(query));
    return !needle.isEmpty() && search_key(text).contains(needle);
}

QList<StudySearchRange> study_search_ranges(QStringView text, QStringView query) {
    const QString needle = search_key(normalized_study_query(query));
    if (needle.isEmpty() || text.isEmpty()) {
        return {};
    }

    // Normalize character by character, keeping each original character's span
    // so a match inside a decomposed character can cover the whole character.
    // Surrogate pairs are normalized together and map to both code units.
    QString normalized;
    QList<MappedCharacter> mapping;
    mapping.reserve(text.size());
    for (qsizetype i = 0; i < text.size();) {
        const qsizetype length = text.at(i).isHighSurrogate() && i + 1 < text.size() ? 2 : 1;
        const QString part = search_key(text.mid(i, length));
        mapping.append({normalized.size(), part.size(), i, length});
        normalized += part;
        i += length;
    }

    QList<StudySearchRange> ranges;
    qsizetype search_start = 0;
    while (search_start < normalized.size()) {
        const qsizetype found = normalized.indexOf(needle, search_start);
        if (found < 0) {
            break;
        }
        const qsizetype found_end = found + needle.size();
        qsizetype first = -1;
        qsizetype last = -1;
        // A match can sit inside one character's expansion (ß -> ss), so the
        // next search resumes after the whole covered character, as Swift's
        // character-based range upper bound does.
        qsizetype covered_end = found_end;
        for (const MappedCharacter& character : std::as_const(mapping)) {
            if (character.normalized_length == 0) {
                continue;
            }
            const qsizetype character_end =
                character.normalized_start + character.normalized_length;
            if (character_end <= found || character.normalized_start >= found_end) {
                continue;
            }
            covered_end = std::max(covered_end, character_end);
            if (first < 0) {
                first = character.original_start;
            }
            last = character.original_start + character.original_length;
        }
        if (first >= 0) {
            ranges.append({first, last - first});
        }
        search_start = covered_end;
    }
    return ranges;
}

}  // namespace niratan::player

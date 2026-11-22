// SPDX-License-Identifier: GPL-3.0-or-later
//
// Audiobook metadata and chapter state of SasayakiViewModel, following
// Features/Sasayaki/SasayakiPlayer.swift (loadAudiobookMetadata,
// currentAudiobookChapterID, teardown) and
// Features/Sasayaki/SasayakiAudiobookMetadata.swift.
//
// When an audio file becomes the source, open_audio_source resets the previous
// values, then reads the embedded metadata and the chpl chapter list from that
// path, in that order, marking isLoadingAudiobookChapters while the reads run.
// A file that could not be opened never reaches this function, so its
// predecessor's metadata stays. chapters exposes id, title, startTime and
// endTime (null when unknown), and currentAudiobookChapterId follows the
// player's last-chapter inclusive end rule; -1 means no chapter contains the
// position. The reads themselves live in services/audiobook.

#include "sasayaki_view_model.h"

#include <QMediaPlayer>
#include <QVariantMap>

#include <string>
#include <vector>

#include "models/sasayaki/sasayaki.h"
#include "sasayaki_view_model_state.h"
#include "services/audiobook/audiobook_metadata.h"

namespace niratan::viewmodels {
namespace {

QString optional_text(const std::optional<std::string>& value) {
    return value.has_value() ? QString::fromStdString(*value) : QString();
}

QVariantList chapter_list(const std::vector<sasayaki::SasayakiAudiobookChapter>& chapters) {
    QVariantList list;
    list.reserve(static_cast<qsizetype>(chapters.size()));
    for (const sasayaki::SasayakiAudiobookChapter& chapter : chapters) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), chapter.id);
        entry.insert(QStringLiteral("title"), QString::fromStdString(chapter.title));
        entry.insert(QStringLiteral("startTime"), chapter.start_time);
        entry.insert(QStringLiteral("endTime"),
                     chapter.end_time.has_value() ? QVariant(*chapter.end_time) : QVariant());
        list.append(entry);
    }
    return list;
}

}  // namespace

QString SasayakiViewModel::audiobook_title() const {
    return optional_text(state_->audiobook_metadata.title);
}

QString SasayakiViewModel::audiobook_artist() const {
    return optional_text(state_->audiobook_metadata.artist);
}

QByteArray SasayakiViewModel::audiobook_artwork() const {
    return state_->audiobook_metadata.artwork_data.value_or(QByteArray());
}

QString SasayakiViewModel::audiobook_artwork_source() const {
    if (!state_->audiobook_metadata.artwork_data.has_value()) {
        return {};
    }
    const QByteArray& bytes = *state_->audiobook_metadata.artwork_data;
    const char* mime = nullptr;
    if (bytes.startsWith(QByteArrayLiteral("\x89PNG\r\n\x1A\n"))) {
        mime = "image/png";
    } else if (bytes.startsWith(QByteArrayLiteral("\xFF\xD8\xFF"))) {
        mime = "image/jpeg";
    }
    if (mime == nullptr) {
        return {};
    }
    return QStringLiteral("data:%1;base64,%2")
        .arg(QLatin1String(mime), QString::fromLatin1(bytes.toBase64()));
}

int SasayakiViewModel::current_audiobook_chapter_id() const {
    return audiobook::current_audiobook_chapter_id(state_->audiobook_chapters, state_->position)
        .value_or(-1);
}

bool SasayakiViewModel::is_loading_audiobook_chapters() const {
    return state_->loading_audiobook_chapters;
}

QVariantList SasayakiViewModel::chapters() const {
    return chapter_list(state_->audiobook_chapters);
}

void SasayakiViewModel::open_audio_source() {
    state_->needs_initial_seek = true;
    state_->player->setSource(QUrl::fromLocalFile(state_->audio_path));
    state_->player->setPlaybackRate(state_->rate);

    // SasayakiPlayer.loadAudiobookMetadata resets first, reads metadata and
    // then chapters, and clears the loading flag in the same task.
    state_->audiobook_metadata = {};
    state_->audiobook_chapters.clear();
    state_->loading_audiobook_chapters = true;
    state_->last_audiobook_chapter_id = -1;
    emit audiobookMetadataChanged();
    emit chaptersChanged();
    emit audiobookChaptersChanged();

    state_->audiobook_metadata = audiobook::read_audiobook_metadata(state_->audio_path);
    state_->audiobook_chapters = audiobook::read_audiobook_chapters(state_->audio_path);
    state_->last_audiobook_chapter_id =
        audiobook::current_audiobook_chapter_id(state_->audiobook_chapters, state_->position)
            .value_or(-1);

    state_->loading_audiobook_chapters = false;
    emit audiobookMetadataChanged();
    emit chaptersChanged();
    emit audiobookChaptersChanged();
}

}  // namespace niratan::viewmodels

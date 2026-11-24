// SPDX-License-Identifier: GPL-3.0-or-later

#include "ttu_sync_sidecars.h"

namespace niratan::sync::detail {
namespace {

struct RemoteReads {
    RemoteSidecars data;
    std::function<void(RemoteSidecars)> callback;

    template <typename T>
    auto receive(std::shared_ptr<RemoteReads> self, std::size_t index,
                 std::optional<T> RemoteSidecars::* field) {
        return [self = std::move(self), index, field](std::optional<T> value, Error error) {
            self->data.*field = std::move(value);
            if (self->data.reads.finished(index, std::move(error))) {
                self->callback(std::move(self->data));
            }
        };
    }
};

}  // namespace

void read_remote(TtuDrive& drive, const DriveSyncFiles& files, bool stats, bool audio,
                 std::function<void(RemoteSidecars)> callback) {
    const auto reads = std::make_shared<RemoteReads>();
    reads->callback = std::move(callback);
    auto read_progress = reads->receive(reads, 0, &RemoteSidecars::progress);
    auto read_stats = reads->receive(reads, 1, &RemoteSidecars::stats);
    auto read_audio = reads->receive(reads, 2, &RemoteSidecars::audio);
    if (files.progress) {
        drive.get_progress_file(files.progress->id, std::move(read_progress));
    } else {
        read_progress({}, {});
    }
    if (stats && files.statistics) {
        drive.get_stats_file(files.statistics->id, std::move(read_stats));
    } else {
        read_stats({}, {});
    }
    if (audio && files.audio_book) {
        drive.get_audio_book_file(files.audio_book->id, std::move(read_audio));
    } else {
        read_audio({}, {});
    }
}

double reference_seconds(std::int64_t milliseconds) {
    return milliseconds / 1000.0 - 978307200.0;
}

void import_progress(books::BookStorage& storage, std::string_view folder,
                     const TtuProgress& value) {
    if (const auto info = storage.load_book_info(folder)) {
        const auto [chapter, progress] =
            info->resolve_character_position(value.explored_char_count).value_or(std::pair{0, 0.0});
        storage.save_bookmark(folder,
                              {chapter, progress, value.explored_char_count,
                               reference_seconds(value.last_bookmark_modified)});
    }
}

void import_audio(books::BookStorage& storage, std::string_view folder, const TtuAudioBook& value) {
    auto playback =
        storage.load_sasayaki_playback(folder).value_or(sasayaki::SasayakiPlaybackData{});
    playback.last_position = value.playback_position;
    storage.save_sasayaki_playback(folder, playback);
}

}  // namespace niratan::sync::detail

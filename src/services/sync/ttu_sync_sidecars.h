// SPDX-License-Identifier: GPL-3.0-or-later
// Private TTU sync sidecar operations following SyncManager.swift.

#pragma once

#include "ttu_sync.h"

#include <array>

namespace niratan::sync::detail {

using Error = std::optional<GoogleDriveError>;
using Rows = std::vector<statistics::Statistics>;

// Await every operation, then report errors in Swift's await order.
template <std::size_t Count> struct Batch {
    std::size_t remaining = Count;
    std::array<Error, Count> errors;

    bool finished(std::size_t index, Error error) {
        errors[index] = std::move(error);
        return --remaining == 0;
    }

    Error failure() const {
        for (const auto& error : errors) {
            if (error) {
                return error;
            }
        }
        return std::nullopt;
    }
};

struct RemoteSidecars {
    std::optional<TtuProgress> progress;
    std::optional<Rows> stats;
    std::optional<TtuAudioBook> audio;
    Batch<3> reads;
};

void read_remote(TtuDrive& drive, const DriveSyncFiles& files, bool stats, bool audio,
                 std::function<void(RemoteSidecars)> callback);
double reference_seconds(std::int64_t milliseconds);
void import_progress(books::BookStorage& storage, std::string_view folder,
                     const TtuProgress& value);
void import_audio(books::BookStorage& storage, std::string_view folder, const TtuAudioBook& value);

}  // namespace niratan::sync::detail

// SPDX-License-Identifier: GPL-3.0-or-later
// SyncManager.swift: syncBook, syncBookOnce, importGoogleDriveBook and the
// bookmark, statistics and audiobook import/export helpers.
// A Drive 404 clears both folder caches and retries the whole book once.
// Import-only can import newer audio before an otherwise synced result, based
// on the local playback sidecar's modification time. Progress export stamps
// the saved bookmark with the same truncated Unix milliseconds sent to Drive.
// Zip conversion is injected; without a converter no zip transfer is started.
// An import without a converter or extracted book reports unavailable.

#pragma once

#include "services/books/book_storage.h"
#include "ttu_drive.h"
#include "ttu_progress.h"

#include <QTimeZone>

namespace niratan::sync {

struct BookDataConverter {
    using Convert = std::function<std::optional<std::filesystem::path>(
        const std::filesystem::path&, const std::filesystem::path&)>;
    Convert export_zip;
    Convert import_zip;
};

struct TtuSyncOptions {
    bool sync_book_data = false;
    bool sync_stats = false;
    statistics::StatisticsSyncMode stats_mode = statistics::StatisticsSyncMode::merge;
    bool sync_audio_book = false;
    bool import_only = false;
};

class TtuSync : public QObject {
    Q_OBJECT

   public:
    TtuSync(TtuDrive& drive, books::BookStorage& storage, BookDataConverter converter,
            int reset_minutes, QTimeZone zone, QObject* parent = nullptr);
    ~TtuSync() override;
    void sync_book(books::BookMetadata book, std::optional<SyncDirection> direction,
                   TtuSyncOptions options, TtuDrive::ResultCallback<TtuResult> callback);
    void import_google_drive_book(DriveSyncFiles files, bool sync_stats, bool sync_audio_book,
                                  DriveTransport::Progress progress,
                                  TtuDrive::ResultCallback<std::filesystem::path> callback);

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::sync

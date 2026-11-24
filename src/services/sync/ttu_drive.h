// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveHandler.swift: folder lookup/creation, listing, media I/O and uploads.
// Calls complete asynchronously in the transport's callback style; cached
// results may complete inline. Root and sanitized title-to-folder IDs are
// cached in memory. Upload names and DTOs are consumed by the TTU reader.
// A folder's cover is requested only on creation; cover upload errors do not
// prevent returning the folder. Other failures are GoogleDriveError values.

#pragma once

#include "drive_client.h"
#include "drive_files.h"
#include "models/statistics/statistics.h"

#include <filesystem>
#include <map>

namespace niratan::sync {

class TtuDrive : public QObject {
    Q_OBJECT

   public:
    using ErrorCallback = std::function<void(std::optional<GoogleDriveError>)>;
    template <typename T>
    using ResultCallback = std::function<void(std::optional<T>, std::optional<GoogleDriveError>)>;
    using FolderFiles = std::map<std::string, DriveSyncFiles>;

    struct CoverProvider {
        std::function<std::optional<QByteArray>()> data;
    };

    explicit TtuDrive(DriveTransport& transport, QObject* parent = nullptr);
    ~TtuDrive() override;
    void find_root_folder(ResultCallback<std::string> callback);
    void list_books(std::string root_folder, ResultCallback<std::vector<DriveFile>> callback);
    void list_sync_files(std::vector<std::string> folder_ids, ResultCallback<FolderFiles> callback);
    void list_sync_files(std::string folder_id, ResultCallback<DriveSyncFiles> callback);
    void download_file(std::string file_id, DriveTransport::Progress progress,
                       ResultCallback<QByteArray> callback);
    void trash_file(std::string file_id, ErrorCallback callback);
    void get_progress_file(std::string file_id, ResultCallback<TtuProgress> callback);
    void get_stats_file(std::string file_id,
                        ResultCallback<std::vector<statistics::Statistics>> callback);
    void get_audio_book_file(std::string file_id, ResultCallback<TtuAudioBook> callback);
    void update_progress_file(std::string folder_id, std::optional<std::string> file_id,
                              TtuProgress value, ErrorCallback callback);
    void update_stats_file(std::string folder_id, std::optional<std::string> file_id,
                           std::vector<statistics::Statistics> value, ErrorCallback callback);
    void update_audio_book_file(std::string folder_id, std::optional<std::string> file_id,
                                TtuAudioBook value, ErrorCallback callback);
    void ensure_book_folder(std::string title, std::string root_folder, CoverProvider cover,
                            ResultCallback<std::string> callback);
    void upload_book_data(std::string folder_id, std::filesystem::path zip, ErrorCallback callback);
    void clear_cache();

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::sync

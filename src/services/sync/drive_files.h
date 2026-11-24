// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveHandler.swift: DriveFileList, DriveFile, DriveSyncFiles, SyncDirection,
// TtuProgress, TtuAudioBook and the corresponding get/update methods.
// Select the first file with each case-sensitive prefix. Decode optional fields
// as absent when missing or null, and ignore unknown keys. Compare JSON by
// meaning, not object key order; integer fields include the Unix millisecond dates.

#pragma once

#include <QByteArray>
#include <QByteArrayView>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace niratan::sync {

struct DriveFile {
    std::string id;
    std::string name;
    std::optional<std::vector<std::string>> parents;
    std::optional<std::string> thumbnail_link;
};

struct DriveFileList {
    std::vector<DriveFile> files;
    std::optional<std::string> next_page_token;
};

struct DriveSyncFiles {
    std::optional<DriveFile> book_data;
    std::optional<DriveFile> cover;
    std::optional<DriveFile> progress;
    std::optional<DriveFile> statistics;
    std::optional<DriveFile> audio_book;

    explicit DriveSyncFiles(std::vector<DriveFile> files);
};

struct TtuProgress {
    int data_id = 0;
    int explored_char_count = 0;
    double progress = 0;
    std::int64_t last_bookmark_modified = 0;  // Unix milliseconds
};

struct TtuAudioBook {
    std::string title;
    double playback_position = 0;
    std::int64_t last_audio_book_modified = 0;
};

enum class SyncDirection { import_from_ttu, export_to_ttu, synced };

bool parse_drive_file_list(QByteArrayView json, DriveFileList& out, std::string& error);
bool parse_ttu_progress(QByteArrayView json, TtuProgress& out, std::string& error);
QByteArray encode_ttu_progress(const TtuProgress& progress);
bool parse_ttu_audio_book(QByteArrayView json, TtuAudioBook& out, std::string& error);
QByteArray encode_ttu_audio_book(const TtuAudioBook& audio_book);

}  // namespace niratan::sync

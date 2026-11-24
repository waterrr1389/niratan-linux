// SPDX-License-Identifier: GPL-3.0-or-later

#include "drive_files.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <limits>
#include <utility>

namespace niratan::sync {
namespace {

bool read_object(QByteArrayView json, QJsonObject& object, std::string& error) {
    QJsonParseError parse_error;
    const auto document = QJsonDocument::fromJson(json.toByteArray(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        error = parse_error.errorString().toStdString();
        return false;
    }
    if (!document.isObject()) {
        error = "Expected a JSON object";
        return false;
    }
    object = document.object();
    return true;
}

bool invalid_field(std::string& error, const char* key) {
    error = std::string("Missing or invalid field: ") + key;
    return false;
}

bool absent(const QJsonValue& value) {
    return value.isUndefined() || value.isNull();
}

bool read_string(const QJsonObject& object, const char* key, std::string& out, std::string& error) {
    const auto value = object.value(QLatin1StringView(key));
    if (!value.isString()) {
        return invalid_field(error, key);
    }
    out = value.toString().toStdString();
    return true;
}

bool read_optional_string(const QJsonObject& object, const char* key,
                          std::optional<std::string>& out, std::string& error) {
    if (absent(object.value(QLatin1StringView(key)))) {
        out.reset();
        return true;
    }
    return read_string(object, key, out.emplace(), error);
}

bool read_double(const QJsonObject& object, const char* key, double& out, std::string& error) {
    const auto value = object.value(QLatin1StringView(key));
    if (!value.isDouble()) {
        return invalid_field(error, key);
    }
    out = value.toDouble();
    return true;
}

template <typename Int>
bool read_integer(const QJsonObject& object, const char* key, Int& out, std::string& error) {
    const auto value = object.value(QLatin1StringView(key));
    constexpr auto min = std::numeric_limits<qint64>::min();
    constexpr auto max = std::numeric_limits<qint64>::max();
    const auto integer = value.toInteger(min);
    // Use a second fallback to distinguish a legitimate minimum from a failed conversion.
    if ((integer == min && value.toInteger(max) != min) || !std::in_range<Int>(integer)) {
        return invalid_field(error, key);
    }
    out = static_cast<Int>(integer);
    return true;
}

bool read_file(const QJsonValue& value, DriveFile& file, std::string& error) {
    if (!value.isObject()) {
        return invalid_field(error, "files");
    }
    const auto object = value.toObject();
    if (!read_string(object, "id", file.id, error) ||
        !read_string(object, "name", file.name, error) ||
        !read_optional_string(object, "thumbnailLink", file.thumbnail_link, error)) {
        return false;
    }
    const auto parents = object.value("parents");
    if (!absent(parents)) {
        if (!parents.isArray()) {
            return invalid_field(error, "parents");
        }
        auto& ids = file.parents.emplace();
        for (const auto parent : parents.toArray()) {
            if (!parent.isString()) {
                return invalid_field(error, "parents");
            }
            ids.push_back(parent.toString().toStdString());
        }
    }
    return true;
}

}  // namespace

DriveSyncFiles::DriveSyncFiles(std::vector<DriveFile> files) {
    for (auto& file : files) {
        if (!book_data && file.name.starts_with("bookdata_")) {
            book_data = std::move(file);
        } else if (!cover && file.name.starts_with("cover_")) {
            cover = std::move(file);
        } else if (!progress && file.name.starts_with("progress_")) {
            progress = std::move(file);
        } else if (!statistics && file.name.starts_with("statistics_")) {
            statistics = std::move(file);
        } else if (!audio_book && file.name.starts_with("audioBook_")) {
            audio_book = std::move(file);
        }
    }
}

bool parse_drive_file_list(QByteArrayView json, DriveFileList& out, std::string& error) {
    QJsonObject object;
    if (!read_object(json, object, error)) {
        return false;
    }
    const auto files = object.value("files");
    if (!files.isArray()) {
        return invalid_field(error, "files");
    }
    DriveFileList list;
    for (const auto value : files.toArray()) {
        if (!read_file(value, list.files.emplace_back(), error)) {
            return false;
        }
    }
    if (!read_optional_string(object, "nextPageToken", list.next_page_token, error)) {
        return false;
    }
    out = std::move(list);
    error.clear();
    return true;
}

bool parse_ttu_progress(QByteArrayView json, TtuProgress& out, std::string& error) {
    QJsonObject object;
    TtuProgress progress;
    if (!read_object(json, object, error) ||
        !read_integer(object, "dataId", progress.data_id, error) ||
        !read_integer(object, "exploredCharCount", progress.explored_char_count, error) ||
        !read_double(object, "progress", progress.progress, error) ||
        !read_integer(object, "lastBookmarkModified", progress.last_bookmark_modified, error)) {
        return false;
    }
    out = progress;
    error.clear();
    return true;
}

QByteArray encode_ttu_progress(const TtuProgress& progress) {
    return QJsonDocument(QJsonObject{
                             {"dataId", progress.data_id},
                             {"exploredCharCount", progress.explored_char_count},
                             {"progress", progress.progress},
                             {"lastBookmarkModified", qint64(progress.last_bookmark_modified)},
                         })
        .toJson(QJsonDocument::Compact);
}

bool parse_ttu_audio_book(QByteArrayView json, TtuAudioBook& out, std::string& error) {
    QJsonObject object;
    TtuAudioBook audio_book;
    if (!read_object(json, object, error) ||
        !read_string(object, "title", audio_book.title, error) ||
        !read_double(object, "playbackPosition", audio_book.playback_position, error) ||
        !read_integer(object, "lastAudioBookModified", audio_book.last_audio_book_modified,
                      error)) {
        return false;
    }
    out = std::move(audio_book);
    error.clear();
    return true;
}

QByteArray encode_ttu_audio_book(const TtuAudioBook& audio_book) {
    return QJsonDocument(QJsonObject{
                             {"title", QString::fromStdString(audio_book.title)},
                             {"playbackPosition", audio_book.playback_position},
                             {"lastAudioBookModified", qint64(audio_book.last_audio_book_modified)},
                         })
        .toJson(QJsonDocument::Compact);
}

}  // namespace niratan::sync

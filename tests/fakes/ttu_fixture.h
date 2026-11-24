// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "fakes/fake_drive_server.h"
#include "services/sync/ttu_sync.h"

#include <QFile>
#include <QTemporaryDir>

namespace ttu_fixture {

inline constexpr double unix_reference = 978307200.0;
inline constexpr std::int64_t remote_ms = 1690000000000;
inline constexpr auto folder_mime = "application/vnd.google-apps.folder";
inline constexpr auto progress_json =
    R"({"dataId":7,"exploredCharCount":250,"progress":0.25,"lastBookmarkModified":1690000000000})";
inline constexpr auto audio_json =
    R"({"title":"My Book","playbackPosition":42.5,"lastAudioBookModified":1690000001000})";
inline constexpr auto stats_json =
    R"([{"title":"My Book","dateKey":"2023-07-22","charactersRead":1200,"readingTime":600.0,"minReadingSpeed":7200,"altMinReadingSpeed":7200,"lastReadingSpeed":7200,"maxReadingSpeed":7200,"lastStatisticModified":1690000000123}])";

template <typename T> struct Result {
    bool done = false;
    std::optional<T> value;
    std::optional<niratan::sync::GoogleDriveError> error;

    auto receive() {
        return [this](auto result, auto failure) {
            value = std::move(result);
            error = std::move(failure);
            done = true;
        };
    }
};

struct Status {
    bool done = false;
    std::optional<niratan::sync::GoogleDriveError> error;

    auto receive() {
        return [this](auto failure) {
            error = std::move(failure);
            done = true;
        };
    }
};

inline bool write(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

inline std::unique_ptr<niratan::sync::DriveTransport> transport_for(FakeDriveServer& server) {
    return std::make_unique<niratan::sync::DriveTransport>(
        server.base_url(), [] { return niratan::sync::DriveToken{"ttu-token", {}}; },
        niratan::sync::DriveTransport::TokenRefresh{});
}

struct Library {
    QTemporaryDir root;
    niratan::books::BookStorage storage{root.path().toStdString()};
    niratan::books::BookMetadata book;
    niratan::books::BookInfo info;

    Library() {
        book.id = "book";
        book.folder = "book";
        book.title = "My Book";
        book.renamed_title = "Display Title";
        info.character_count = 1000;
        info.chapter_info.emplace("one.xhtml", niratan::books::ChapterInfo{0, 0, 200, {}});
        info.chapter_info.emplace("two.xhtml", niratan::books::ChapterInfo{1, 200, 800, {}});
    }

    bool start() {
        return root.isValid() && storage.save_metadata(book) &&
            storage.save_book_info(book.folder, info);
    }

    bool bookmark(int count, std::int64_t ms) {
        return storage.save_bookmark(book.folder, {1, 0.125, count, ms / 1000.0 - unix_reference});
    }

    std::filesystem::path path() const { return storage.books_directory() / book.folder; }
};

inline void seed_folders(FakeDriveServer& server) {
    server.put_file("root-id", "ttu-reader-data", "root", {}, folder_mime);
    server.put_file("book-id", "My Book", "root-id", {}, folder_mime);
}

inline void seed_media(FakeDriveServer& server) {
    server.put_file("p", "progress_1_6_1690000000000_0.25.json", "book-id", progress_json);
    server.put_file("s", "statistics_1_6_fixture.json", "book-id", stats_json);
    server.put_file("a", "audioBook_1_6_1690000001000_42.5.json", "book-id", audio_json);
}

inline QString query(const FakeDriveServer::Request& request, const QString& key) {
    return request.query.queryItemValue(key, QUrl::FullyDecoded);
}

}  // namespace ttu_fixture

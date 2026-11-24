// SPDX-License-Identifier: GPL-3.0-or-later

#include "local_file_server.h"

#include <QByteArray>
#include <QDebug>
#include <QHostAddress>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QtGlobal>

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "local_audio.h"

namespace niratan::audio {

namespace {

constexpr std::string_view kCoverPrefix = "/cover/cover.";
constexpr std::string_view kSasayakiPath = "/sasayaki/audio.m4a";
constexpr std::string_view kSourceListPath = "/localaudio/get/";
constexpr std::string_view kAudioPrefix = "/localaudio/";
constexpr std::size_t kMaxRequestHeader = 8192;
constexpr int kMaxStartRetries = 10;
constexpr int kStartRetryDelayMs = 300;

constexpr char kEmptySourceList[] = R"({"type":"audioSourceList","audioSources":[]})";
constexpr char kNotFoundType[] = "text/plain; charset=utf-8";

struct Request {
    std::string path;
    std::map<std::string, std::string, std::less<>> query;
};

struct SqliteCloser {
    void operator()(sqlite3* database) const {
        if (database != nullptr) {
            sqlite3_close(database);
        }
    }
};

using Database = std::unique_ptr<sqlite3, SqliteCloser>;

std::string to_std(const QString& text) {
    return text.toStdString();
}

// A single space separated request line. Empty pieces do not count as a
// split, as String.split(separator:) does. A line with fewer than two pieces
// leaves the target at "/".
Request parse_request(const QByteArray& header) {
    std::string target = "/";

    const int line_end = header.indexOf("\r\n");
    const QByteArray first_line = line_end < 0 ? header : header.left(line_end);
    std::vector<std::string> pieces;
    for (const QByteArray& piece : first_line.split(' ')) {
        if (!piece.isEmpty()) {
            pieces.push_back(piece.toStdString());
        }
    }
    if (pieces.size() >= 2) {
        target = pieces[1];
    }

    Request request;
    const QUrl url = QUrl::fromEncoded("http://localhost" + QByteArray::fromStdString(target));
    if (!url.isValid()) {
        request.path = "/";
        return request;
    }
    request.path = to_std(url.path(QUrl::FullyDecoded));
    const QUrlQuery query(url);
    for (const auto& [name, value] : query.queryItems(QUrl::FullyDecoded)) {
        request.query[to_std(name)] = to_std(value);
    }
    return request;
}

std::string second_percent_decode(std::string_view text) {
    const QByteArray bytes(text.data(), static_cast<qsizetype>(text.size()));
    return QUrl::fromPercentEncoding(bytes).toStdString();
}

// String.split(separator: "/", maxSplits: 1): leading separators are skipped
// without counting as a split, and an empty tail is dropped. A tail with
// another separator keeps it in the second piece.
std::vector<std::string_view> split_audio_tail(std::string_view tail) {
    while (tail.starts_with('/')) {
        tail.remove_prefix(1);
    }
    if (tail.empty()) {
        return {};
    }
    const std::size_t slash = tail.find('/');
    if (slash == std::string_view::npos) {
        return {tail};
    }
    const std::string_view remainder = tail.substr(slash + 1);
    if (remainder.empty()) {
        return {tail.substr(0, slash)};
    }
    return {tail.substr(0, slash), remainder};
}

std::string json_string(std::string_view text) {
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string result = "\"";
    for (const char character : text) {
        const auto byte = static_cast<unsigned char>(character);
        switch (character) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (byte < 0x20) {
                    result += "\\u00";
                    result += kHex[byte >> 4];
                    result += kHex[byte & 0x0F];
                } else {
                    result += character;
                }
                break;
        }
    }
    result += '"';
    return result;
}

std::string source_list_response(const std::string& source, const std::string& file, quint16 port) {
    std::string body = R"({"type":"audioSourceList","audioSources":[{"name":)";
    body += json_string(source);
    body += R"(,"url":)";
    body += json_string(local_audio_url(source, file, port));
    body += "}]}";
    return body;
}

Database open_database(const std::string& path) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return {};
    }
    sqlite3* database = nullptr;
    if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
        if (database != nullptr) {
            sqlite3_close(database);
        }
        return {};
    }
    return Database(database);
}

void send_response(QTcpSocket* socket, const char* status, std::string_view content_type,
                   const QByteArray& body) {
    QByteArray response = "HTTP/1.1 ";
    response += status;
    response += "\r\nContent-Type: ";
    response.append(content_type.data(), static_cast<qsizetype>(content_type.size()));
    response += "\r\nContent-Length: ";
    response += QByteArray::number(body.size());
    response += "\r\nConnection: close\r\n\r\n";
    response += body;
    socket->write(response);
    socket->disconnectFromHost();
}

void send_not_found(QTcpSocket* socket) {
    send_response(socket, "404 Not Found", kNotFoundType, {});
}

}  // namespace

LocalFileServer::LocalFileServer(std::string database_path, quint16 port, QObject* parent)
    : QObject(parent),
      database_path_(std::move(database_path)),
      requested_port_(port),
      server_(this),
      retry_timer_(this) {
    connect(&server_, &QTcpServer::newConnection, this, &LocalFileServer::handle_connection);
    retry_timer_.setSingleShot(true);
    connect(&retry_timer_, &QTimer::timeout, this, &LocalFileServer::start);
}

LocalFileServer::~LocalFileServer() = default;

void LocalFileServer::set_cover(QByteArray data) {
    cover_ = std::move(data);
    start();
}

void LocalFileServer::set_sasayaki_audio(QByteArray data) {
    sasayaki_audio_ = std::move(data);
    start();
}

void LocalFileServer::clear_media() {
    cover_.reset();
    sasayaki_audio_.reset();
    stop_if_idle();
}

void LocalFileServer::set_audio_enabled(bool enabled) {
    if (audio_enabled_ == enabled) {
        if (enabled) {
            start();
        }
        return;
    }
    audio_enabled_ = enabled;
    if (enabled) {
        if (server_.isListening()) {
            server_.close();
        }
        retry_timer_.stop();
        retry_count_ = 0;
        start();
    } else {
        stop_if_idle();
    }
}

bool LocalFileServer::is_listening() const {
    return server_.isListening();
}

void LocalFileServer::set_book_images(
    std::string route, std::function<std::optional<BookImage>(std::string_view)> lookup) {
    book_image_route_ = std::move(route);
    book_image_lookup_ = std::move(lookup);
    if (book_image_lookup_) {
        start();
    } else {
        stop_if_idle();
    }
}

quint16 LocalFileServer::port() const {
    return server_.isListening() ? server_.serverPort() : 0;
}

void LocalFileServer::handle_connection() {
    while (QTcpSocket* socket = server_.nextPendingConnection()) {
        if (!socket->peerAddress().isLoopback()) {
            socket->abort();
            socket->deleteLater();
            continue;
        }

        auto buffer = std::make_shared<QByteArray>();
        auto handled = std::make_shared<bool>(false);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer, handled] {
            if (*handled) {
                return;
            }
            buffer->append(socket->readAll());
            const int header_end = buffer->indexOf("\r\n\r\n");
            if (header_end < 0 && buffer->size() < static_cast<qsizetype>(kMaxRequestHeader)) {
                return;
            }
            *handled = true;
            const QByteArray header =
                header_end < 0 ? *buffer : buffer->left(header_end);
            handle_request(socket, header);
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void LocalFileServer::handle_request(QTcpSocket* socket, const QByteArray& header) {
    const Request request = parse_request(header);

    if (book_image_lookup_ && request.path.starts_with(book_image_route_)) {
        const auto image = book_image_lookup_(std::string_view(request.path).substr(book_image_route_.size()));
        if (image) {
            send_response(socket, "200 OK", image->first, image->second);
        } else {
            send_not_found(socket);
        }
        return;
    }

    if (request.path.starts_with(kCoverPrefix)) {
        if (!cover_) {
            send_not_found(socket);
            return;
        }
        send_response(socket, "200 OK", "application/octet-stream", *cover_);
        return;
    }

    if (request.path == kSasayakiPath) {
        if (!sasayaki_audio_) {
            send_not_found(socket);
            return;
        }
        send_response(socket, "200 OK", "audio/x-m4a", *sasayaki_audio_);
        return;
    }

    if (request.path == kSourceListPath) {
        const auto term = request.query.find("term");
        const auto reading = request.query.find("reading");
        const std::string expression = term == request.query.end() ? "" : term->second;
        const std::string raw_reading = reading == request.query.end() ? "" : reading->second;

        const Database database = open_database(database_path_);
        if (!database) {
            send_response(socket, "200 OK", "application/json", kEmptySourceList);
            return;
        }
        const std::optional<LocalAudioEntry> entry =
            find_local_audio(database.get(), expression, raw_reading);
        if (!entry) {
            send_response(socket, "200 OK", "application/json", kEmptySourceList);
            return;
        }
        const std::string body = source_list_response(entry->source, entry->file, port());
        send_response(socket, "200 OK", "application/json",
                      QByteArray(body.data(), static_cast<qsizetype>(body.size())));
        return;
    }

    if (request.path.starts_with(kAudioPrefix)) {
        const std::vector<std::string_view> parts =
            split_audio_tail(std::string_view(request.path).substr(kAudioPrefix.size()));
        if (parts.size() != 2) {
            send_not_found(socket);
            return;
        }
        const std::string source(parts[0]);
        const std::string file = second_percent_decode(parts[1]);

        const Database database = open_database(database_path_);
        if (!database) {
            send_not_found(socket);
            return;
        }

        sqlite3_stmt* statement = nullptr;
        const char* sql = "SELECT data FROM android WHERE source = ? AND file = ?;";
        if (sqlite3_prepare_v2(database.get(), sql, -1, &statement, nullptr) != SQLITE_OK) {
            sqlite3_finalize(statement);
            send_not_found(socket);
            return;
        }
        sqlite3_bind_text(statement, 1, source.c_str(), static_cast<int>(source.size()),
                          SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, file.c_str(), static_cast<int>(file.size()),
                          SQLITE_TRANSIENT);

        QByteArray audio;
        bool found = false;
        if (sqlite3_step(statement) == SQLITE_ROW) {
            const void* bytes = sqlite3_column_blob(statement, 0);
            if (bytes != nullptr) {
                const int size = sqlite3_column_bytes(statement, 0);
                audio = QByteArray(static_cast<const char*>(bytes), size);
                found = true;
            }
        }
        sqlite3_finalize(statement);

        if (!found) {
            send_not_found(socket);
            return;
        }
        send_response(socket, "200 OK", audio_mime_type(file), audio);
        return;
    }

    send_not_found(socket);
}

void LocalFileServer::start() {
    if (server_.isListening()) {
        return;
    }
    if (server_.listen(book_image_lookup_ ? QHostAddress::LocalHost : QHostAddress::Any,
                       requested_port_)) {
        retry_count_ = 0;
        retry_timer_.stop();
        return;
    }

    ++retry_count_;
    qWarning("Local media server failed to listen on port %u", unsigned(requested_port_));
    if (retry_count_ <= kMaxStartRetries) {
        retry_timer_.start(kStartRetryDelayMs);
    }
}

void LocalFileServer::stop_if_idle() {
    if (cover_ || sasayaki_audio_ || audio_enabled_ || book_image_lookup_) {
        return;
    }
    retry_timer_.stop();
    retry_count_ = 0;
    if (server_.isListening()) {
        server_.close();
    }
}

}  // namespace niratan::audio

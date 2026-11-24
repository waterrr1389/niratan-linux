// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/audio/local_file_server.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QString>
#include <QTcpSocket>
#include <QUrl>
#include <QTemporaryDir>
#include <QtTest>

#include <sqlite3.h>

#include <array>
#include <optional>
#include <string>

using niratan::audio::LocalFileServer;

// Core/LocalFileServer.swift (respond, getCover, getSasayakiAudio,
// getAudioSources, getAudio, parseRequest, send, setCover, setSasayakiAudio,
// setAudioServer, clearMedia) has no test script of its own, so the cases
// follow its routes and responses directly. Requests go over a real loopback
// socket; the android database is built in a temporary directory.
namespace {

constexpr char kEmptySourceList[] = "{\"type\":\"audioSourceList\",\"audioSources\":[]}";

struct TestDatabase {
    QTemporaryDir directory;
    std::string path;
    sqlite3* db = nullptr;

    TestDatabase() {
        path = directory.filePath(QStringLiteral("android.db")).toStdString();
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            sqlite3_close(db);
            db = nullptr;
            return;
        }
        execute("CREATE TABLE entries (expression TEXT, reading TEXT, source TEXT, file TEXT)");
        execute("CREATE TABLE android (source TEXT, file TEXT, data BLOB)");
    }

    ~TestDatabase() {
        if (db != nullptr) {
            sqlite3_close(db);
        }
    }

    TestDatabase(const TestDatabase&) = delete;
    TestDatabase& operator=(const TestDatabase&) = delete;

    void execute(const char* sql) { sqlite3_exec(db, sql, nullptr, nullptr, nullptr); }

    void add_entry(std::string_view expression, std::string_view reading, std::string_view source,
                   std::string_view file) {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, "INSERT INTO entries VALUES (?, ?, ?, ?)", -1, &statement,
                               nullptr) != SQLITE_OK) {
            return;
        }
        const std::array<std::string_view, 4> values{expression, reading, source, file};
        for (std::size_t index = 0; index < values.size(); ++index) {
            sqlite3_bind_text(statement, static_cast<int>(index + 1), values[index].data(),
                              static_cast<int>(values[index].size()), SQLITE_TRANSIENT);
        }
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }

    void add_audio(std::string_view source, std::string_view file, const QByteArray& data) {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, "INSERT INTO android VALUES (?, ?, ?)", -1, &statement,
                               nullptr) != SQLITE_OK) {
            return;
        }
        sqlite3_bind_text(statement, 1, source.data(), static_cast<int>(source.size()),
                          SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, file.data(), static_cast<int>(file.size()),
                          SQLITE_TRANSIENT);
        sqlite3_bind_blob(statement, 3, data.constData(), data.size(), SQLITE_TRANSIENT);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }
};

struct Response {
    bool complete = false;
    int status = 0;
    QByteArray header;
    QByteArray content_type;
    QByteArray body;
};

QByteArray encoded(std::string_view text) {
    return QUrl::toPercentEncoding(QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())));
}

Response parse_response(const QByteArray& raw) {
    Response result;
    const int header_end = raw.indexOf("\r\n\r\n");
    if (header_end < 0) {
        return result;
    }
    const QByteArray header = raw.left(header_end);
    const QList<QByteArray> lines = header.split('\n');
    if (lines.isEmpty()) {
        return result;
    }
    const QList<QByteArray> status_parts = lines.front().trimmed().split(' ');
    if (status_parts.size() < 2) {
        return result;
    }
    result.status = status_parts[1].toInt();

    qsizetype length = -1;
    for (const QByteArray& line : lines) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith("Content-Type: ")) {
            result.content_type = trimmed.mid(QByteArray("Content-Type: ").size());
        } else if (trimmed.startsWith("Content-Length: ")) {
            length = trimmed.mid(QByteArray("Content-Length: ").size()).toLongLong();
        }
    }
    if (length < 0 || raw.size() < header_end + 4 + length) {
        return result;
    }
    result.header = header;
    result.body = raw.mid(header_end + 4, length);
    result.complete = true;
    return result;
}

// The client and the server share one event loop, so the request is pumped
// with QTest::qWait instead of a blocking waitForReadyRead.
Response exchange(quint16 port, const QByteArray& target) {
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);

    QElapsedTimer timer;
    timer.start();
    while (socket.state() != QAbstractSocket::ConnectedState) {
        if (timer.elapsed() > 2000) {
            return {};
        }
        QTest::qWait(1);
    }
    socket.write("GET " + target + " HTTP/1.1\r\nHost: localhost\r\n\r\n");

    QByteArray raw;
    while (socket.state() != QAbstractSocket::UnconnectedState && timer.elapsed() < 4000) {
        QTest::qWait(2);
        raw += socket.readAll();
    }
    raw += socket.readAll();
    return parse_response(raw);
}

}  // namespace

class LocalFileServerTest : public QObject {
    Q_OBJECT

   private slots:
    void unknown_routes_are_not_found();
    void book_images_use_the_active_route();
    void cover_route_serves_the_cover_bytes();
    void cover_route_matches_the_prefix();
    void sasayaki_route_is_exact();
    void a_missing_database_returns_the_empty_source_list();
    void the_source_list_returns_one_entry();
    void the_source_list_converts_the_reading_to_hiragana();
    void repeated_query_names_keep_the_last_value();
    void a_blob_is_served_with_its_mime_type();
    void a_blob_decodes_the_file_twice();
    void a_missing_blob_is_not_found();
    void an_audio_path_needs_a_source_and_a_file();
    void json_escapes_quotes_and_control_characters();
    void the_listener_stops_when_nothing_remains();
    void responses_close_the_connection();
    void a_source_list_without_a_match_is_empty();
};

void LocalFileServerTest::book_images_use_the_active_route() {
    LocalFileServer server({}, 0);
    server.set_book_images("/book-images/current/", [](std::string_view path)
        -> std::optional<LocalFileServer::BookImage> {
        if (path == "OEBPS/images/cover #1.png") {
            return LocalFileServer::BookImage{"image/png", "image bytes"};
        }
        return std::nullopt;
    });
    QVERIFY(server.is_listening());

    const auto image = exchange(server.port(), "/book-images/current/OEBPS/images/cover%20%231.png");
    QVERIFY(image.complete);
    QCOMPARE(image.status, 200);
    QCOMPARE(image.content_type, QByteArray("image/png"));
    QCOMPARE(image.body, QByteArray("image bytes"));
    QCOMPARE(exchange(server.port(), "/book-images/previous/OEBPS/images/cover%20%231.png").status, 404);
    QCOMPARE(exchange(server.port(), "/book-images/current/OEBPS/content.opf").status, 404);

    server.set_book_images({}, {});
    QVERIFY(!server.is_listening());
}

void LocalFileServerTest::unknown_routes_are_not_found() {
    LocalFileServer server("", 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    const Response response = exchange(server.port(), "/nope");

    QVERIFY(response.complete);
    QCOMPARE(response.status, 404);
    QCOMPARE(response.content_type, QByteArray("text/plain; charset=utf-8"));
    QVERIFY(response.body.isEmpty());
}

void LocalFileServerTest::cover_route_serves_the_cover_bytes() {
    LocalFileServer server("", 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    const Response missing = exchange(server.port(), "/cover/cover.png");
    QVERIFY(missing.complete);
    QCOMPARE(missing.status, 404);

    server.set_cover(QByteArray("PNG"));
    const Response response = exchange(server.port(), "/cover/cover.png");

    QVERIFY(response.complete);
    QCOMPARE(response.status, 200);
    QCOMPARE(response.content_type, QByteArray("application/octet-stream"));
    QCOMPARE(response.body, QByteArray("PNG"));
}

void LocalFileServerTest::cover_route_matches_the_prefix() {
    LocalFileServer server("", 0);
    server.set_cover(QByteArray("PNG"));
    QTRY_VERIFY(server.is_listening());

    QCOMPARE(exchange(server.port(), "/cover/cover.").status, 200);
    QCOMPARE(exchange(server.port(), "/cover/other.png").status, 404);
}

void LocalFileServerTest::sasayaki_route_is_exact() {
    LocalFileServer server("", 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    QCOMPARE(exchange(server.port(), "/sasayaki/audio.m4a").status, 404);

    server.set_sasayaki_audio(QByteArray("M4A"));
    const Response response = exchange(server.port(), "/sasayaki/audio.m4a?term=x");

    QVERIFY(response.complete);
    QCOMPARE(response.status, 200);
    QCOMPARE(response.content_type, QByteArray("audio/x-m4a"));
    QCOMPARE(response.body, QByteArray("M4A"));
}

void LocalFileServerTest::a_missing_database_returns_the_empty_source_list() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LocalFileServer server(directory.filePath(QStringLiteral("missing.db")).toStdString(), 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    const Response response = exchange(server.port(), "/localaudio/get/?term=%E7%8C%AB");

    QVERIFY(response.complete);
    QCOMPARE(response.status, 200);
    QCOMPARE(response.content_type, QByteArray("application/json"));
    QCOMPARE(response.body, QByteArray(kEmptySourceList));
}

void LocalFileServerTest::the_source_list_returns_one_entry() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.add_entry("cat", "", "nhk16", "cat x.mp3");

    LocalFileServer server(database.path, 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    const Response response = exchange(server.port(), "/localaudio/get/?term=cat");

    QVERIFY(response.complete);
    QCOMPARE(response.status, 200);
    const QByteArray expected =
        QByteArray("{\"type\":\"audioSourceList\",\"audioSources\":[{\"name\":\"nhk16\",\"url\":"
                   "\"http://localhost:") +
        QByteArray::number(server.port()) + "/localaudio/nhk16/cat%20x.mp3\"}]}";
    QCOMPARE(response.body, expected);
}

void LocalFileServerTest::the_source_list_converts_the_reading_to_hiragana() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.add_entry("\u732B", "\u306D\u3053", "forvo", "cat.mp3");

    LocalFileServer server(database.path, 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    const Response response =
        exchange(server.port(), "/localaudio/get/?term=" + encoded("\u732B") +
                                    "&reading=" + encoded("\u30CD\u30B3"));

    QVERIFY(response.complete);
    QVERIFY(!response.body.contains("\"nhk16\""));
    QVERIFY(response.body.contains("\"forvo\""));
}

void LocalFileServerTest::repeated_query_names_keep_the_last_value() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.add_entry("\u72AC", "", "nhk16", "dog.mp3");
    database.add_entry("\u732B", "", "forvo", "cat.mp3");

    LocalFileServer server(database.path, 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    const Response response = exchange(
        server.port(), "/localaudio/get/?term=" + encoded("\u72AC") + "&term=" + encoded("\u732B"));

    QVERIFY(response.complete);
    QVERIFY(response.body.contains("\"forvo\""));
}

void LocalFileServerTest::a_blob_is_served_with_its_mime_type() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.add_audio("forvo", "cat file.mp3", QByteArray("ID3"));

    LocalFileServer server(database.path, 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    const Response response = exchange(server.port(), "/localaudio/forvo/cat%20file.mp3");

    QVERIFY(response.complete);
    QCOMPARE(response.status, 200);
    QCOMPARE(response.content_type, QByteArray("audio/mpeg"));
    QCOMPARE(response.body, QByteArray("ID3"));
}

void LocalFileServerTest::a_blob_decodes_the_file_twice() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.add_audio("forvo", "cat file.mp3", QByteArray("ID3"));

    LocalFileServer server(database.path, 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    QCOMPARE(exchange(server.port(), "/localaudio/forvo/cat%20file.mp3").status, 200);
    QCOMPARE(exchange(server.port(), "/localaudio/forvo/cat%2520file.mp3").status, 200);
}

void LocalFileServerTest::a_missing_blob_is_not_found() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);

    LocalFileServer server(database.path, 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    const Response response = exchange(server.port(), "/localaudio/forvo/missing.mp3");

    QVERIFY(response.complete);
    QCOMPARE(response.status, 404);
    QVERIFY(response.body.isEmpty());
}

void LocalFileServerTest::an_audio_path_needs_a_source_and_a_file() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);

    LocalFileServer server(database.path, 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    QCOMPARE(exchange(server.port(), "/localaudio/forvo").status, 404);
    QCOMPARE(exchange(server.port(), "/localaudio/forvo/").status, 404);
    QCOMPARE(exchange(server.port(), "/localaudio//file.mp3").status, 404);
}

void LocalFileServerTest::json_escapes_quotes_and_control_characters() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.add_entry("term", "", "a\"b\\c", "x\ny.mp3");

    LocalFileServer server(database.path, 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    const Response response = exchange(server.port(), "/localaudio/get/?term=term");

    QVERIFY(response.complete);
    QVERIFY(response.body.contains("\"name\":\"a\\\"b\\\\c\""));
    QVERIFY(response.body.contains("x%0Ay.mp3"));
}

void LocalFileServerTest::the_listener_stops_when_nothing_remains() {
    LocalFileServer server("", 0);
    QVERIFY(!server.is_listening());

    server.set_cover(QByteArray("PNG"));
    QTRY_VERIFY(server.is_listening());

    server.clear_media();
    QTRY_VERIFY(!server.is_listening());

    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    server.set_audio_enabled(false);
    QTRY_VERIFY(!server.is_listening());
}

void LocalFileServerTest::responses_close_the_connection() {
    LocalFileServer server("", 0);
    server.set_cover(QByteArray("PNG"));
    QTRY_VERIFY(server.is_listening());

    const Response response = exchange(server.port(), "/cover/cover.png");

    QVERIFY(response.complete);
    QVERIFY(response.header.startsWith("HTTP/1.1 200 OK\r\n"));
    QVERIFY(response.header.contains("Content-Type: application/octet-stream\r\n"));
    QVERIFY(response.header.contains("Content-Length: 3\r\n"));
    QVERIFY(response.header.endsWith("Connection: close"));
}

void LocalFileServerTest::a_source_list_without_a_match_is_empty() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.add_entry("dog", "", "nhk16", "dog.mp3");

    LocalFileServer server(database.path, 0);
    server.set_audio_enabled(true);
    QTRY_VERIFY(server.is_listening());

    const Response response = exchange(server.port(), "/localaudio/get/?term=cat");

    QVERIFY(response.complete);
    QCOMPARE(response.status, 200);
    QCOMPARE(response.body, QByteArray(kEmptySourceList));
}

QTEST_GUILESS_MAIN(LocalFileServerTest)
#include "tst_local_file_server.moc"

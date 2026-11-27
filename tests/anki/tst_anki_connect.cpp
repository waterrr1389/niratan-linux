// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/anki_connect.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonArray>
#include <QTest>

#include <optional>
#include <string>
#include <utility>

using niratan::anki::AnkiConnectClient;
using niratan::anki::AnkiConnectConfig;
using niratan::anki::AnkiConnectResult;

// Core/AnkiManager.swift (ankiConnectRequest(action:params:),
// AnkiConnectConfig, AnkiConnectError) has no test script of its own, so the
// cases follow the request and response handling directly against a stub
// AnkiConnect server on loopback.
namespace {

struct StubServer {
    QTcpServer server;

    StubServer() { server.listen(QHostAddress::LocalHost, 0); }

    quint16 port() const { return server.serverPort(); }
};

std::string url_for(const StubServer& stub) {
    return "http://127.0.0.1:" + std::to_string(stub.port());
}

struct ReceivedRequest {
    bool complete = false;
    QTcpSocket* socket = nullptr;
    QByteArray header;
    QByteArray body;
};

ReceivedRequest receive_request(QTcpServer& server) {
    ReceivedRequest result;
    QElapsedTimer timer;
    timer.start();

    QTcpSocket* socket = nullptr;
    while (socket == nullptr && timer.elapsed() < 2000) {
        if (server.hasPendingConnections()) {
            socket = server.nextPendingConnection();
        } else {
            QTest::qWait(1);
        }
    }
    if (socket == nullptr) {
        return result;
    }
    result.socket = socket;

    QByteArray raw;
    qsizetype body_length = -1;
    while (timer.elapsed() < 2000) {
        QTest::qWait(1);
        raw += socket->readAll();
        const int header_end = raw.indexOf("\r\n\r\n");
        if (header_end >= 0 && body_length < 0) {
            result.header = raw.left(header_end);
            for (const QByteArray& line : result.header.split('\n')) {
                const QByteArray trimmed = line.trimmed();
                if (trimmed.startsWith("Content-Length: ")) {
                    body_length = trimmed.mid(16).toLongLong();
                }
            }
        }
        if (header_end >= 0 && body_length >= 0 && raw.size() >= header_end + 4 + body_length) {
            result.body = raw.mid(header_end + 4, body_length);
            result.complete = true;
            break;
        }
    }
    return result;
}

void respond(QTcpSocket* socket, const QByteArray& body, int status = 200) {
    QByteArray response =
        status == 200 ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 500 Internal Server Error\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;
    socket->write(response);
    socket->disconnectFromHost();
}

}  // namespace

class AnkiConnectTest : public QObject {
    Q_OBJECT

   private slots:
    void a_request_carries_the_action_and_version();
    void params_and_the_api_key_are_sent();
    void an_empty_api_key_is_omitted();
    void an_error_string_fails_the_request();
    void a_null_error_succeeds();
    void a_non_object_response_is_a_null_result();
    void a_missing_result_is_null();
    void a_non_json_body_fails();
    void an_http_error_status_still_parses_the_body();
    void a_refused_connection_fails();
    void an_invalid_url_fails_without_a_request();
    void a_timeout_fails();
};

void AnkiConnectTest::a_request_carries_the_action_and_version() {
    StubServer stub;
    QVERIFY(stub.server.isListening());
    AnkiConnectClient client({url_for(stub), 1000, std::nullopt});

    std::optional<AnkiConnectResult> result;
    client.request("version", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    const ReceivedRequest request = receive_request(stub.server);
    QVERIFY(request.complete);
    QVERIFY(request.header.startsWith("POST / HTTP/1.1"));
    QVERIFY(request.header.contains("Content-Type: application/json"));

    const QJsonObject body = QJsonDocument::fromJson(request.body).object();
    QCOMPARE(body.value("action").toString(), QString("version"));
    QCOMPARE(body.value("version").toInt(), 6);
    QVERIFY(!body.contains("params"));
    QVERIFY(!body.contains("key"));

    respond(request.socket, R"({"result":6,"error":null})");
    QTRY_VERIFY(result.has_value());
    QVERIFY(result->ok);
    QVERIFY(result->error.empty());
    QCOMPARE(result->result.toInt(), 6);
}

void AnkiConnectTest::params_and_the_api_key_are_sent() {
    StubServer stub;
    AnkiConnectClient client({url_for(stub), 1000, std::string("secret")});

    std::optional<AnkiConnectResult> result;
    client.request("findNotes", QJsonObject{{"query", "nid:1"}}, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    const ReceivedRequest request = receive_request(stub.server);
    QVERIFY(request.complete);
    const QJsonObject body = QJsonDocument::fromJson(request.body).object();
    QCOMPARE(body.value("params").toObject().value("query").toString(), QString("nid:1"));
    QCOMPARE(body.value("key").toString(), QString("secret"));

    respond(request.socket, R"({"result":[1],"error":null})");
    QTRY_VERIFY(result.has_value());
    QVERIFY(result->ok);
    QCOMPARE(result->result.toArray().size(), 1);
}

void AnkiConnectTest::an_empty_api_key_is_omitted() {
    StubServer stub;
    AnkiConnectClient client({url_for(stub), 1000, std::string()});

    std::optional<AnkiConnectResult> result;
    client.request("version", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    const ReceivedRequest request = receive_request(stub.server);
    QVERIFY(request.complete);
    QVERIFY(!QJsonDocument::fromJson(request.body).object().contains("key"));

    respond(request.socket, R"({"result":6,"error":null})");
    QTRY_VERIFY(result.has_value());
    QVERIFY(result->ok);
}

void AnkiConnectTest::an_error_string_fails_the_request() {
    StubServer stub;
    AnkiConnectClient client({url_for(stub), 1000, std::nullopt});

    std::optional<AnkiConnectResult> result;
    client.request("addNote", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    const ReceivedRequest request = receive_request(stub.server);
    QVERIFY(request.complete);
    respond(request.socket, R"({"result":null,"error":"cannot create note"})");

    QTRY_VERIFY(result.has_value());
    QVERIFY(!result->ok);
    QCOMPARE(result->error, std::string("cannot create note"));
}

void AnkiConnectTest::a_null_error_succeeds() {
    StubServer stub;
    AnkiConnectClient client({url_for(stub), 1000, std::nullopt});

    std::optional<AnkiConnectResult> result;
    client.request("version", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    const ReceivedRequest request = receive_request(stub.server);
    QVERIFY(request.complete);
    respond(request.socket, R"({"result":"ok","error":null})");

    QTRY_VERIFY(result.has_value());
    QVERIFY(result->ok);
    QCOMPARE(result->result.toString(), QString("ok"));
}

void AnkiConnectTest::a_non_object_response_is_a_null_result() {
    StubServer stub;
    AnkiConnectClient client({url_for(stub), 1000, std::nullopt});

    std::optional<AnkiConnectResult> result;
    client.request("version", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    const ReceivedRequest request = receive_request(stub.server);
    QVERIFY(request.complete);
    respond(request.socket, "[1,2]");

    QTRY_VERIFY(result.has_value());
    QVERIFY(result->ok);
    QVERIFY(result->result.isNull());
}

void AnkiConnectTest::a_missing_result_is_null() {
    StubServer stub;
    AnkiConnectClient client({url_for(stub), 1000, std::nullopt});

    std::optional<AnkiConnectResult> result;
    client.request("version", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    const ReceivedRequest request = receive_request(stub.server);
    QVERIFY(request.complete);
    respond(request.socket, "{}");

    QTRY_VERIFY(result.has_value());
    QVERIFY(result->ok);
    QVERIFY(result->result.isUndefined());
}

void AnkiConnectTest::a_non_json_body_fails() {
    StubServer stub;
    AnkiConnectClient client({url_for(stub), 1000, std::nullopt});

    std::optional<AnkiConnectResult> result;
    client.request("version", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    const ReceivedRequest request = receive_request(stub.server);
    QVERIFY(request.complete);
    respond(request.socket, "not json");

    QTRY_VERIFY(result.has_value());
    QVERIFY(!result->ok);
    QVERIFY(!result->error.empty());
}

void AnkiConnectTest::an_http_error_status_still_parses_the_body() {
    StubServer stub;
    AnkiConnectClient client({url_for(stub), 1000, std::nullopt});

    std::optional<AnkiConnectResult> result;
    client.request("version", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    const ReceivedRequest request = receive_request(stub.server);
    QVERIFY(request.complete);
    respond(request.socket, R"({"result":7,"error":null})", 500);

    QTRY_VERIFY(result.has_value());
    QVERIFY(result->ok);
    QCOMPARE(result->result.toInt(), 7);
}

void AnkiConnectTest::a_refused_connection_fails() {
    quint16 port = 0;
    {
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
        port = probe.serverPort();
    }

    AnkiConnectClient client({"http://127.0.0.1:" + std::to_string(port), 1000, std::nullopt});
    std::optional<AnkiConnectResult> result;
    client.request("version", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), 3000);
    QVERIFY(!result->ok);
    QVERIFY(!result->error.empty());
}

void AnkiConnectTest::an_invalid_url_fails_without_a_request() {
    AnkiConnectClient client({"http://[bad", 1000, std::nullopt});
    std::optional<AnkiConnectResult> result;
    client.request("version", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    QVERIFY(result.has_value());
    QVERIFY(!result->ok);
    QCOMPARE(result->error, std::string("Invalid URL specified"));
}

void AnkiConnectTest::a_timeout_fails() {
    StubServer stub;
    AnkiConnectClient client({url_for(stub), 100, std::nullopt});

    std::optional<AnkiConnectResult> result;
    client.request("version", std::nullopt, [&](AnkiConnectResult value) {
        result = std::move(value);
    });

    receive_request(stub.server);
    QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), 3000);
    QVERIFY(!result->ok);
    QVERIFY(!result->error.empty());
}

QTEST_GUILESS_MAIN(AnkiConnectTest)
#include "tst_anki_connect.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The AnkiConnect transport, following Core/AnkiManager.swift
// (ankiConnectRequest(action:params:), AnkiConnectConfig, AnkiConnectError).
//
// Every request is one POST of a JSON object:
//   {"action": <action>, "version": 6, "params": <params>?, "key": <api key>?}
// "params" is omitted when the caller passes nothing and "key" when the API
// key is empty. The Content-Type is application/json and the transfer times
// out after config.timeout_ms; the Swift request hardcodes 10 seconds, which
// is the default here.
//
// The response is read regardless of the HTTP status, as URLSession does.
// A JSON object with a string "error" fails with that text. Any other JSON
// object, including one with "error": null, succeeds with its "result"
// member, or with null when the member is missing. A response that is not a
// JSON object counts as an empty object and succeeds with null, matching
// JSONSerialization's [String: Any] cast. Bytes that are not JSON fail with
// the parser's message.
//
// Only transport failures (no HTTP response: refused connection, timeout,
// malformed URL) fail without parsing the body. The malformed URL message is
// "Invalid URL specified", the Swift localized error; transport and parser
// messages come from Qt and are not translated.

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QObject>

#include <functional>
#include <optional>
#include <string>

namespace niratan::anki {

struct AnkiConnectConfig {
    std::string url = "http://127.0.0.1:8765";
    // The Swift request uses 10 seconds and does not read the config's own
    // timeout field; this default matches it.
    int timeout_ms = 10000;
    // Omitted from the request body when empty.
    std::optional<std::string> api_key;
};

struct AnkiConnectResult {
    bool ok = false;
    // The "result" member of a successful response. Undefined when the
    // member is absent and Null when it is JSON null, as the Swift
    // dictionary returns nil and NSNull respectively.
    QJsonValue result;
    // Empty on success; the AnkiConnect error text, the transport message or
    // the parser message on failure.
    std::string error;

    bool operator==(const AnkiConnectResult&) const = default;
};

class AnkiConnectClient : public QObject {
    Q_OBJECT

   public:
    using Callback = std::function<void(AnkiConnectResult)>;

    explicit AnkiConnectClient(AnkiConnectConfig config, QObject* parent = nullptr);
    ~AnkiConnectClient() override;

    // The callback runs once on the client's thread. The client must outlive
    // the request; a pending callback is dropped with it.
    void request(std::string action, std::optional<QJsonObject> params, Callback callback);

   private:
    AnkiConnectConfig config_;
    QNetworkAccessManager network_;
};

}  // namespace niratan::anki

// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveAuth.swift, GoogleDriveAuthorizationPolicy.swift, TokenStorage.swift,
// GoogleDriveLoopbackAuthorization.swift and script/test_google_drive_{auth_scope,
// loopback_authorization,credentials}.swift.

#include "fakes/oauth_fixture.h"
#include "services/sync/oauth_credentials.h"
#include "services/sync/oauth_policy.h"
#include "services/sync/sync_config.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace niratan::sync;
using namespace oauth_fixture;
using namespace std::chrono_literals;

namespace {

constexpr auto owner_permissions =
    std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;

struct AuthResult {
    bool finished = false;
    std::optional<AuthError> error;

    auto receive() {
        return [this](std::optional<AuthError> value) {
            finished = true;
            error = std::move(value);
        };
    }
};

struct RefreshResult : AuthResult {
    std::optional<std::string> token;

    auto receive_refresh() {
        return [this](std::optional<std::string> value, std::optional<AuthError> failure) {
            token = std::move(value);
            receive()(std::move(failure));
        };
    }
};

struct LoopbackResult {
    int calls = 0;
    std::optional<LoopbackCode> code;
    std::optional<LoopbackErrorKind> error;

    auto receive() {
        return [this](auto value, auto failure) {
            ++calls;
            code = std::move(value);
            error = failure;
        };
    }
};

QByteArray callback_page(const QByteArray& status, bool accepted) {
    const QByteArray message = accepted
        ? "Google Drive authorization received. Return to Niratan to finish connecting."
        : "This Google Drive callback could not be accepted. Return to Niratan to continue "
          "sign-in.";
    const QByteArray body =
        R"(<!doctype html><html><head><meta charset="utf-8"><meta name="referrer" content="no-referrer"><title>Google Drive sign-in</title></head><body><h1>Google Drive sign-in</h1><p>)" +
        message + "</p></body></html>";
    return "HTTP/1.1 " + status + "\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " +
        QByteArray::number(body.size()) +
        "\r\nCache-Control: no-store"
        "\r\nContent-Security-Policy: default-src 'none'; frame-ancestors 'none'"
        "\r\nConnection: close\r\n\r\n" +
        body;
}

bool seed_credentials(const AuthFixture& fixture) {
    return std::filesystem::create_directories(fixture.credentials().parent_path()) &&
        write(fixture.credentials(), credentials_json);
}

class LockedDirectory {
   public:
    explicit LockedDirectory(std::filesystem::path path) : path_(std::move(path)) {
        std::filesystem::permissions(
            path_, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
    }

    ~LockedDirectory() {
        std::error_code error;
        std::filesystem::permissions(path_, std::filesystem::perms::owner_all, error);
    }

   private:
    std::filesystem::path path_;
};

}  // namespace

class GoogleDriveAuthTest : public QObject {
    Q_OBJECT

   private slots:
    void scope_policy() {
        QCOMPARE(file_scope, "https://www.googleapis.com/auth/drive.file");
        QCOMPARE(library_scope, "https://www.googleapis.com/auth/drive");
        QCOMPARE(requested_scope, file_scope);
        QVERIFY(permits_stored_scope(std::nullopt));
        QVERIFY(!permits_stored_scope(""));
        QVERIFY(permits_stored_scope(file_scope));
        QVERIFY(permits_stored_scope(library_scope));
        const auto mixed = "openid " + std::string(file_scope) + "\nemail";
        QVERIFY(permits_stored_scope(mixed));
        QVERIFY(!permits_stored_scope(std::string(file_scope) + ".metadata"));
        QVERIFY(!scope_includes(file_scope, library_scope));
        QCOMPARE(granted_scope(std::nullopt, requested_scope),
                 std::optional{std::string(file_scope)});
        QVERIFY(!granted_scope("", requested_scope));
        QVERIFY(!granted_scope("drive.readonly", requested_scope));
        QCOMPARE(granted_scope(mixed, requested_scope), std::optional{mixed});
        QCOMPARE(granted_scope(library_scope, requested_scope),
                 std::optional{std::string(library_scope)});
    }

    void credential_invalidation_policy() {
        constexpr auto revoked =
            R"({"error":"invalid_grant","error_description":"Token has been revoked."})";
        QVERIFY(invalidates_stored_credentials(400, revoked));
        QVERIFY(!invalidates_stored_credentials(400, R"({"error":"invalid_client"})"));
        QVERIFY(!invalidates_stored_credentials(400, "<html>Service unavailable</html>"));
        QVERIFY(!invalidates_stored_credentials(400, R"({"error":{"message":"invalid_grant"}})"));
        for (auto status : {200, 429, 503}) {
            QVERIFY(!invalidates_stored_credentials(status, revoked));
        }
        QVERIFY(!invalidates_stored_credentials(std::nullopt, revoked));
    }

    void validates_client_id_without_trimming() {
        QVERIFY(valid_client_id(client_id));
        QVERIFY(valid_client_id("1234567890-abc123.apps.googleusercontent.com"));
        for (auto bad :
             {"fixture.apps.googleusercontent.com", "123-Fixture.apps.googleusercontent.com",
              "123-.apps.googleusercontent.com", " 123-fixture.apps.googleusercontent.com",
              "123-fixture.apps.googleusercontent.com\n"}) {
            QVERIFY(!valid_client_id(bad));
        }
    }

    void resolves_trimmed_oauth_configuration() {
        AuthFixture fixture;
        QVERIFY(fixture.start());
        auto config = load_sync_oauth_config(fixture.settings());
        QCOMPARE(resolved_client_id(config), client_id);
        QCOMPARE(resolved_client_secret(config), std::optional{std::string("configured-secret")});
        QVERIFY(qputenv("NIRATAN_GDRIVE_CLIENT_ID", " 456-other.apps.googleusercontent.com "));
        QVERIFY(qputenv("NIRATAN_GDRIVE_CLIENT_SECRET", " override-secret "));
        QCOMPARE(resolved_client_id(config), "456-other.apps.googleusercontent.com");
        QCOMPARE(resolved_client_secret(config), std::optional{std::string("override-secret")});
        QVERIFY(qputenv("NIRATAN_GDRIVE_CLIENT_ID", " \t"));
        QVERIFY(qputenv("NIRATAN_GDRIVE_CLIENT_SECRET", ""));
        QCOMPARE(resolved_client_id(config), client_id);
        QCOMPARE(resolved_client_secret(config), std::optional{std::string("configured-secret")});
        QVERIFY(write(fixture.settings(), R"({"googleClientSecret":" "})"));
        QVERIFY(!resolved_client_secret(load_sync_oauth_config(fixture.settings())));
        QVERIFY(write(fixture.settings(), "{damaged"));
        QVERIFY(resolved_client_id(load_sync_oauth_config(fixture.settings())).empty());
        QVERIFY(!resolved_client_secret(load_sync_oauth_config(fixture.settings())));
        QCOMPARE(GoogleDriveAuth::Config{}.token_endpoint,
                 QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
    }

    void pkce_matches_rfc_vector() {
        const auto challenge =
            LoopbackAuthorization::code_challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");
        QCOMPARE(challenge, "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
        QCOMPARE(challenge.size(), 43U);
        QCOMPARE(challenge.find_first_of("+/="), std::string::npos);
    }

    void loopback_validates_callback_and_completes() {
        LoopbackResult result;
        QUrl authorization;
        LoopbackAuthorization loopback;
        const auto open = [&](const QUrl& url) {
            authorization = url;
            return true;
        };
        loopback.authorize(client_id, std::string(requested_scope), 10s, open, result.receive());
        QTRY_VERIFY_WITH_TIMEOUT(authorization.isValid(), 1000);
        QCOMPARE(authorization.adjusted(QUrl::RemoveQuery),
                 QUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth")));
        const QUrl uri = redirect(authorization);
        QCOMPARE(uri.scheme(), QStringLiteral("http"));
        QCOMPARE(uri.host(), QStringLiteral("127.0.0.1"));
        QCOMPARE(uri.path(), QStringLiteral("/"));
        QVERIFY(uri.port() > 0);
        const auto query = QUrlQuery(authorization);
        QCOMPARE(query.queryItemValue(QStringLiteral("client_id")), QString::fromLatin1(client_id));
        QCOMPARE(query.queryItemValue(QStringLiteral("response_type")), QStringLiteral("code"));
        QCOMPARE(query.queryItemValue(QStringLiteral("scope")),
                 QString::fromLatin1(file_scope.data(), static_cast<qsizetype>(file_scope.size())));
        QCOMPARE(query.queryItemValue(QStringLiteral("access_type")), QStringLiteral("offline"));
        QCOMPARE(query.queryItemValue(QStringLiteral("prompt")), QStringLiteral("consent"));
        QCOMPARE(query.queryItemValue(QStringLiteral("code_challenge_method")),
                 QStringLiteral("S256"));
        QVERIFY(!query.hasQueryItem(QStringLiteral("code_verifier")));
        const auto challenge = query.queryItemValue(QStringLiteral("code_challenge"));
        QCOMPARE(challenge.size(), 43);
        const auto session_state = state(authorization);
        QCOMPARE(session_state.size(), 43);
        const auto matching_state = "&state=" + session_state;
        struct Rejection {
            QByteArray target;
            QByteArray status;
            QByteArray method = "GET";
            QByteArray extra;
        };
        const std::vector<Rejection> rejected{
            {"/?code=fixture&state=wrong", "403", "GET", {}},
            {"/?error=access_denied&state=wrong", "403", "GET", {}},
            {"/favicon.ico?code=fixture" + matching_state, "400", "GET", {}},
            {"/?code=fixture" + matching_state + "#fragment", "400", "GET", {}},
            {"/?code=fixture" + matching_state + matching_state, "403", "GET", {}},
            {"/?code=one&code=two" + matching_state, "400", "GET", {}},
            {"/?code=fixture&error=access_denied" + matching_state, "400", "GET", {}},
            {"/?code=" + matching_state, "400", "GET", {}},
            {"/?code=fixture" + matching_state, "400", "POST", {}},
            {"/?code=fixture" + matching_state, "400", "GET", "Host: localhost\r\n"},
            {"/?code=fixture" + matching_state, "431", "GET",
             "X-Fill: " + QByteArray(100001, 'x') + "\r\n"},
        };
        for (const auto& test : rejected) {
            const auto reply = callback(uri, test.target, test.method, test.extra);
            QVERIFY2(reply.startsWith("HTTP/1.1 " + test.status), reply.constData());
            QVERIFY(reply.contains("This Google Drive callback could not be accepted."));
            QVERIFY(!reply.contains(session_state));
            QVERIFY(!reply.contains("code=fixture"));
            QVERIFY(loopback.is_authorizing());
            QCOMPARE(result.calls, 0);
        }
        QCOMPARE(callback(uri, "/?code=fixture&state=wrong"),
                 callback_page("403 Forbidden", false));
        QCOMPARE(callback(uri, "/?code=fixture-code" + matching_state, "GET",
                          "X-Fill: " + QByteArray(95000, 'x') + "\r\n"),
                 callback_page("200 OK", true));
        QTRY_COMPARE(result.calls, 1);
        QVERIFY(result.code.has_value());
        QVERIFY(!result.error);
        QCOMPARE(result.code->code, "fixture-code");
        QCOMPARE(result.code->redirect_uri, uri.toString().toStdString());
        QCOMPARE(result.code->code_verifier.size(), 43U);
        QCOMPARE(LoopbackAuthorization::code_challenge(result.code->code_verifier),
                 challenge.toStdString());
        QVERIFY(!loopback.is_authorizing());
        QVERIFY(port_closed(uri));

        authorization.clear();
        loopback.authorize(client_id, std::string(requested_scope), 10s, open, result.receive());
        QTRY_VERIFY_WITH_TIMEOUT(authorization.isValid(), 1000);
        QVERIFY(state(authorization) != session_state);
        QVERIFY(query_value(authorization, QStringLiteral("code_challenge")) != challenge);
        loopback.cancel();
    }

    void loopback_failure_modes_data() {
        QTest::addColumn<int>("mode");
        QTest::newRow("denial") << 0;
        QTest::newRow("cancel-twice") << 1;
        QTest::newRow("timeout") << 2;
        QTest::newRow("browser-failure") << 3;
        QTest::newRow("already-in-progress") << 4;
    }

    void loopback_failure_modes() {
        QFETCH(int, mode);
        LoopbackResult result;
        LoopbackResult second;
        QUrl authorization;
        int openings = 0;
        LoopbackAuthorization loopback;
        const auto open = [&](const QUrl& url) {
            ++openings;
            authorization = url;
            return mode != 3;
        };
        loopback.authorize(client_id, std::string(requested_scope), mode == 2 ? 100ms : 10s, open,
                           result.receive());
        QTRY_VERIFY_WITH_TIMEOUT(authorization.isValid(), 1000);
        const auto uri = redirect(authorization);
        LoopbackErrorKind expected = LoopbackErrorKind::cancelled;
        switch (mode) {
            case 0:
                QCOMPARE(callback(uri, "/?error=access_denied&state=" + state(authorization)),
                         callback_page("200 OK", false));
                expected = LoopbackErrorKind::authorization_denied;
                break;
            case 1:
                loopback.cancel();
                loopback.cancel();
                break;
            case 2:
                expected = LoopbackErrorKind::timed_out;
                break;
            case 3:
                expected = LoopbackErrorKind::browser_unavailable;
                break;
            case 4:
                loopback.authorize(client_id, std::string(requested_scope), 10s, open,
                                   second.receive());
                QTRY_COMPARE(second.calls, 1);
                QCOMPARE(second.error, std::optional{LoopbackErrorKind::already_in_progress});
                QCOMPARE(openings, 1);
                QCOMPARE(result.calls, 0);
                QVERIFY(loopback.is_authorizing());
                loopback.cancel();
                break;
        }
        QTRY_COMPARE(result.calls, 1);
        QVERIFY(!result.code);
        QCOMPARE(result.error, std::optional{expected});
        QVERIFY(!loopback.is_authorizing());
        QVERIFY(port_closed(uri));
    }

    void loopback_descriptions() {
        using enum LoopbackErrorKind;
        QCOMPARE(loopback_error_description(already_in_progress),
                 "Google Drive sign-in is already in progress.");
        QCOMPARE(loopback_error_description(listener_unavailable),
                 "Could not start Google Drive sign-in. Please try again.");
        QCOMPARE(loopback_error_description(browser_unavailable),
                 "Could not open the browser for Google Drive sign-in.");
        QCOMPARE(loopback_error_description(timed_out),
                 "Google Drive sign-in timed out. Please try again.");
        QCOMPARE(loopback_error_description(authorization_denied),
                 "Google Drive sign-in was not completed. Please try again.");
    }

    void credentials_decode_and_replace_damaged_file() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto path = std::filesystem::path(root.filePath("credentials.json").toStdString());
        CredentialsStore store(path);
        QVERIFY(write(
            path,
            R"({"accessToken":"SYNTHETIC-OLD-ACCESS","refreshToken":"SYNTHETIC-OLD-REFRESH","clientId":"123-fixture.apps.googleusercontent.com"})"));
        const auto legacy = store.load();
        QVERIFY(legacy.has_value());
        QCOMPARE(legacy->access_token, "SYNTHETIC-OLD-ACCESS");
        QCOMPARE(legacy->refresh_token, "SYNTHETIC-OLD-REFRESH");
        QCOMPARE(legacy->client_id, client_id);
        QVERIFY(!legacy->client_secret);
        QVERIFY(store.save(*legacy));
        QVERIFY(!QJsonDocument::fromJson(read(path)).object().contains("clientSecret"));

        QVERIFY(write(path, R"({"accessToken":"a","clientId":"c"})"));
        QVERIFY(!store.load());
        QVERIFY(write(path, "{damaged"));
        QVERIFY(!store.load());
        const GoogleDriveCredentials current{"a", "r", "c", "s"};
        QVERIFY(store.save(current));
        QCOMPARE(store.load(), std::optional{current});
        QCOMPARE(
            QJsonDocument::fromJson(read(path)),
            QJsonDocument::fromJson(
                R"({"accessToken":"a","refreshToken":"r","clientId":"c","clientSecret":"s"})"));
        QCOMPARE(std::filesystem::status(path).permissions(), owner_permissions);
    }

    void failed_credential_write_keeps_previous_file() {
        AuthFixture fixture;
        QVERIFY(fixture.start());
        QVERIFY(seed_credentials(fixture));
        CredentialsStore store(fixture.credentials());
        const auto previous = store.load();
        QVERIFY(previous.has_value());
        const LockedDirectory lock(fixture.credentials().parent_path());
        QVERIFY(!store.save({"new-a", "new-r", client_id, {}}));
        QCOMPARE(read(fixture.credentials()), QByteArray(credentials_json));
        QCOMPARE(store.load(), previous);
    }

    void credentials_are_cached_and_client_must_match() {
        AuthFixture fixture;
        QVERIFY(fixture.start());
        GoogleDriveAuth auth(fixture.settings(), fixture.credentials(), fixture.loopback,
                             fixture.config());
        std::string error;
        QVERIFY(!auth.access_token(&error));
        QCOMPARE(error, "Not authenticated\nPlease sign in");
        QVERIFY(!auth.is_authenticated());
        QVERIFY(seed_credentials(fixture));
        QCOMPARE(auth.access_token(), std::optional{std::string("SYNTHETIC-OLD-ACCESS")});
        QVERIFY(auth.is_authenticated());
        QVERIFY(std::filesystem::remove(fixture.credentials()));
        QCOMPARE(auth.access_token(), std::optional{std::string("SYNTHETIC-OLD-ACCESS")});
        QVERIFY(qputenv("NIRATAN_GDRIVE_CLIENT_ID", "456-other.apps.googleusercontent.com"));
        QVERIFY(!auth.is_authenticated());
        QVERIFY(qputenv("NIRATAN_GDRIVE_CLIENT_ID", "invalid-id"));
        QVERIFY(!auth.is_authenticated());
    }

    void exchange_outcomes_data() {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("failure");
        QTest::newRow("granted") << 200 << QByteArray(exchange_json) << -1;
        QTest::newRow("omitted-scope")
            << 201
            << QByteArray(
                   R"({"access_token":"SYNTHETIC-NEW-ACCESS","refresh_token":"SYNTHETIC-NEW-REFRESH"})")
            << -1;
        QTest::newRow("wrong-scope")
            << 200
            << QByteArray(R"({"access_token":"a","refresh_token":"r","scope":"drive.readonly"})")
            << static_cast<int>(AuthErrorKind::missing_library_access);
        QTest::newRow("no-refresh-token") << 200 << QByteArray(R"({"access_token":"a"})")
                                          << static_cast<int>(AuthErrorKind::missing_refresh_token);
        QTest::newRow("rejected") << 400 << QByteArray("{}")
                                  << static_cast<int>(AuthErrorKind::token_exchange_failed);
        QTest::newRow("no-access-token") << 200 << QByteArray(R"({"refresh_token":"r"})")
                                         << static_cast<int>(AuthErrorKind::invalid_response);
    }

    void exchange_outcomes() {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(int, failure);
        AuthFixture fixture;
        QVERIFY(fixture.start());
        fixture.endpoint.enqueue({status, body});
        const auto config = fixture.config();
        QCOMPARE(config.timeout.count(), 10000);
        AuthResult result;
        GoogleDriveAuth auth(fixture.settings(), fixture.credentials(), fixture.loopback, config);
        auth.authenticate(result.receive());
        QTRY_VERIFY_WITH_TIMEOUT(fixture.authorization.isValid(), 1000);
        const auto uri = redirect(fixture.authorization);
        QVERIFY(callback(uri, "/?code=fixture-code&state=" + state(fixture.authorization))
                    .startsWith("HTTP/1.1 200"));
        QTRY_VERIFY_WITH_TIMEOUT(result.finished, 1000);
        QCOMPARE(fixture.endpoint.requests().size(), 1);
        const auto& request = fixture.endpoint.requests().front();
        QCOMPARE(request.method, "POST");
        QCOMPARE(request.path, QStringLiteral("/token"));
        QCOMPARE(request.headers.value("content-type"), "application/x-www-form-urlencoded");
        const auto form = [&](const QString& key) {
            return request.form.queryItemValue(key, QUrl::FullyDecoded);
        };
        QCOMPARE(form("code"), QStringLiteral("fixture-code"));
        QCOMPARE(form("client_id"), QString::fromLatin1(client_id));
        QCOMPARE(form("redirect_uri"), uri.toString());
        QCOMPARE(form("grant_type"), QStringLiteral("authorization_code"));
        QCOMPARE(form("client_secret"), QStringLiteral("configured-secret"));
        QCOMPARE(LoopbackAuthorization::code_challenge(form("code_verifier").toStdString()),
                 query_value(fixture.authorization, "code_challenge").toStdString());
        if (failure < 0) {
            QVERIFY(!result.error);
            const GoogleDriveCredentials expected{"SYNTHETIC-NEW-ACCESS", "SYNTHETIC-NEW-REFRESH",
                                                  client_id, "configured-secret"};
            QCOMPARE(CredentialsStore(fixture.credentials()).load(), std::optional{expected});
            QCOMPARE(auth.access_token(), std::optional{expected.access_token});
            QVERIFY(auth.is_authenticated());
        } else {
            QVERIFY(result.error);
            QCOMPARE(result.error->kind, static_cast<AuthErrorKind>(failure));
            if (status == 400) {
                QCOMPARE(result.error->status_code, 400);
            }
            QVERIFY(!std::filesystem::exists(fixture.credentials()));
            QVERIFY(!auth.is_authenticated());
        }
    }

    void refresh_outcomes_data() {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("failure");
        QTest::newRow("no-access-token") << 200 << QByteArray(R"({"refresh_token":"r"})")
                                         << static_cast<int>(AuthErrorKind::invalid_response);
        QTest::newRow("success") << 200
                                 << QByteArray(R"({"access_token":"SYNTHETIC-REFRESHED-ACCESS"})")
                                 << -1;
        QTest::newRow("wrong-scope")
            << 200 << QByteArray(R"({"access_token":"a","scope":"drive.readonly"})")
            << static_cast<int>(AuthErrorKind::missing_library_access);
        QTest::newRow("revoked") << 400 << QByteArray(R"({"error":"invalid_grant"})")
                                 << static_cast<int>(AuthErrorKind::token_refresh_failed);
        QTest::newRow("bad-client") << 400 << QByteArray(R"({"error":"invalid_client"})")
                                    << static_cast<int>(AuthErrorKind::token_refresh_unavailable);
        QTest::newRow("unavailable") << 503 << QByteArray("{}")
                                     << static_cast<int>(AuthErrorKind::token_refresh_unavailable);
        QTest::newRow("not-exactly-200")
            << 201 << QByteArray(R"({"access_token":"a"})")
            << static_cast<int>(AuthErrorKind::token_refresh_unavailable);
    }

    void refresh_outcomes() {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(int, failure);
        AuthFixture fixture;
        QVERIFY(fixture.start());
        QVERIFY(seed_credentials(fixture));
        fixture.endpoint.enqueue({status, body});
        RefreshResult result;
        GoogleDriveAuth auth(fixture.settings(), fixture.credentials(), fixture.loopback,
                             fixture.config());
        auth.refresh(result.receive_refresh());
        QTRY_VERIFY_WITH_TIMEOUT(result.finished, 1000);
        QCOMPARE(fixture.endpoint.requests().size(), 1);
        const auto& request = fixture.endpoint.requests().front();
        QCOMPARE(request.method, "POST");
        QCOMPARE(request.path, QStringLiteral("/token"));
        QCOMPARE(request.headers.value("content-type"), "application/x-www-form-urlencoded");
        QCOMPARE(request.form.queryItemValue("client_id"), QString::fromLatin1(client_id));
        QCOMPARE(request.form.queryItemValue("grant_type"), QStringLiteral("refresh_token"));
        QCOMPARE(request.form.queryItemValue("refresh_token"),
                 QStringLiteral("SYNTHETIC-OLD-REFRESH"));
        QCOMPARE(request.form.queryItemValue("client_secret"), QStringLiteral("saved-secret"));
        if (failure < 0) {
            QVERIFY(!result.error);
            const GoogleDriveCredentials credentials{
                "SYNTHETIC-REFRESHED-ACCESS", "SYNTHETIC-OLD-REFRESH", client_id, "saved-secret"};
            QCOMPARE(result.token, std::optional{credentials.access_token});
            QCOMPARE(auth.access_token(), result.token);
            QCOMPARE(CredentialsStore(fixture.credentials()).load(), std::optional{credentials});
        } else {
            QVERIFY(!result.token);
            QVERIFY(result.error);
            const auto expected = static_cast<AuthErrorKind>(failure);
            QCOMPARE(result.error->kind, expected);
            if (expected == AuthErrorKind::token_refresh_failed) {
                QVERIFY(!std::filesystem::exists(fixture.credentials()));
                QVERIFY(!auth.access_token());
            } else {
                QCOMPARE(read(fixture.credentials()), QByteArray(credentials_json));
                QCOMPARE(auth.access_token(), std::optional{std::string("SYNTHETIC-OLD-ACCESS")});
            }
        }
    }

    void failed_token_save_preserves_file_and_cache_data() {
        QTest::addColumn<bool>("exchange");
        QTest::newRow("exchange") << true;
        QTest::newRow("refresh") << false;
    }

    void legacy_refresh_uses_matching_configured_secret() {
        AuthFixture fixture;
        QVERIFY(fixture.start());
        QVERIFY(std::filesystem::create_directories(fixture.credentials().parent_path()));
        QVERIFY(write(
            fixture.credentials(),
            R"({"accessToken":"a","refreshToken":"r","clientId":"123-fixture.apps.googleusercontent.com"})"));
        fixture.endpoint.enqueue({200, R"({"access_token":"renewed"})"});
        RefreshResult result;
        GoogleDriveAuth auth(fixture.settings(), fixture.credentials(), fixture.loopback,
                             fixture.config());
        auth.refresh(result.receive_refresh());
        QTRY_VERIFY_WITH_TIMEOUT(result.finished, 1000);
        QVERIFY(!result.error);
        QCOMPARE(result.token, std::optional{std::string("renewed")});
        QCOMPARE(fixture.endpoint.requests().size(), 1);
        QCOMPARE(fixture.endpoint.requests().front().form.queryItemValue("client_secret"),
                 QStringLiteral("configured-secret"));
        const auto stored = CredentialsStore(fixture.credentials()).load();
        QVERIFY(stored);
        QVERIFY(!stored->client_secret);
    }

    void failed_token_save_preserves_file_and_cache() {
        QFETCH(bool, exchange);
        AuthFixture fixture;
        QVERIFY(fixture.start());
        QVERIFY(seed_credentials(fixture));
        fixture.endpoint.enqueue({200, exchange_json});
        RefreshResult result;
        GoogleDriveAuth auth(fixture.settings(), fixture.credentials(), fixture.loopback,
                             fixture.config());
        const auto previous = auth.access_token();
        QVERIFY(previous.has_value());
        const LockedDirectory lock(fixture.credentials().parent_path());
        if (exchange) {
            auth.authenticate(result.receive());
            QTRY_VERIFY_WITH_TIMEOUT(fixture.authorization.isValid(), 1000);
            QVERIFY(callback(redirect(fixture.authorization),
                             "/?code=fixture-code&state=" + state(fixture.authorization))
                        .startsWith("HTTP/1.1 200"));
        } else {
            auth.refresh(result.receive_refresh());
        }
        QTRY_VERIFY_WITH_TIMEOUT(result.finished, 1000);
        QVERIFY(result.error);
        QCOMPARE(result.error->kind, AuthErrorKind::credentials_save_failed);
        QVERIFY(!result.token);
        QCOMPARE(read(fixture.credentials()), QByteArray(credentials_json));
        QCOMPARE(auth.access_token(), previous);
    }

    void cancel_during_held_exchange_keeps_previous_connection() {
        AuthFixture fixture;
        QVERIFY(fixture.start());
        QVERIFY(seed_credentials(fixture));
        fixture.endpoint.enqueue({200, exchange_json, "application/json", true});
        AuthResult result;
        GoogleDriveAuth auth(fixture.settings(), fixture.credentials(), fixture.loopback,
                             fixture.config());
        const auto previous = auth.access_token();
        QVERIFY(previous.has_value());
        auth.authenticate(result.receive());
        QTRY_VERIFY_WITH_TIMEOUT(fixture.authorization.isValid(), 1000);
        QVERIFY(callback(redirect(fixture.authorization),
                         "/?code=fixture-code&state=" + state(fixture.authorization))
                    .startsWith("HTTP/1.1 200"));
        QTRY_COMPARE(fixture.endpoint.requests().size(), 1);
        QVERIFY(!result.finished);
        auth.cancel_authentication();
        fixture.endpoint.release();
        QTRY_VERIFY_WITH_TIMEOUT(result.finished, 1000);
        QVERIFY(result.error);
        QCOMPARE(result.error->kind, AuthErrorKind::cancelled);
        QCOMPARE(read(fixture.credentials()), QByteArray(credentials_json));
        QCOMPARE(auth.access_token(), previous);
    }

    void authorization_failure_before_exchange_data() {
        QTest::addColumn<int>("mode");
        QTest::newRow("invalid-client-id") << 0;
        QTest::newRow("browser-failure") << 1;
        QTest::newRow("timeout") << 2;
        QTest::newRow("denial") << 3;
        QTest::newRow("overlap-and-cancel") << 4;
        QTest::newRow("no-browser-opener") << 5;
    }

    void authorization_failure_before_exchange() {
        QFETCH(int, mode);
        AuthFixture fixture;
        QVERIFY(fixture.start());
        fixture.browser_available = mode != 1;
        auto config = fixture.config();
        if (mode == 0) {
            QVERIFY(qputenv("NIRATAN_GDRIVE_CLIENT_ID", "invalid"));
        } else if (mode == 2) {
            config.loopback_timeout = 100ms;
        } else if (mode == 5) {
            config.browser_opener = {};
        }
        AuthResult result;
        AuthResult second;
        GoogleDriveAuth auth(fixture.settings(), fixture.credentials(), fixture.loopback, config);
        auth.authenticate(result.receive());
        auto expected = AuthErrorKind::invalid_client_id;
        if (mode != 0 && mode != 5) {
            QTRY_VERIFY_WITH_TIMEOUT(fixture.authorization.isValid(), 1000);
        }
        switch (mode) {
            case 1:
            case 5:
                expected = AuthErrorKind::browser_unavailable;
                break;
            case 2:
                expected = AuthErrorKind::timed_out;
                break;
            case 3:
                expected = AuthErrorKind::authorization_denied;
                QVERIFY(callback(redirect(fixture.authorization),
                                 "/?error=access_denied&state=" + state(fixture.authorization))
                            .startsWith("HTTP/1.1 200"));
                break;
            case 4:
                auth.authenticate(second.receive());
                QTRY_VERIFY_WITH_TIMEOUT(second.finished, 1000);
                QVERIFY(second.error);
                QCOMPARE(second.error->kind, AuthErrorKind::already_in_progress);
                QVERIFY(!result.finished);
                auth.cancel_authentication();
                expected = AuthErrorKind::cancelled;
                break;
        }
        QTRY_VERIFY_WITH_TIMEOUT(result.finished, 1000);
        QVERIFY(result.error);
        QCOMPARE(result.error->kind, expected);
        QCOMPARE(fixture.browser_calls, mode == 0 || mode == 5 ? 0 : 1);
        QVERIFY(!fixture.loopback.is_authorizing());
        if (mode != 0 && mode != 5) {
            QVERIFY(port_closed(redirect(fixture.authorization)));
        }
        QVERIFY(fixture.endpoint.requests().empty());
        QVERIFY(!std::filesystem::exists(fixture.credentials()));
    }

    void auth_descriptions() {
        using enum AuthErrorKind;
        QCOMPARE(AuthError{already_in_progress}.description(),
                 "Google Drive sign-in is already in progress.");
        QCOMPARE(AuthError{invalid_client_id}.description(), "Invalid Client ID format");
        QCOMPARE((AuthError{token_exchange_failed, 400}).description(),
                 "Token exchange failed: 400");
        QCOMPARE(AuthError{not_authenticated}.description(), "Not authenticated\nPlease sign in");
        QCOMPARE(AuthError{invalid_response}.description(), "Invalid token response");
        QCOMPARE(AuthError{token_refresh_failed}.description(),
                 "Failed to refresh token\nPlease sign in again");
        QCOMPARE(AuthError{token_refresh_unavailable}.description(),
                 "Google Drive could not refresh authorization. Your saved connection was kept. "
                 "Please try again.");
        QCOMPARE(AuthError{missing_refresh_token}.description(),
                 "Google did not return a refresh token\nPlease try connecting again");
        QCOMPARE(AuthError{missing_library_access}.description(),
                 "Google Drive file access was not granted. Reconnect and allow access to sync.");
        QCOMPARE(AuthError{credentials_save_failed}.description(),
                 "Could not save Google Drive authorization. Please try connecting again.");
        QCOMPARE(AuthError{listener_unavailable}.description(),
                 "Could not start Google Drive sign-in. Please try again.");
        QCOMPARE(AuthError{browser_unavailable}.description(),
                 "Could not open the browser for Google Drive sign-in.");
        QCOMPARE(AuthError{timed_out}.description(),
                 "Google Drive sign-in timed out. Please try again.");
        QCOMPARE(AuthError{authorization_denied}.description(),
                 "Google Drive sign-in was not completed. Please try again.");
    }

    void sign_out_clears_file_and_cache() {
        AuthFixture fixture;
        QVERIFY(fixture.start());
        QVERIFY(seed_credentials(fixture));
        GoogleDriveAuth auth(fixture.settings(), fixture.credentials(), fixture.loopback,
                             fixture.config());
        QVERIFY(auth.is_authenticated());
        QVERIFY(auth.access_token());
        auth.sign_out();
        QVERIFY(!std::filesystem::exists(fixture.credentials()));
        QVERIFY(!auth.is_authenticated());
        std::string error;
        QVERIFY(!auth.access_token(&error));
        QCOMPARE(error, "Not authenticated\nPlease sign in");
    }
};

QTEST_GUILESS_MAIN(GoogleDriveAuthTest)
#include "tst_google_drive_auth.moc"

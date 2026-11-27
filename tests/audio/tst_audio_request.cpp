// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/audio/audio_request.h"

#include <QTest>

#include <optional>
#include <string>

using niratan::audio::AudioRequest;
using niratan::audio::parse_audio_request;

// Features/Popup/PopupWebView.swift (AudioHandler) has no test script; the
// cases follow its query parsing and timeout choice.
class AudioRequestTest : public QObject {
    Q_OBJECT

   private slots:
    void localhost_gets_the_longer_timeout();
    void other_hosts_get_the_short_timeout();
    void the_target_is_percent_decoded_once();
    void the_first_url_item_is_used();
    void missing_or_unusable_targets_are_rejected();
};

void AudioRequestTest::localhost_gets_the_longer_timeout() {
    const auto request = parse_audio_request(
        "audio://?url=http%3A%2F%2Flocalhost%3A18765%2Flocalaudio%2Fget%2F%3Fterm%3Dx");

    QVERIFY(request.has_value());
    QCOMPARE(request->url, std::string("http://localhost:18765/localaudio/get/?term=x"));
    QCOMPARE(request->timeout_ms, 5000);
}

void AudioRequestTest::other_hosts_get_the_short_timeout() {
    const auto local = parse_audio_request(
        "audio://?url=http%3A%2F%2F127.0.0.1%3A8765%2Faudio");
    QVERIFY(local.has_value());
    QCOMPARE(local->timeout_ms, 4000);

    const auto remote = parse_audio_request(
        "audio://?url=https%3A%2F%2Fexample.test%2F%3Fterm%3D%7Bterm%7D");
    QVERIFY(remote.has_value());
    QCOMPARE(remote->timeout_ms, 4000);
}

void AudioRequestTest::the_target_is_percent_decoded_once() {
    const auto request = parse_audio_request(
        "audio://?url=https%3A%2F%2Fexample.test%2F%3Fterm%3D%7Bterm%7D%26reading%3D%7Breading%7D");

    QVERIFY(request.has_value());
    QCOMPARE(request->url,
             std::string("https://example.test/?term={term}&reading={reading}"));
}

void AudioRequestTest::the_first_url_item_is_used() {
    const auto request = parse_audio_request(
        "audio://?url=http%3A%2F%2Flocalhost%2Fa&url=http%3A%2F%2Flocalhost%2Fb");

    QVERIFY(request.has_value());
    QCOMPARE(request->url, std::string("http://localhost/a"));
}

void AudioRequestTest::missing_or_unusable_targets_are_rejected() {
    QVERIFY(!parse_audio_request("audio://").has_value());
    QVERIFY(!parse_audio_request("audio://?other=x").has_value());
    QVERIFY(!parse_audio_request("audio://?url=").has_value());
    QVERIFY(!parse_audio_request("audio://?url=http%3A%2F%2F%5Bbad").has_value());
}

QTEST_APPLESS_MAIN(AudioRequestTest)
#include "tst_audio_request.moc"

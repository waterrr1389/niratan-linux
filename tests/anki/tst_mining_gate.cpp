// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/mining_gate.h"

#include <QtTest>

#include <string>
#include <string_view>

using niratan::anki::duplicate_expression;
using niratan::anki::DuplicateLookupResult;
using niratan::anki::FieldMappings;
using niratan::anki::gate_anki_mining;
using niratan::anki::MiningContext;
using niratan::anki::MiningResult;
using niratan::anki::MiningSettings;
using niratan::anki::MiningStatus;
using niratan::anki::needs_sasayaki_audio;
using niratan::anki::needs_video_audio_clip;
using niratan::anki::needs_video_screenshot;
using niratan::anki::Strings;
using niratan::anki::VideoMiningContext;

namespace {

MiningSettings configured() {
    MiningSettings settings;
    settings.selected_deck = "Mining";
    settings.selected_note_type = "Lapis";
    return settings;
}

MiningContext with_video(const VideoMiningContext& video = {}) {
    MiningContext context;
    context.video = video;
    return context;
}

// Answers the gate's duplicate lookup with a fixed result and records how
// often and for which expression it was asked.
struct LookupSpy {
    DuplicateLookupResult result;
    int calls = 0;
    std::string expression;

    niratan::anki::DuplicateLookup as_function() {
        return [this](std::string_view word) {
            ++calls;
            expression = std::string(word);
            return result;
        };
    }
};

const std::string kConfigureMessage = "Configure Anki deck and model first.";
const std::string kDuplicateMessage = "Already exists in Anki.";
const std::string kAudioClipMessage = "Unable to capture the subtitle audio clip.";
const std::string kScreenshotMessage = "Unable to capture the video screenshot.";

// A lookup that reports no duplicate, for cases about the media checks.
niratan::anki::DuplicateLookup clean_lookup() {
    return [](std::string_view) { return DuplicateLookupResult{}; };
}

}  // namespace

// Features/Popup/AnkiMining.swift (preflightAnkiMining, mineAnkiEntry) has no
// test script of its own, so the cases follow its branches directly.
class MiningGateTest : public QObject {
    Q_OBJECT

   private slots:
    void missing_deck_fails_before_the_lookup();
    void missing_note_type_fails_before_the_lookup();
    void an_empty_name_counts_as_selected();
    void allow_dupes_skips_the_lookup();
    void the_lookup_searches_the_expression_field();
    void a_missing_expression_searches_for_Entry();
    void an_empty_expression_is_not_replaced();
    void a_duplicate_reports_the_first_note_id();
    void a_duplicate_without_note_ids_reports_none();
    void a_clean_lookup_passes();
    void a_passed_preflight_skips_the_whole_lookup();
    void a_missing_audio_clip_fails();
    void the_audio_clip_error_message_wins();
    void a_named_audio_clip_passes();
    void a_pathed_audio_clip_passes();
    void a_missing_screenshot_fails();
    void the_screenshot_error_message_wins();
    void a_named_screenshot_passes();
    void a_pathed_screenshot_passes();
    void media_checks_need_their_flags();
    void a_missing_video_skips_the_media_checks();
    void the_preflight_runs_before_the_media_checks();
    void media_checks_run_after_a_passed_preflight();
    void the_audio_clip_runs_before_the_screenshot();
    void screenshot_needs_an_exact_handlebar_value();
    void audio_clip_needs_an_exact_handlebar_value();
    void sasayaki_audio_needs_an_exact_handlebar_value();
};

void MiningGateTest::missing_deck_fails_before_the_lookup() {
    MiningSettings settings = configured();
    settings.selected_deck.reset();
    LookupSpy spy;
    spy.result = {true, {7}};

    const auto result = gate_anki_mining(settings, {}, {}, spy.as_function());

    QVERIFY(result.has_value());
    QCOMPARE(result->status, MiningStatus::failed);
    QCOMPARE(result->message, kConfigureMessage);
    QCOMPARE(result->note_id, std::optional<std::int64_t>{});
    QCOMPARE(spy.calls, 0);
}

void MiningGateTest::missing_note_type_fails_before_the_lookup() {
    MiningSettings settings = configured();
    settings.selected_note_type.reset();
    LookupSpy spy;
    spy.result = {true, {7}};

    const auto result = gate_anki_mining(settings, {}, {}, spy.as_function());

    QVERIFY(result.has_value());
    QCOMPARE(result->status, MiningStatus::failed);
    QCOMPARE(result->message, kConfigureMessage);
    QCOMPARE(spy.calls, 0);
}

void MiningGateTest::an_empty_name_counts_as_selected() {
    MiningSettings settings;
    settings.selected_deck = "";
    settings.selected_note_type = "";
    LookupSpy spy;

    const auto result = gate_anki_mining(settings, {}, {}, spy.as_function());

    QVERIFY(!result.has_value());
    QCOMPARE(spy.calls, 1);
}

void MiningGateTest::allow_dupes_skips_the_lookup() {
    MiningSettings settings = configured();
    settings.allow_dupes = true;
    LookupSpy spy;
    spy.result = {true, {7}};

    const auto result = gate_anki_mining(settings, {}, {}, spy.as_function());

    QVERIFY(!result.has_value());
    QCOMPARE(spy.calls, 0);
}

void MiningGateTest::the_lookup_searches_the_expression_field() {
    MiningSettings settings = configured();
    const Strings content = {{"expression", "決定"}, {"reading", "けってい"}};
    LookupSpy spy;

    gate_anki_mining(settings, content, {}, spy.as_function());

    QCOMPARE(spy.calls, 1);
    QCOMPARE(spy.expression, std::string("決定"));
}

void MiningGateTest::a_missing_expression_searches_for_Entry() {
    const Strings content = {{"reading", "けってい"}};

    QCOMPARE(duplicate_expression(content), std::string("Entry"));
}

void MiningGateTest::an_empty_expression_is_not_replaced() {
    const Strings content = {{"expression", ""}};
    LookupSpy spy;

    gate_anki_mining(configured(), content, {}, spy.as_function());

    QCOMPARE(spy.expression, std::string());
}

void MiningGateTest::a_duplicate_reports_the_first_note_id() {
    LookupSpy spy;
    spy.result = {true, {42, 43}};

    const auto result = gate_anki_mining(configured(), {}, {}, spy.as_function());

    QVERIFY(result.has_value());
    QCOMPARE(result->status, MiningStatus::duplicate);
    QCOMPARE(result->message, kDuplicateMessage);
    QCOMPARE(result->note_id, std::optional<std::int64_t>{42});
}

void MiningGateTest::a_duplicate_without_note_ids_reports_none() {
    LookupSpy spy;
    spy.result = {true, {}};

    const auto result = gate_anki_mining(configured(), {}, {}, spy.as_function());

    QVERIFY(result.has_value());
    QCOMPARE(result->status, MiningStatus::duplicate);
    QCOMPARE(result->note_id, std::optional<std::int64_t>{});
}

void MiningGateTest::a_clean_lookup_passes() {
    LookupSpy spy;
    spy.result = {false, {}};

    const auto result = gate_anki_mining(configured(), {}, {}, spy.as_function());

    QVERIFY(!result.has_value());
    QCOMPARE(spy.calls, 1);
}

void MiningGateTest::a_passed_preflight_skips_the_whole_lookup() {
    MiningSettings settings;
    settings.allow_dupes = false;
    LookupSpy spy;
    spy.result = {true, {7}};

    const auto result = gate_anki_mining(settings, {}, {}, spy.as_function(), true);

    QVERIFY(!result.has_value());
    QCOMPARE(spy.calls, 0);
}

void MiningGateTest::a_missing_audio_clip_fails() {
    MiningSettings settings = configured();
    settings.needs_video_audio_clip = true;

    const auto result = gate_anki_mining(settings, {}, with_video(), clean_lookup());

    QVERIFY(result.has_value());
    QCOMPARE(result->status, MiningStatus::failed);
    QCOMPARE(result->message, kAudioClipMessage);
    QCOMPARE(result->note_id, std::optional<std::int64_t>{});
}

void MiningGateTest::the_audio_clip_error_message_wins() {
    MiningSettings settings = configured();
    settings.needs_video_audio_clip = true;
    VideoMiningContext video;
    video.audio_clip_error_message = "The clip was not written.";

    const auto result = gate_anki_mining(settings, {}, with_video(video), clean_lookup());

    QVERIFY(result.has_value());
    QCOMPARE(result->message, std::string("The clip was not written."));
}

void MiningGateTest::a_named_audio_clip_passes() {
    MiningSettings settings = configured();
    settings.needs_video_audio_clip = true;
    VideoMiningContext video;
    video.audio_clip_filename = "clip.m4a";

    QVERIFY(!gate_anki_mining(settings, {}, with_video(video), clean_lookup()).has_value());
}

void MiningGateTest::a_pathed_audio_clip_passes() {
    MiningSettings settings = configured();
    settings.needs_video_audio_clip = true;
    VideoMiningContext video;
    video.audio_clip_path = "/tmp/clip.m4a";

    QVERIFY(!gate_anki_mining(settings, {}, with_video(video), clean_lookup()).has_value());
}

void MiningGateTest::a_missing_screenshot_fails() {
    MiningSettings settings = configured();
    settings.needs_video_screenshot = true;

    const auto result = gate_anki_mining(settings, {}, with_video(), clean_lookup());

    QVERIFY(result.has_value());
    QCOMPARE(result->status, MiningStatus::failed);
    QCOMPARE(result->message, kScreenshotMessage);
}

void MiningGateTest::the_screenshot_error_message_wins() {
    MiningSettings settings = configured();
    settings.needs_video_screenshot = true;
    VideoMiningContext video;
    video.screenshot_error_message = "The frame was not written.";

    const auto result = gate_anki_mining(settings, {}, with_video(video), clean_lookup());

    QVERIFY(result.has_value());
    QCOMPARE(result->message, std::string("The frame was not written."));
}

void MiningGateTest::a_named_screenshot_passes() {
    MiningSettings settings = configured();
    settings.needs_video_screenshot = true;
    VideoMiningContext video;
    video.screenshot_filename = "frame.png";

    QVERIFY(!gate_anki_mining(settings, {}, with_video(video), clean_lookup()).has_value());
}

void MiningGateTest::a_pathed_screenshot_passes() {
    MiningSettings settings = configured();
    settings.needs_video_screenshot = true;
    VideoMiningContext video;
    video.screenshot_path = "/tmp/frame.png";

    QVERIFY(!gate_anki_mining(settings, {}, with_video(video), clean_lookup()).has_value());
}

void MiningGateTest::media_checks_need_their_flags() {
    const auto result = gate_anki_mining(configured(), {}, with_video(), clean_lookup());

    QVERIFY(!result.has_value());
}

void MiningGateTest::a_missing_video_skips_the_media_checks() {
    MiningSettings settings = configured();
    settings.needs_video_audio_clip = true;
    settings.needs_video_screenshot = true;

    const auto result = gate_anki_mining(settings, {}, {}, clean_lookup());

    QVERIFY(!result.has_value());
}

void MiningGateTest::the_preflight_runs_before_the_media_checks() {
    MiningSettings settings;
    settings.needs_video_audio_clip = true;

    const auto result = gate_anki_mining(settings, {}, with_video(), {});

    QVERIFY(result.has_value());
    QCOMPARE(result->message, kConfigureMessage);
}

void MiningGateTest::media_checks_run_after_a_passed_preflight() {
    MiningSettings settings;
    settings.needs_video_audio_clip = true;

    const auto result = gate_anki_mining(settings, {}, with_video(), {}, true);

    QVERIFY(result.has_value());
    QCOMPARE(result->message, kAudioClipMessage);
}

void MiningGateTest::the_audio_clip_runs_before_the_screenshot() {
    MiningSettings settings = configured();
    settings.needs_video_audio_clip = true;
    settings.needs_video_screenshot = true;
    VideoMiningContext video;
    video.audio_clip_error_message = "audio";
    video.screenshot_error_message = "screenshot";

    const auto result = gate_anki_mining(settings, {}, with_video(video), clean_lookup());

    QVERIFY(result.has_value());
    QCOMPARE(result->message, std::string("audio"));
}

void MiningGateTest::screenshot_needs_an_exact_handlebar_value() {
    QVERIFY(needs_video_screenshot({{"Picture", "{book-cover}"}}));
    QVERIFY(needs_video_screenshot({{"Image", "{video-screenshot}"}}));
    QVERIFY(!needs_video_screenshot({{"Image", "{video-screenshot}-brief"}}));
    QVERIFY(!needs_video_screenshot({{"Image", "{book-cover-brief}"}}));
    QVERIFY(!needs_video_screenshot({{"Image", " {book-cover}"}}));
    QVERIFY(!needs_video_screenshot({}));
}

void MiningGateTest::audio_clip_needs_an_exact_handlebar_value() {
    QVERIFY(needs_video_audio_clip({{"Audio", "{sasayaki-audio}"}}));
    QVERIFY(needs_video_audio_clip({{"Audio", "{video-audio-clip}"}}));
    QVERIFY(!needs_video_audio_clip({{"Audio", "{audio}"}}));
    QVERIFY(!needs_video_audio_clip({{"Audio", "{sasayaki-audio-clip}"}}));
    QVERIFY(!needs_video_audio_clip({}));
}

void MiningGateTest::sasayaki_audio_needs_an_exact_handlebar_value() {
    QVERIFY(needs_sasayaki_audio({{"Sound", "{sasayaki-audio}"}}));
    QVERIFY(!needs_sasayaki_audio({{"Sound", "{sasayaki-audio-clip}"}}));
    QVERIFY(!needs_sasayaki_audio({{"Sound", "{audio}"}}));
    QVERIFY(!needs_sasayaki_audio({{"Sound", "prefix{sasayaki-audio}"}}));
    QVERIFY(!needs_sasayaki_audio({}));
    QVERIFY(needs_sasayaki_audio({{"Word", "{expression}"}, {"Sound", "{sasayaki-audio}"}}));
}

QTEST_APPLESS_MAIN(MiningGateTest)
#include "tst_mining_gate.moc"

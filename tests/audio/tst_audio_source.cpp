// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/dictionary/audio_source.h"

#include <QtTest>

#include <optional>
#include <set>
#include <string>
#include <vector>

using niratan::dictionary::AudioSource;
using niratan::dictionary::audio_source_id;
using niratan::dictionary::destination_offset;
using niratan::dictionary::default_audio_source;
using niratan::dictionary::legacy_local_audio_urls;
using niratan::dictionary::local_audio_source;
using niratan::dictionary::payload;
using niratan::dictionary::synchronizing_local_source;

// Ported from script/test_audio_source_reorder.swift and the
// AudioSourceReorder functions in Models/Dictionary.swift. The cases after the
// script's own ones cover branches it leaves open.
namespace {

const std::string canonical_url = "http://127.0.0.1/audio";
const std::string legacy_url = "http://localhost/audio";

AudioSource canonical_source() {
    return AudioSource{"Local", canonical_url};
}

AudioSource legacy_local(bool enabled = true) {
    return AudioSource{"Legacy Local", legacy_url, enabled};
}

AudioSource source_a() {
    return AudioSource{"A", "https://a.example"};
}

AudioSource source_b() {
    return AudioSource{"B", "https://b.example"};
}

std::set<std::string> legacy_urls() {
    return {legacy_url};
}

}  // namespace

class AudioSourceTest : public QObject {
    Q_OBJECT

   private slots:
    void payload_round_trips_the_identifier();
    void plain_text_has_no_identifier();
    void an_empty_payload_has_no_identifier();

    void dragging_down_inserts_after_the_target_row();
    void dragging_up_inserts_before_the_target_row();
    void dropping_on_the_same_row_has_no_offset();

    void a_local_source_keeps_its_position_and_enabled_state();
    void a_newly_enabled_local_source_starts_at_the_top();
    void disabling_removes_canonical_and_legacy_sources();

    void no_sources_keeps_only_the_canonical_source();
    void no_sources_disabled_stays_empty();
    void a_canonical_source_already_first_is_kept_in_place();
    void a_legacy_url_equal_to_the_canonical_url_is_one_local_source();
    void the_first_local_source_decides_the_enabled_state();
    void a_local_source_at_the_last_index_stays_at_the_end();
    void disabling_without_local_sources_keeps_the_order();
    void built_in_audio_sources_keep_their_defaults();
};

void AudioSourceTest::payload_round_trips_the_identifier() {
    const std::string id = "https://example.test/audio?term={term}";
    QCOMPARE(payload(id), std::string("hoshi-audio-source:https://example.test/audio?term={term}"));

    const auto decoded = audio_source_id(payload(id));
    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, id);
}

void AudioSourceTest::plain_text_has_no_identifier() {
    QVERIFY(!audio_source_id("https://example.test/audio").has_value());
    QCOMPARE(audio_source_id("https://example.test/audio"), std::optional<std::string>{});
}

void AudioSourceTest::an_empty_payload_has_no_identifier() {
    QVERIFY(!audio_source_id("hoshi-audio-source:").has_value());
    QCOMPARE(audio_source_id(""), std::optional<std::string>{});
    QCOMPARE(payload(""), std::string("hoshi-audio-source:"));
}

void AudioSourceTest::dragging_down_inserts_after_the_target_row() {
    QCOMPARE(destination_offset(0, 2), std::optional<int>{3});
    QCOMPARE(destination_offset(2, 5), std::optional<int>{6});
}

void AudioSourceTest::dragging_up_inserts_before_the_target_row() {
    QCOMPARE(destination_offset(2, 0), std::optional<int>{0});
    QCOMPARE(destination_offset(5, 2), std::optional<int>{2});
}

void AudioSourceTest::dropping_on_the_same_row_has_no_offset() {
    QCOMPARE(destination_offset(1, 1), std::optional<int>{});
}

void AudioSourceTest::a_local_source_keeps_its_position_and_enabled_state() {
    const std::vector<AudioSource> sources{source_a(), source_b(), legacy_local(false)};

    const auto result = synchronizing_local_source(sources, true, canonical_source(), legacy_urls());

    const std::vector<AudioSource> expected{
        source_a(),
        source_b(),
        AudioSource{"Local", canonical_url, false},
    };
    QCOMPARE(result, expected);
}

void AudioSourceTest::a_newly_enabled_local_source_starts_at_the_top() {
    const std::vector<AudioSource> sources{source_a(), source_b()};

    const auto result = synchronizing_local_source(sources, true, canonical_source(), legacy_urls());

    const std::vector<AudioSource> expected{canonical_source(), source_a(), source_b()};
    QCOMPARE(result, expected);
}

void AudioSourceTest::disabling_removes_canonical_and_legacy_sources() {
    const std::vector<AudioSource> sources{
        source_a(),
        legacy_local(false),
        canonical_source(),
        source_b(),
    };

    const auto result = synchronizing_local_source(sources, false, canonical_source(), legacy_urls());

    const std::vector<AudioSource> expected{source_a(), source_b()};
    QCOMPARE(result, expected);
}

void AudioSourceTest::no_sources_keeps_only_the_canonical_source() {
    const auto result = synchronizing_local_source({}, true, canonical_source(), {});

    QCOMPARE(result, std::vector<AudioSource>{canonical_source()});
}

void AudioSourceTest::no_sources_disabled_stays_empty() {
    const auto result = synchronizing_local_source({}, false, canonical_source(), {});

    QVERIFY(result.empty());
}

void AudioSourceTest::a_canonical_source_already_first_is_kept_in_place() {
    const std::vector<AudioSource> sources{canonical_source(), source_a(), source_b()};

    const auto result = synchronizing_local_source(sources, true, canonical_source(), legacy_urls());

    QCOMPARE(result, sources);
}

void AudioSourceTest::a_legacy_url_equal_to_the_canonical_url_is_one_local_source() {
    const std::vector<AudioSource> sources{source_a(), legacy_local(), source_b()};
    const std::set<std::string> urls{legacy_url, canonical_url};

    const auto result = synchronizing_local_source(sources, true, canonical_source(), urls);

    const std::vector<AudioSource> expected{source_a(), canonical_source(), source_b()};
    QCOMPARE(result, expected);
}

void AudioSourceTest::the_first_local_source_decides_the_enabled_state() {
    const std::vector<AudioSource> sources{
        source_a(),
        legacy_local(false),
        canonical_source(),
    };

    const auto result = synchronizing_local_source(sources, true, canonical_source(), legacy_urls());

    const std::vector<AudioSource> expected{
        source_a(),
        AudioSource{"Local", canonical_url, false},
    };
    QCOMPARE(result, expected);
}

void AudioSourceTest::a_local_source_at_the_last_index_stays_at_the_end() {
    const std::vector<AudioSource> sources{source_a(), source_b(), canonical_source()};

    const auto result = synchronizing_local_source(sources, true, canonical_source(), legacy_urls());

    const std::vector<AudioSource> expected{source_a(), source_b(), canonical_source()};
    QCOMPARE(result, expected);
}

void AudioSourceTest::disabling_without_local_sources_keeps_the_order() {
    const std::vector<AudioSource> sources{source_a(), source_b()};

    const auto result = synchronizing_local_source(sources, false, canonical_source(), legacy_urls());

    QCOMPARE(result, sources);
}

void AudioSourceTest::built_in_audio_sources_keep_their_defaults() {
    QCOMPARE(default_audio_source(),
             (AudioSource{"Default",
                          "https://hoshi-reader.manhhaoo-do.workers.dev/?term={term}&reading={reading}",
                          true, true}));
    QCOMPARE(local_audio_source(),
             (AudioSource{"Local",
                          "http://localhost:18765/localaudio/get/?term={term}&reading={reading}", true,
                          false}));
    QCOMPARE(legacy_local_audio_urls(),
             std::vector<std::string>{
                 "http://localhost:8765/localaudio/get/?term={term}&reading={reading}"});
}

QTEST_APPLESS_MAIN(AudioSourceTest)
#include "tst_audio_source.moc"

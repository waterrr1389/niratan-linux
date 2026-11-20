// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/media_naming.h"

#include <QtTest>

using niratan::anki::direct_audio_markup;
using niratan::anki::direct_image_markup;
using niratan::anki::safe_media_extension;
using niratan::anki::safe_media_filename;

// Core/AnkiManager.swift (safeAnkiMediaFilename, safeMediaExtension,
// directAudioMarkup, directImageMarkup) has no test script of its own, so the
// cases follow those functions directly.
class MediaNamingTest : public QObject {
    Q_OBJECT

   private slots:
    void allowed_characters_pass_through();
    void other_characters_become_underscores();
    void a_multibyte_character_is_one_underscore();
    void only_the_last_path_component_is_kept();
    void a_name_of_dots_and_underscores_gets_the_default();
    void trimming_decides_emptiness_but_is_not_applied();
    void an_empty_name_gets_the_default();
    void an_empty_extension_uses_the_fallback();
    void an_extension_is_lowercased();
    void an_extension_with_a_dot_uses_the_fallback();
    void an_extension_that_is_only_separators_uses_the_fallback();
    void an_extension_is_sanitized();
    void audio_markup_wraps_the_safe_name();
    void image_markup_wraps_the_safe_name();
};

void MediaNamingTest::allowed_characters_pass_through() {
    QCOMPARE(safe_media_filename("hoshi_audio-1.mp3"), std::string("hoshi_audio-1.mp3"));
}

void MediaNamingTest::other_characters_become_underscores() {
    QCOMPARE(safe_media_filename("a b&c.png"), std::string("a_b_c.png"));
}

void MediaNamingTest::a_multibyte_character_is_one_underscore() {
    QCOMPARE(safe_media_filename("表紙a.png"), std::string("__a.png"));
    QCOMPARE(safe_media_filename("x\xF0\x9F\x98\x80y.png"), std::string("x_y.png"));
}

void MediaNamingTest::only_the_last_path_component_is_kept() {
    QCOMPARE(safe_media_filename("/tmp/dir/cover.png"), std::string("cover.png"));
    QCOMPARE(safe_media_filename("dir/sub/cover.png"), std::string("cover.png"));
    QCOMPARE(safe_media_filename("dir/sub/"), std::string("sub"));
    QCOMPARE(safe_media_filename("/"), std::string("hoshi_media"));
}

void MediaNamingTest::a_name_of_dots_and_underscores_gets_the_default() {
    QCOMPARE(safe_media_filename("._."), std::string("hoshi_media"));
    QCOMPARE(safe_media_filename("..."), std::string("hoshi_media"));
    QCOMPARE(safe_media_filename("表紙"), std::string("hoshi_media"));
}

void MediaNamingTest::trimming_decides_emptiness_but_is_not_applied() {
    QCOMPARE(safe_media_filename("_a_"), std::string("_a_"));
    QCOMPARE(safe_media_filename(".hidden."), std::string(".hidden."));
}

void MediaNamingTest::an_empty_name_gets_the_default() {
    QCOMPARE(safe_media_filename(""), std::string("hoshi_media"));
}

void MediaNamingTest::an_empty_extension_uses_the_fallback() {
    QCOMPARE(safe_media_extension("", "png"), std::string("png"));
}

void MediaNamingTest::an_extension_is_lowercased() {
    QCOMPARE(safe_media_extension("JPG", "png"), std::string("jpg"));
}

void MediaNamingTest::an_extension_with_a_dot_uses_the_fallback() {
    QCOMPARE(safe_media_extension("tar.gz", "bin"), std::string("bin"));
}

void MediaNamingTest::an_extension_that_is_only_separators_uses_the_fallback() {
    QCOMPARE(safe_media_extension("_", "png"), std::string("hoshi_media"));
}

void MediaNamingTest::an_extension_is_sanitized() {
    QCOMPARE(safe_media_extension("m4a?x", "mp3"), std::string("m4a_x"));
}

void MediaNamingTest::audio_markup_wraps_the_safe_name() {
    QCOMPARE(direct_audio_markup("/x/a b.mp3"), std::string("[sound:a_b.mp3]"));
}

void MediaNamingTest::image_markup_wraps_the_safe_name() {
    QCOMPARE(direct_image_markup("/x/a b.png"), std::string("<img src=\"a_b.png\">"));
}

QTEST_APPLESS_MAIN(MediaNamingTest)
#include "tst_media_naming.moc"

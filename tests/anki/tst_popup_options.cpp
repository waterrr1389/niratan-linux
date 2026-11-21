// SPDX-License-Identifier: GPL-3.0-or-later
//
// The five mining globals of the popup hosts, following Core/AnkiManager.swift
// (needsAudio, allowDupes, useAnkiConnect, embedMedia, compactGlossaries),
// Features/Dictionary/DictionarySearchView.swift (buildPopupPayload) and
// Features/Popup/PopupView.swift (buildContent).

#include "services/anki/popup_options.h"

#include <QtTest>

using niratan::anki::AnkiSettings;
using niratan::anki::popup_options;
using niratan::anki::PopupOptions;

class PopupOptionsTest : public QObject {
    Q_OBJECT

   private slots:
    void defaults_match_the_anki_manager();
    void needs_audio_matches_the_exact_audio_handlebar();
    void profile_values_reach_their_globals();
};

// B17: AnkiManager's stored defaults and AnkiProfileConfig fallbacks.
void PopupOptionsTest::defaults_match_the_anki_manager() {
    const PopupOptions options = popup_options(AnkiSettings{});
    QCOMPARE(options.needs_audio, false);
    QCOMPARE(options.allow_dupes, false);
    QCOMPARE(options.use_anki_connect, true);
    QCOMPARE(options.embed_media, false);
    QCOMPARE(options.compact_glossaries, false);
}

// B9: AnkiManager.needsAudio compares fieldMappings values to "{audio}" exactly.
void PopupOptionsTest::needs_audio_matches_the_exact_audio_handlebar() {
    AnkiSettings settings;
    settings.field_mappings = {{"Word", "{expression}"}};
    QCOMPARE(popup_options(settings).needs_audio, false);

    settings.field_mappings = {{"Audio", "{audio}"}};
    QCOMPARE(popup_options(settings).needs_audio, true);

    for (const char* value : {"{sasayaki-audio}", "{video-audio}", "{audio}suffix", "prefix{audio}",
                              " {audio}", "{Audio}"}) {
        settings.field_mappings = {{"Audio", value}};
        QCOMPARE(popup_options(settings).needs_audio, false);
    }
}

// B10–B13: the profile's allowDupes, embedMedia and compactGlossaries, and the
// computed constant useAnkiConnect.
void PopupOptionsTest::profile_values_reach_their_globals() {
    AnkiSettings settings;
    settings.allow_dupes = true;
    settings.embed_media = true;
    settings.compact_glossaries = true;

    const PopupOptions options = popup_options(settings);
    QCOMPARE(options.needs_audio, false);
    QCOMPARE(options.allow_dupes, true);
    QCOMPARE(options.use_anki_connect, true);
    QCOMPARE(options.embed_media, true);
    QCOMPARE(options.compact_glossaries, true);
}

QTEST_APPLESS_MAIN(PopupOptionsTest)
#include "tst_popup_options.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary popup HTML shell, following
// Features/Popup/PopupView.swift (buildContent), Features/Popup/PopupWebView.swift
// and Niratan-win's Services/Dictionary/PopupHtmlGenerator.cs.
//
// The shell inlines the popup stylesheet and scripts and assigns the globals
// the script reads before it runs: lookup entries, dictionary styles, the
// display settings, audio sources and the theme colors. The markup uses the
// same ids and classes as the Yomitan-derived popup (popup-viewport,
// popup-error, entries-container, overlay) and the theme variables
// --background-color and --text-color from Features/Popup/popup.css.
//
// Script order follows PopupWebView.constructHtml: the globals (including
// scanNonJapaneseText, scanLength, kanjiLookupEnabled and twoColumnLayout),
// then selection.js, then the window.hoshiSelection.language assignment with
// the profile's language id, then popup.js. Every inline script carries the
// same per-document nonce. The collapse and exclude sets travel in the
// globals script: PopupView.buildContent writes window.collapsedDictionaries
// only for Custom mode and window.excludedDictionaries in every mode.
//
// Values are embedded as JSON in a script context. "</" inside serialized
// data is written as "<\/" so a glossary cannot close the script element;
// the JSON meaning is unchanged. Strings are UTF-8. The stylesheet and script
// texts are trusted application assets and are inserted unchanged.
// Only scripts carrying the shell's per-document nonce can run.
//
// Defaults come from Core/UserConfig.swift: maxResults and scanLength 16,
// scanNonJapaneseText on, Expand All, compact glossaries on, expression tags
// off, harmonic frequency off, pitch deduplication off, compact pitch accents
// on, two column and expand-first off, autoplay off, playback mode interrupt
// and swipe threshold 0 when swipe-to-dismiss is off. kanji_lookup_enabled is
// false because the port has no kanji lookup.
//
// DictionarySearchView.buildPopupPayload gives the dictionary page one extra
// pair: an optional query source and its text size. When the query source is
// set, #entries-container starts with a dictionary-query-source div sized by
// search_text_size and its divider, and the globals script assigns the query to
// window.dictionaryQuerySource; the script fills the heading text. When it is
// absent, neither the markup nor the assignment is written, which keeps the
// reader and video documents of PopupView.buildContent unchanged. The size
// defaults to UserConfig.searchTextSize, 26.

#pragma once

#include <QJsonArray>
#include <QJsonObject>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::dictionary {

struct PopupDisplaySettings {
    std::string content_language_id = "ja";
    int max_results = 16;
    int scan_length = 16;
    // UserConfig.scanNonJapaneseText, read by selection.js.
    bool scan_non_japanese_text = true;
    // PopupWebView.kanjiLookupEnabled; the port has no kanji lookup.
    bool kanji_lookup_enabled = false;
    // CollapseMode raw value: "Expand All", "Collapse All" or "Custom".
    std::string collapse_mode = "Expand All";
    bool expand_first_dictionary = false;
    bool two_column_layout = false;
    // Titles the popup starts collapsed. Only Custom mode carries them, as
    // PopupView.buildContent writes [] for the other modes.
    std::vector<std::string> collapsed_dictionaries;
    // Titles of the resolved term dictionaries whose category is exclude, in
    // list order, in every collapse mode (DictionaryManager.excludedDictionaries).
    std::vector<std::string> excluded_dictionaries;
    bool compact_glossaries = true;
    bool show_expression_tags = false;
    bool harmonic_frequency = false;
    bool deduplicate_pitch_accents = false;
    bool compact_pitch_accents = true;
    std::string custom_css;
    // Zero when swipe-to-dismiss is off.
    int swipe_threshold = 0;
    // DictionarySearchView.buildPopupPayload querySource; absent for the
    // reader and video popup documents.
    std::optional<std::string> query_source;
    // UserConfig.searchTextSize, the query heading's pixel size.
    int search_text_size = 26;
};

struct PopupAudioSettings {
    // Enabled source URLs and their names, in list order.
    std::vector<std::string> sources;
    std::vector<std::string> source_names;
    bool autoplay = false;
    // AudioPlaybackMode raw value: "interrupt", "duck" or "mix".
    std::string playback_mode = "interrupt";
};

struct PopupMiningSettings {
    bool needs_audio = false;
    bool allow_dupes = false;
    bool use_anki_connect = true;
    bool embed_media = false;
    bool compact_glossaries = false;
};

struct PopupTheme {
    bool dark = false;
    // "#RRGGBB" colors applied to --background-color and --text-color.
    std::string background = "#FFFFFF";
    std::string text = "#000000";
};

struct PopupConfig {
    PopupDisplaySettings display;
    PopupAudioSettings audio;
    PopupMiningSettings mining;
    PopupTheme theme;
};

// css, selection_js and js are the contents of popup.css, selection.js and
// popup.js. selection_js runs before the language assignment; js runs last.
std::string popup_shell_html(const QJsonArray& entries, const QJsonObject& dictionary_styles,
                             const PopupConfig& config, std::string_view css,
                             std::string_view selection_js, std::string_view js);

}  // namespace niratan::dictionary

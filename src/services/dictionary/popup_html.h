// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary popup HTML shell, following
// Features/Popup/PopupView.swift (buildContent), Features/Popup/PopupWebView.swift
// and Niratan-win's Services/Dictionary/PopupHtmlGenerator.cs.
//
// The shell inlines the popup stylesheet and script and assigns the globals
// the script reads before it runs: lookup entries, dictionary styles, the
// display settings, audio sources and the theme colors. The markup uses the
// same ids and classes as the Yomitan-derived popup (popup-viewport,
// popup-error, entries-container, overlay) and the theme variables
// --background-color and --text-color from Features/Popup/popup.css.
//
// Values are embedded as JSON in a script context. "</" inside serialized
// data is written as "<\/" so a glossary cannot close the script element;
// the JSON meaning is unchanged. Strings are UTF-8. The stylesheet and script
// texts are trusted application assets and are inserted unchanged.
// Only scripts carrying the shell's per-document nonce can run.
//
// Defaults come from Core/UserConfig.swift: maxResults and scanLength 16,
// Expand All, compact glossaries on, expression tags off, harmonic frequency
// off, pitch deduplication off, compact pitch accents on, two column and
// expand-first off, autoplay off, playback mode interrupt and swipe threshold
// 0 when swipe-to-dismiss is off.

#pragma once

#include <QJsonArray>
#include <QJsonObject>

#include <string>
#include <string_view>
#include <vector>

namespace niratan::dictionary {

struct PopupDisplaySettings {
    std::string content_language_id = "ja";
    int max_results = 16;
    int scan_length = 16;
    // CollapseMode raw value: "Expand All", "Collapse All" or "Custom".
    std::string collapse_mode = "Expand All";
    bool expand_first_dictionary = false;
    bool two_column_layout = false;
    bool compact_glossaries = true;
    bool show_expression_tags = false;
    bool harmonic_frequency = false;
    bool deduplicate_pitch_accents = false;
    bool compact_pitch_accents = true;
    std::string custom_css;
    // Zero when swipe-to-dismiss is off.
    int swipe_threshold = 0;
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

// css and js are the contents of popup.css and popup.js.
std::string popup_shell_html(const QJsonArray& entries, const QJsonObject& dictionary_styles,
                             const PopupConfig& config, std::string_view css,
                             std::string_view js);

}  // namespace niratan::dictionary

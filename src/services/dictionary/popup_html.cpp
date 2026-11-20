// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows Features/Popup/PopupView.swift (buildContent), PopupWebView.swift and
// Niratan-win's Services/Dictionary/PopupHtmlGenerator.cs (GenerateHtml).
//
// Markup and the global assignments before the popup script are fixed by
// popup.js. Serialized data goes through escaped_json because a literal "</"
// inside a script element ends that element early; "<\/" is the same JSON
// string value. The stylesheet and script texts are trusted assets and are
// inserted unchanged.

#include "popup_html.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QString>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::dictionary {
namespace {

// Every occurrence of "</" would close the enclosing script element, so it is
// written as "<\/", which JSON decodes back to the same string.
std::string escaped_json(const QJsonDocument& document) {
    std::string json = document.toJson(QJsonDocument::Compact).toStdString();
    for (std::size_t position = json.find("</"); position != std::string::npos;
         position = json.find("</", position + 3)) {
        json.replace(position, 2, "<\\/");
    }
    return json;
}

// A JSON string literal for a value embedded next to other globals; the
// one-element array serialization is stripped of its brackets.
std::string json_string(const std::string& value) {
    const std::string quoted =
        escaped_json(QJsonDocument(QJsonArray{QString::fromStdString(value)}));
    return quoted.substr(1, quoted.size() - 2);
}

QJsonArray json_string_array(const std::vector<std::string>& values) {
    QJsonArray array;
    for (const std::string& value : values) {
        array.append(QString::fromStdString(value));
    }
    return array;
}

const char* js_bool(bool value) {
    return value ? "true" : "false";
}

}  // namespace

std::string popup_shell_html(const QJsonArray& entries, const QJsonObject& dictionary_styles,
                             const PopupConfig& config, std::string_view css,
                             std::string_view js) {
    std::string html;
    html.reserve(css.size() + js.size() + 2048);

    html += "<!doctype html>\n<html lang=\"ja\" data-niratan-color-scheme=\"";
    html += config.theme.dark ? "dark" : "light";
    html += "\">\n<head>\n";
    html += "<meta charset=\"utf-8\" />\n";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, "
            "maximum-scale=1.0, user-scalable=no\" />\n";
    html += "<title>Dictionary Lookup</title>\n";
    html += "<style>";
    html += css;
    html += "</style>\n";
    html += "<style>html, body { --background-color: ";
    html += config.theme.background;
    html += "; --text-color: ";
    html += config.theme.text;
    html += "; background-color: var(--background-color); color: var(--text-color); }";
    html += "</style>\n";
    html += "</head>\n<body>\n";
    html += "<div id=\"popup-viewport\">\n";
    html += "<div id=\"popup-error\" style=\"display:none;\"></div>\n";
    html += "<div id=\"entries-container\"></div>\n";
    html += "<div class=\"overlay\">";
    html += "<div class=\"overlay-close\" onclick=\"closeOverlay()\">x</div>";
    html += "<div class=\"overlay-content\"></div></div>\n";
    html += "</div>\n";
    html += "<script>\n";

    html += "window.lookupEntries = ";
    html += escaped_json(QJsonDocument(entries));
    html += ";\nwindow.entryCount = ";
    html += std::to_string(entries.size());
    html += ";\nwindow.dictionaryStyles = ";
    html += escaped_json(QJsonDocument(dictionary_styles));
    html += ";\n";

    // Dictionary visibility starts empty; the script restores saved state.
    html += "window.collapsedDictionaries = [];\n";
    html += "window.excludedDictionaries = [];\n";

    const PopupDisplaySettings& display = config.display;
    html += "window.compactGlossaries = ";
    html += js_bool(display.compact_glossaries);
    html += ";\nwindow.compactPitchAccents = ";
    html += js_bool(display.compact_pitch_accents);
    html += ";\nwindow.harmonicFrequency = ";
    html += js_bool(display.harmonic_frequency);
    html += ";\nwindow.deduplicatePitchAccents = ";
    html += js_bool(display.deduplicate_pitch_accents);
    html += ";\nwindow.twoColumnLayout = ";
    html += js_bool(display.two_column_layout);
    html += ";\nwindow.expandFirstDictionary = ";
    html += js_bool(display.expand_first_dictionary);
    html += ";\nwindow.collapseMode = ";
    html += json_string(display.collapse_mode);
    html += ";\nwindow.showExpressionTags = ";
    html += js_bool(display.show_expression_tags);
    html += ";\nwindow.contentLanguageId = ";
    html += json_string(display.content_language_id);
    html += ";\nwindow.maxResults = ";
    html += std::to_string(display.max_results);
    html += ";\nwindow.scanLength = ";
    html += std::to_string(display.scan_length);
    html += ";\n";

    html += "window.audioSources = ";
    html += escaped_json(QJsonDocument(json_string_array(config.audio.sources)));
    html += ";\nwindow.audioSourceNames = ";
    html += escaped_json(QJsonDocument(json_string_array(config.audio.source_names)));
    html += ";\nwindow.audioEnableAutoplay = ";
    html += js_bool(config.audio.autoplay);
    html += ";\nwindow.audioPlaybackMode = ";
    html += json_string(config.audio.playback_mode);
    html += ";\nwindow.customCSS = ";
    html += json_string(display.custom_css);
    html += ";\nwindow.swipeThreshold = ";
    html += std::to_string(display.swipe_threshold);
    html += ";\n";

    const PopupMiningSettings& mining = config.mining;
    html += "window.needsAudio = ";
    html += js_bool(mining.needs_audio);
    html += ";\nwindow.allowDupes = ";
    html += js_bool(mining.allow_dupes);
    html += ";\nwindow.useAnkiConnect = ";
    html += js_bool(mining.use_anki_connect);
    html += ";\nwindow.embedMedia = ";
    html += js_bool(mining.embed_media);
    html += ";\nwindow.compactGlossariesAnki = ";
    html += js_bool(mining.compact_glossaries);
    html += ";\n";

    html += "</script>\n<script>";
    html += js;
    html += "</script>\n</body>\n</html>";

    return html;
}

}  // namespace niratan::dictionary

// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/popup_html.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QTest>

#include <algorithm>
#include <cctype>
#include <string>

using niratan::dictionary::popup_shell_html;
using niratan::dictionary::PopupConfig;
using niratan::dictionary::PopupDisplaySettings;
using niratan::dictionary::PopupTheme;

// Features/Popup/PopupView.swift (buildContent), PopupWebView.swift
// (constructHtml) and PopupHtmlGenerator.cs define the shell; popup.js
// consumes the globals and selection.js supplies window.hoshiSelection.
namespace {

std::string globals_section(const std::string& html) {
    const std::string marker = "window.lookupEntries = ";
    const std::size_t start = html.find(marker);
    if (start == std::string::npos) {
        return {};
    }
    const std::size_t value_start = start + marker.size();
    const std::size_t end = html.find(";\n", value_start);
    if (end == std::string::npos) {
        return {};
    }
    return html.substr(value_start, end - value_start);
}

// The JSON value of a `window.<name> = <value>;` assignment.
std::string array_section(const std::string& html, const std::string& name) {
    const std::string marker = "window." + name + " = ";
    const std::size_t start = html.find(marker);
    if (start == std::string::npos) {
        return {};
    }
    const std::size_t value_start = start + marker.size();
    const std::size_t end = html.find(";\n", value_start);
    if (end == std::string::npos) {
        return {};
    }
    return html.substr(value_start, end - value_start);
}

QStringList string_array(const std::string& json) {
    const QJsonArray array = QJsonDocument::fromJson(QByteArray::fromStdString(json)).array();
    QStringList values;
    for (const QJsonValue& value : array) {
        values.append(value.toString());
    }
    std::sort(values.begin(), values.end());
    return values;
}

QJsonArray entries_fixture() {
    QJsonObject entry;
    entry.insert("expression", QString("decide"));
    entry.insert("reading", QString("decide"));
    entry.insert("matched", QString("decided"));
    return QJsonArray{entry};
}

PopupConfig dark_config() {
    PopupConfig config;
    config.theme.dark = true;
    config.theme.background = "#1E1E1E";
    config.theme.text = "#FFFFFF";
    return config;
}

}  // namespace

class PopupHtmlTest : public QObject {
    Q_OBJECT

   private slots:
    void the_shell_has_the_popup_markup_and_theme();
    void entries_are_embedded_and_counted();
    void dictionary_styles_are_embedded();
    void display_settings_are_embedded();
    void audio_settings_are_embedded();
    void closing_tags_in_data_are_escaped();
    void custom_css_is_embedded_as_a_json_string();
    void stylesheet_and_script_are_inserted();
    void only_shell_scripts_can_use_the_web_channel();
    void empty_entries_are_an_empty_array();
    void profile_dictionary_sets_are_embedded();
    void empty_dictionary_sets_are_empty_arrays();
    void dictionary_titles_keep_their_json_escaping();
    void selection_script_runs_before_the_popup_script();
    void query_source_heading_is_sized_and_precedes_the_entries_script();
    void query_source_global_is_json_escaped();
    void no_query_source_writes_no_heading_or_global();
};

void PopupHtmlTest::the_shell_has_the_popup_markup_and_theme() {
    const std::string html = popup_shell_html({}, {}, dark_config(), "/*css*/", "", "//js");

    QVERIFY(html.starts_with("<!doctype html>"));
    QVERIFY(html.find("data-niratan-color-scheme=\"dark\"") != std::string::npos);
    QVERIFY(html.find("--background-color: #1E1E1E;") != std::string::npos);
    QVERIFY(html.find("--text-color: #FFFFFF;") != std::string::npos);
    QVERIFY(html.find("id=\"popup-viewport\"") != std::string::npos);
    QVERIFY(html.find("id=\"popup-error\"") != std::string::npos);
    QVERIFY(html.find("id=\"entries-container\"") != std::string::npos);
    QVERIFY(html.find("class=\"overlay\"") != std::string::npos);
}

void PopupHtmlTest::entries_are_embedded_and_counted() {
    const QJsonArray entries = entries_fixture();
    const std::string html = popup_shell_html(entries, {}, PopupConfig{}, "", "", "");

    const QJsonDocument embedded =
        QJsonDocument::fromJson(QByteArray::fromStdString(globals_section(html)));
    QCOMPARE(embedded.array(), entries);
    QVERIFY(html.find("window.entryCount = 1;") != std::string::npos);
}

void PopupHtmlTest::dictionary_styles_are_embedded() {
    const QJsonObject styles{{"Probe", "b { color: red; }"}};
    const std::string html = popup_shell_html({}, styles, PopupConfig{}, "", "", "");

    QVERIFY(html.find("window.dictionaryStyles = {\"Probe\":\"b { color: red; }\"};") !=
            std::string::npos);
}

void PopupHtmlTest::display_settings_are_embedded() {
    PopupConfig config;
    config.display.max_results = 7;
    config.display.scan_length = 9;
    config.display.scan_non_japanese_text = false;
    config.display.kanji_lookup_enabled = false;
    config.display.collapse_mode = "Collapse All";
    config.display.expand_first_dictionary = true;
    config.display.two_column_layout = true;
    config.display.compact_glossaries = false;
    config.display.show_expression_tags = true;
    config.display.harmonic_frequency = true;
    config.display.deduplicate_pitch_accents = true;
    config.display.compact_pitch_accents = false;
    config.display.swipe_threshold = 40;

    const std::string html = popup_shell_html({}, {}, config, "", "", "");

    QVERIFY(html.find("window.maxResults = 7;") != std::string::npos);
    QVERIFY(html.find("window.scanLength = 9;") != std::string::npos);
    QVERIFY(html.find("window.scanNonJapaneseText = false;") != std::string::npos);
    QVERIFY(html.find("window.kanjiLookupEnabled = false;") != std::string::npos);
    QVERIFY(html.find("window.collapseMode = \"Collapse All\";") != std::string::npos);
    QVERIFY(html.find("window.expandFirstDictionary = true;") != std::string::npos);
    QVERIFY(html.find("window.twoColumnLayout = true;") != std::string::npos);
    QVERIFY(html.find("window.compactGlossaries = false;") != std::string::npos);
    QVERIFY(html.find("window.showExpressionTags = true;") != std::string::npos);
    QVERIFY(html.find("window.harmonicFrequency = true;") != std::string::npos);
    QVERIFY(html.find("window.deduplicatePitchAccents = true;") != std::string::npos);
    QVERIFY(html.find("window.compactPitchAccents = false;") != std::string::npos);
    QVERIFY(html.find("window.swipeThreshold = 40;") != std::string::npos);
}

void PopupHtmlTest::audio_settings_are_embedded() {
    PopupConfig config;
    config.audio.sources = {"http://localhost/audio?term={term}"};
    config.audio.source_names = {"Local"};
    config.audio.autoplay = true;
    config.audio.playback_mode = "duck";
    config.mining.needs_audio = true;
    config.mining.allow_dupes = true;

    const std::string html = popup_shell_html({}, {}, config, "", "", "");

    QVERIFY(html.find("window.audioSources = [\"http://localhost/audio?term={term}\"];") !=
            std::string::npos);
    QVERIFY(html.find("window.audioSourceNames = [\"Local\"];") != std::string::npos);
    QVERIFY(html.find("window.audioEnableAutoplay = true;") != std::string::npos);
    QVERIFY(html.find("window.audioPlaybackMode = \"duck\";") != std::string::npos);
    QVERIFY(html.find("window.needsAudio = true;") != std::string::npos);
    QVERIFY(html.find("window.allowDupes = true;") != std::string::npos);
}

void PopupHtmlTest::closing_tags_in_data_are_escaped() {
    const QString text = "</script><script>alert(1)</script>";
    QJsonObject entry;
    entry.insert("expression", text);
    const QJsonArray entries{entry};

    const std::string html = popup_shell_html(entries, {}, PopupConfig{}, "", "", "");
    const std::string section = globals_section(html);

    QVERIFY(!section.empty());
    QVERIFY(section.find("</script>") == std::string::npos);
    const QJsonDocument embedded = QJsonDocument::fromJson(QByteArray::fromStdString(section));
    QCOMPARE(embedded.array().first().toObject().value("expression").toString(), text);
}

void PopupHtmlTest::custom_css_is_embedded_as_a_json_string() {
    PopupConfig config;
    config.display.custom_css = "body { color: red; }";

    const std::string html = popup_shell_html({}, {}, config, "", "", "");

    QVERIFY(html.find("window.customCSS = \"body { color: red; }\";") != std::string::npos);
}

void PopupHtmlTest::stylesheet_and_script_are_inserted() {
    const std::string html = popup_shell_html({}, {}, PopupConfig{}, "/*CSS*/", "", "//JS");

    QVERIFY(html.find("<style>/*CSS*/</style>") != std::string::npos);
    QVERIFY(html.find("//JS") != std::string::npos);
    QVERIFY(html.find("//JS") > html.find("window.lookupEntries"));
}

// PopupWebView.constructHtml puts the globals, selection.js, the language
// assignment and popup.js in one nonced script sequence.
void PopupHtmlTest::only_shell_scripts_can_use_the_web_channel() {
    const auto html =
        QString::fromStdString(popup_shell_html({}, {}, PopupConfig{}, "", "//selection", "//JS"));
    const auto policy = QRegularExpression("script-src 'nonce-([a-f0-9-]+)'").match(html);
    QVERIFY(policy.hasMatch());
    QCOMPARE(html.count("<script nonce=\"" + policy.captured(1) + "\">"), 4);
    QVERIFY(!html.contains("<script>"));
}

void PopupHtmlTest::empty_entries_are_an_empty_array() {
    const std::string html = popup_shell_html({}, {}, PopupConfig{}, "", "", "");

    QVERIFY(html.find("window.lookupEntries = [];") != std::string::npos);
    QVERIFY(html.find("window.entryCount = 0;") != std::string::npos);
}

// B1: PopupView.buildContent and DictionaryManager.collapsedDictionaries /
// excludedDictionaries; PopupWebView.constructHtml's scan globals.
void PopupHtmlTest::profile_dictionary_sets_are_embedded() {
    PopupConfig config;
    config.display.scan_non_japanese_text = false;
    config.display.kanji_lookup_enabled = false;
    config.display.collapsed_dictionaries = {"B", "A"};
    config.display.excluded_dictionaries = {"Jitendex"};
    config.display.content_language_id = "en";

    const std::string html =
        popup_shell_html({}, {}, config, "", "window.hoshiSelection = {language: 'ja'};", "//JS");

    QVERIFY(html.find("window.scanNonJapaneseText = false;") != std::string::npos);
    QVERIFY(html.find("window.kanjiLookupEnabled = false;") != std::string::npos);
    QCOMPARE(string_array(array_section(html, "collapsedDictionaries")), (QStringList{"A", "B"}));
    QCOMPARE(string_array(array_section(html, "excludedDictionaries")), (QStringList{"Jitendex"}));
}

void PopupHtmlTest::empty_dictionary_sets_are_empty_arrays() {
    const std::string html = popup_shell_html({}, {}, PopupConfig{}, "", "", "");

    QVERIFY(html.find("window.collapsedDictionaries = [];\n") != std::string::npos);
    QVERIFY(html.find("window.excludedDictionaries = [];\n") != std::string::npos);
}

void PopupHtmlTest::dictionary_titles_keep_their_json_escaping() {
    PopupConfig config;
    const QString apostrophe = QStringLiteral("O'Brien");
    const QString backslash = QStringLiteral("a\\b");
    config.display.collapsed_dictionaries = {apostrophe.toStdString(), backslash.toStdString()};
    config.display.excluded_dictionaries = {apostrophe.toStdString()};

    const std::string html = popup_shell_html({}, {}, config, "", "", "");

    QStringList collapsed = string_array(array_section(html, "collapsedDictionaries"));
    collapsed.sort();
    QStringList expected{apostrophe, backslash};
    expected.sort();
    QCOMPARE(collapsed, expected);
    QCOMPARE(string_array(array_section(html, "excludedDictionaries")), QStringList{apostrophe});
    // The backslash survives as JSON escaping, so the quote stays a value.
    QVERIFY(html.find("\"a\\\\b\"") != std::string::npos);
}

// B2: the selection script runs before the language assignment, which runs
// before popup.js.
void PopupHtmlTest::selection_script_runs_before_the_popup_script() {
    PopupConfig config;
    config.display.content_language_id = "en";
    const std::string selection = "window.hoshiSelection = {language: 'ja'};";
    const std::string popup_script = "//POPUP";

    const std::string html = popup_shell_html({}, {}, config, "", selection, popup_script);

    const std::size_t selection_at = html.find(selection);
    const std::size_t language_at = html.find("window.hoshiSelection.language = 'en';");
    const std::size_t popup_at = html.find(popup_script);
    QVERIFY(selection_at != std::string::npos);
    QVERIFY(language_at != std::string::npos);
    QVERIFY(popup_at != std::string::npos);
    QVERIFY(selection_at < language_at);
    QVERIFY(language_at < popup_at);
    QVERIFY(html.find("window.scanNonJapaneseText") < selection_at);
}

// B1: DictionarySearchView.buildPopupPayload writes the query heading and its
// divider as the first children of #entries-container, sized by searchTextSize.
void PopupHtmlTest::query_source_heading_is_sized_and_precedes_the_entries_script() {
    PopupConfig config = dark_config();
    config.display.query_source = "猫";
    config.display.search_text_size = 30;

    const std::string html = popup_shell_html({}, {}, config, "", "", "");

    const std::string heading =
        "<div id=\"dictionary-query-source\" class=\"dictionary-query-source\" "
        "style=\"font-size: 30px; min-height: calc(30px * 1.4);\"></div>";
    const std::string divider = "<hr class=\"dictionary-query-source-divider\">";
    const std::size_t heading_at = html.find(heading);
    const std::size_t divider_at = html.find(divider);
    const std::size_t container_at = html.find("id=\"entries-container\">");
    std::size_t first_child = container_at + std::string("id=\"entries-container\">").size();
    while (first_child < html.size() &&
           std::isspace(static_cast<unsigned char>(html[first_child]))) {
        ++first_child;
    }
    QCOMPARE(first_child, heading_at);
    std::size_t after_heading = heading_at + heading.size();
    while (after_heading < html.size() &&
           std::isspace(static_cast<unsigned char>(html[after_heading]))) {
        ++after_heading;
    }
    QCOMPARE(after_heading, divider_at);
    const std::size_t entries_script_at = html.find("window.lookupEntries = ");
    QVERIFY(container_at != std::string::npos);
    QVERIFY(heading_at != std::string::npos);
    QVERIFY(divider_at != std::string::npos);
    QVERIFY(entries_script_at != std::string::npos);
    QVERIFY(container_at < heading_at);
    QVERIFY(heading_at < divider_at);
    QVERIFY(divider_at < entries_script_at);
    QVERIFY(html.find("window.dictionaryQuerySource = \"猫\";") != std::string::npos);
}

// B2: the query reaches popup.js as a JSON string and cannot close the script.
void PopupHtmlTest::query_source_global_is_json_escaped() {
    PopupConfig config;
    config.display.query_source = "a\"b</script>";

    const std::string html = popup_shell_html({}, {}, config, "", "", "");

    const std::string section = array_section(html, "dictionaryQuerySource");
    QVERIFY(!section.empty());
    QVERIFY(section.find("</") == std::string::npos);
    const QJsonArray decoded =
        QJsonDocument::fromJson(QByteArray::fromStdString("[" + section + "]")).array();
    QCOMPARE(decoded.size(), 1);
    QCOMPARE(decoded.at(0).toString(), QStringLiteral("a\"b</script>"));
}

// B2: PopupView.buildContent has no query source, so reader and video
// documents keep neither the heading nor the global.
void PopupHtmlTest::no_query_source_writes_no_heading_or_global() {
    const std::string html = popup_shell_html({}, {}, PopupConfig{}, "", "", "");

    QVERIFY(html.find("<div id=\"dictionary-query-source\"") == std::string::npos);
    QVERIFY(html.find("class=\"dictionary-query-source-divider\"") == std::string::npos);
    QVERIFY(html.find("window.dictionaryQuerySource") == std::string::npos);
}

QTEST_APPLESS_MAIN(PopupHtmlTest)
#include "tst_popup_html.moc"

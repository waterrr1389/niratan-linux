// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/popup_html.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtTest>

#include <string>

using niratan::dictionary::PopupConfig;
using niratan::dictionary::PopupDisplaySettings;
using niratan::dictionary::PopupTheme;
using niratan::dictionary::popup_shell_html;

// Features/Popup/PopupView.swift (buildContent), PopupWebView.swift and
// PopupHtmlGenerator.cs define the shell; popup.js consumes the globals.
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
};

void PopupHtmlTest::the_shell_has_the_popup_markup_and_theme() {
    const std::string html = popup_shell_html({}, {}, dark_config(), "/*css*/", "//js");

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
    const std::string html = popup_shell_html(entries, {}, PopupConfig{}, "", "");

    const QJsonDocument embedded = QJsonDocument::fromJson(QByteArray::fromStdString(globals_section(html)));
    QCOMPARE(embedded.array(), entries);
    QVERIFY(html.find("window.entryCount = 1;") != std::string::npos);
}

void PopupHtmlTest::dictionary_styles_are_embedded() {
    const QJsonObject styles{{"Probe", "b { color: red; }"}};
    const std::string html = popup_shell_html({}, styles, PopupConfig{}, "", "");

    QVERIFY(html.find("window.dictionaryStyles = {\"Probe\":\"b { color: red; }\"};") !=
            std::string::npos);
}

void PopupHtmlTest::display_settings_are_embedded() {
    PopupConfig config;
    config.display.max_results = 7;
    config.display.scan_length = 9;
    config.display.collapse_mode = "Collapse All";
    config.display.expand_first_dictionary = true;
    config.display.two_column_layout = true;
    config.display.compact_glossaries = false;
    config.display.show_expression_tags = true;
    config.display.harmonic_frequency = true;
    config.display.deduplicate_pitch_accents = true;
    config.display.compact_pitch_accents = false;
    config.display.swipe_threshold = 40;

    const std::string html = popup_shell_html({}, {}, config, "", "");

    QVERIFY(html.find("window.maxResults = 7;") != std::string::npos);
    QVERIFY(html.find("window.scanLength = 9;") != std::string::npos);
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

    const std::string html = popup_shell_html({}, {}, config, "", "");

    QVERIFY(html.find(
                "window.audioSources = [\"http://localhost/audio?term={term}\"];") !=
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

    const std::string html = popup_shell_html(entries, {}, PopupConfig{}, "", "");
    const std::string section = globals_section(html);

    QVERIFY(!section.empty());
    QVERIFY(section.find("</script>") == std::string::npos);
    const QJsonDocument embedded = QJsonDocument::fromJson(QByteArray::fromStdString(section));
    QCOMPARE(embedded.array().first().toObject().value("expression").toString(), text);
}

void PopupHtmlTest::custom_css_is_embedded_as_a_json_string() {
    PopupConfig config;
    config.display.custom_css = "body { color: red; }";

    const std::string html = popup_shell_html({}, {}, config, "", "");

    QVERIFY(html.find("window.customCSS = \"body { color: red; }\";") != std::string::npos);
}

void PopupHtmlTest::stylesheet_and_script_are_inserted() {
    const std::string html = popup_shell_html({}, {}, PopupConfig{}, "/*CSS*/", "//JS");

    QVERIFY(html.find("<style>/*CSS*/</style>") != std::string::npos);
    QVERIFY(html.find("//JS") != std::string::npos);
    QVERIFY(html.find("//JS") > html.find("window.lookupEntries"));
}

void PopupHtmlTest::only_shell_scripts_can_use_the_web_channel() {
    const auto html = QString::fromStdString(popup_shell_html({}, {}, PopupConfig{}, "", "//JS"));
    const auto policy = QRegularExpression("script-src 'nonce-([a-f0-9-]+)'").match(html);
    QVERIFY(policy.hasMatch());
    QCOMPARE(html.count("<script nonce=\"" + policy.captured(1) + "\">"), 2);
    QVERIFY(!html.contains("<script>"));
    QVERIFY(!html.contains("onclick="));
}

void PopupHtmlTest::empty_entries_are_an_empty_array() {
    const std::string html = popup_shell_html({}, {}, PopupConfig{}, "", "");

    QVERIFY(html.find("window.lookupEntries = [];") != std::string::npos);
    QVERIFY(html.find("window.entryCount = 0;") != std::string::npos);
}

QTEST_APPLESS_MAIN(PopupHtmlTest)
#include "tst_popup_html.moc"

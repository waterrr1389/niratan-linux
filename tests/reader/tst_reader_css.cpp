// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/reader_css.h"

#include <QTest>

#include <string>

using niratan::reader::Appearance;
using niratan::reader::Colors;
using niratan::reader::PageSize;
using niratan::reader::reader_css;
using niratan::reader::reader_spacer_script;

// Cases follow NativeReaderView.injectReader: the writing mode, the column
// rules, padding, image caps, furigana modes and paragraph mode.
namespace {

Appearance base() {
    Appearance appearance;
    appearance.selected_font = "Noto Serif JP";
    return appearance;
}

PageSize page() { return PageSize{800, 600}; }

Colors colors() {
    return Colors{"#111111", "#FAFAFA", "#222222", "#EEEEEE"};
}

bool has(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

class ReaderCssTest : public QObject {
    Q_OBJECT

   private slots:
    void vertical_writing_is_the_default_mode();
    void horizontal_writing_paginates_with_columns();
    void two_columns_only_apply_to_paged_horizontal_writing();
    void continuous_mode_removes_the_page_height();
    void padding_and_overlap_are_written();
    void image_caps_follow_the_layout();
    void typography_switches_apply_only_when_advanced();
    void furigana_modes_change_the_rules();
    void paragraph_mode_breaks_before_paragraphs();
    void colors_and_font_are_inserted();
    void spacers_follow_the_padding();
};

void ReaderCssTest::vertical_writing_is_the_default_mode() {
    const std::string css = reader_css(base(), page(), colors(), "");

    QVERIFY(has(css, "writing-mode: vertical-rl !important;"));
    QVERIFY(has(css, "width: var(--page-width, 100vw) !important;"));
    QVERIFY(has(css, "column-width: var(--page-height, 100vh) !important;"));
    QVERIFY(has(css, "column-gap: calc(0vh + 22px) !important;"));
}

void ReaderCssTest::horizontal_writing_paginates_with_columns() {
    Appearance appearance = base();
    appearance.vertical_writing = false;
    const std::string css = reader_css(appearance, page(), colors(), "");

    QVERIFY(has(css, "writing-mode: horizontal-tb !important;"));
    QVERIFY(has(css, "width: var(--page-width, 100vw) !important;"));
    QVERIFY(has(css, "height: var(--page-height, 100vh) !important;"));
    QVERIFY(has(css, "column-fill: auto !important;"));
    QVERIFY(has(css, "column-width: calc(var(--page-width, 100vw) - 5vw) !important;"));
    QVERIFY(has(css, "column-gap: 5vw !important;"));
    QVERIFY(!has(css, "column-count: 2"));
}

void ReaderCssTest::two_columns_only_apply_to_paged_horizontal_writing() {
    Appearance appearance = base();
    appearance.vertical_writing = false;
    appearance.two_column = true;
    QVERIFY(has(reader_css(appearance, page(), colors(), ""), "column-count: 2 !important;"));

    appearance.paragraph_mode = true;
    QVERIFY(!has(reader_css(appearance, page(), colors(), ""), "column-count: 2"));

    appearance.paragraph_mode = false;
    appearance.continuous = true;
    QVERIFY(!has(reader_css(appearance, page(), colors(), ""), "column-count: 2"));

    appearance.continuous = false;
    appearance.vertical_writing = true;
    QVERIFY(!has(reader_css(appearance, page(), colors(), ""), "column-count: 2"));
}

void ReaderCssTest::continuous_mode_removes_the_page_height() {
    Appearance appearance = base();
    appearance.continuous = true;
    const std::string css = reader_css(appearance, page(), colors(), "");

    QVERIFY(!has(css, "height: var(--page-height, 100vh) !important;"));
    QVERIFY(!has(css, "column-width:"));
    QVERIFY(has(css, "overflow-y: hidden !important;"));

    appearance.vertical_writing = false;
    QVERIFY(has(reader_css(appearance, page(), colors(), ""), "overflow-x: hidden !important;"));
}

void ReaderCssTest::padding_and_overlap_are_written() {
    Appearance appearance = base();
    appearance.horizontal_padding = 6;
    appearance.vertical_padding = 4;
    const std::string css = reader_css(appearance, page(), colors(), "");

    QVERIFY(has(css, "padding: 2vh 3vw !important;"));
    QVERIFY(has(css, "padding-bottom: calc(2vh + 22px) !important;"));
}

void ReaderCssTest::image_caps_follow_the_layout() {
    const std::string vertical = reader_css(base(), page(), colors(), "");
    QVERIFY(has(vertical, "max-width: 95vw !important;"));
    QVERIFY(has(vertical, "max-height: calc(100vh - 22px) !important;"));

    Appearance horizontal = base();
    horizontal.vertical_writing = false;
    const std::string css = reader_css(horizontal, page(), colors(), "");
    QVERIFY(has(css, "max-width: calc(var(--page-width, 100vw) - 5vw) !important;"));
    QVERIFY(has(css, "max-height: 100vh !important;"));

    horizontal.continuous = true;
    const std::string scrolling = reader_css(horizontal, page(), colors(), "");
    QVERIFY(has(scrolling, "max-width: 95vw !important;"));
    QVERIFY(has(scrolling, "max-height: none !important;"));
}

void ReaderCssTest::typography_switches_apply_only_when_advanced() {
    Appearance appearance = base();
    appearance.line_height = 1.8;
    appearance.character_spacing = 0.1;
    QVERIFY(!has(reader_css(appearance, page(), colors(), ""), "line-height: 1.8"));

    appearance.layout_advanced = true;
    const std::string css = reader_css(appearance, page(), colors(), "");
    QVERIFY(has(css, "line-height: 1.8 !important;"));
    QVERIFY(has(css, "letter-spacing: 0.001em !important;"));
    QVERIFY(has(css, "text-align: start !important;"));

    appearance.justify_text = true;
    QVERIFY(!has(reader_css(appearance, page(), colors(), ""), "text-align: start !important;"));
}

void ReaderCssTest::furigana_modes_change_the_rules() {
    Appearance appearance = base();
    const std::string off = reader_css(appearance, page(), colors(), "");
    QVERIFY(!has(off, "opacity: 0.4"));
    QVERIFY(has(off, "ruby.furigana-hidden > rt"));

    appearance.furigana_mode = "Dimmed";
    QVERIFY(has(reader_css(appearance, page(), colors(), ""), "ruby > rt, ruby > rp { opacity: 0.4 !important; }"));

    appearance.furigana_mode = "Toggle";
    const std::string toggle = reader_css(appearance, page(), colors(), "");
    QVERIFY(has(toggle, "ruby.furigana-hidden > rt {"));
    QVERIFY(has(toggle, "background-size: 0.14em 100% !important;"));

    appearance.vertical_writing = false;
    QVERIFY(has(reader_css(appearance, page(), colors(), ""),
                "background-size: 100% 0.14em !important;"));
}

void ReaderCssTest::paragraph_mode_breaks_before_paragraphs() {
    Appearance appearance = base();
    appearance.vertical_writing = false;
    appearance.paragraph_mode = true;
    appearance.layout_advanced = true;
    appearance.paragraph_spacing = 1.2;
    const std::string css = reader_css(appearance, page(), colors(), "");

    QVERIFY(has(css, "p.hoshi-paragraph {"));
    QVERIFY(has(css, "break-before: column !important;"));
    QVERIFY(has(css, "margin-top: 1.2em !important;"));
    QVERIFY(has(css, "font-kerning: none !important;"));
}

void ReaderCssTest::colors_and_font_are_inserted() {
    Appearance appearance = base();
    appearance.avoid_page_break = true;
    const std::string css = reader_css(appearance, page(), colors(), "@font-face { }");

    QVERIFY(has(css, "@font-face { }"));
    QVERIFY(has(css, "font-family: 'Noto Serif JP', serif !important;"));
    QVERIFY(has(css, "font-size: 22px !important;"));
    QVERIFY(has(css, "break-inside: avoid !important;"));
    QVERIFY(has(css, "--hoshi-sasayaki-text-color"));
}

void ReaderCssTest::spacers_follow_the_padding() {
    Appearance appearance = base();
    QVERIFY(has(reader_spacer_script(appearance), "spacer.style.height = 'calc(0vh + 22px)';"));

    appearance.vertical_padding = 4;
    QVERIFY(has(reader_spacer_script(appearance),
                "spacer.style.height = 'calc(2vh + 22px)';"));
    QVERIFY(has(reader_spacer_script(appearance), "spacer.style.width = '100%';"));

    Appearance horizontal = base();
    horizontal.vertical_writing = false;
    horizontal.horizontal_padding = 0;
    QCOMPARE(reader_spacer_script(horizontal), std::string(""));

    horizontal.horizontal_padding = 5;
    QVERIFY(has(reader_spacer_script(horizontal), "spacer.style.width = '2.5vw';"));
    QVERIFY(has(reader_spacer_script(horizontal), "spacer.style.height = '100%';"));
}

QTEST_APPLESS_MAIN(ReaderCssTest)
#include "tst_reader_css.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The chapter stylesheet, assembled in the order of
// NativeMac/NativeReaderView.swift (injectReader). The page size and the
// resolved colours reach the page as CSS variables set by the host, so the
// stylesheet only names those variables and never writes the literal values;
// Colors is therefore not read here. Column and image lengths come from
// services/reader/layout.h so their formulas live in one place.

#include "reader_css.h"

#include "layout.h"

#include <array>
#include <charconv>
#include <string>

namespace niratan::reader {
namespace {

// injectReader interpolates Swift numbers, which print in the shortest form
// that round-trips. CSS reads the same value either way.
std::string number(double value) {
    std::array<char, 32> buffer{};
    const std::to_chars_result result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

std::string px(double value) { return number(value) + "px"; }

std::string vw(double value) { return number(value) + "vw"; }

std::string vh(double value) { return number(value) + "vh"; }

// Models/Highlight.swift, HighlightColor.css. Injection order follows
// HighlightColor.allCases.
std::string highlight_colors() {
    struct Entry {
        const char* name;
        int red;
        int green;
        int blue;
    };
    constexpr Entry kEntries[] = {
        {"yellow", 239, 209, 56},
        {"green", 152, 220, 129},
        {"blue", 149, 185, 255},
        {"pink", 255, 155, 180},
        {"purple", 197, 175, 251},
    };

    std::string css;
    for (const Entry& entry : kEntries) {
        if (!css.empty()) css += "\n";
        css += ".hoshi-highlight-";
        css += entry.name;
        css += " { background-color: rgba(";
        css += std::to_string(entry.red);
        css += ", ";
        css += std::to_string(entry.green);
        css += ", ";
        css += std::to_string(entry.blue);
        css += ", 0.35) !important; }";
    }
    return css;
}

}  // namespace

std::string reader_css(const Appearance& appearance, const PageSize& page, const Colors& colors,
                       const std::string& font_face_css) {
    // The host writes --page-width, --page-height, --hoshi-text-color and the
    // sasayaki colours onto the document element, so these arguments are not
    // part of the stylesheet text.
    (void)page;
    (void)colors;

    const bool vertical = appearance.vertical_writing;
    const bool continuous = appearance.continuous;
    // UserConfig.paragraphMode and continuousMode cannot both hold; the guard
    // mirrors injectReader's paragraphMode.
    const bool paragraph_mode = appearance.paragraph_mode && !continuous;

    LayoutInput input;
    input.vertical_writing = vertical;
    input.continuous = continuous;
    input.paragraph_mode = paragraph_mode;
    input.two_column = appearance.two_column && !vertical && !continuous && !paragraph_mode;
    input.font_size = appearance.font_size;
    input.horizontal_padding = appearance.horizontal_padding;
    input.vertical_padding = appearance.vertical_padding;
    const PageLayout layout = page_layout(input);

    const bool spread = layout.column_count > 1;
    const std::string writing_mode = vertical ? "vertical-rl" : "horizontal-tb";

    std::string css;
    css += font_face_css;
    css += "\n";
    css += "@media (prefers-color-scheme: light) { :root { --hoshi-text-color: #000; } }\n";
    css += "@media (prefers-color-scheme: dark) { :root { --hoshi-text-color: #fff; } }\n";

    // globalSizingCss.
    if (continuous || !vertical) {
        css += "* {\n";
        css += "    max-width: 100% !important;\n";
        css += "    box-sizing: border-box !important;\n";
        css += "}\n";
    }

    css += "html, body {\n";
    css += "    margin: 0 !important;\n";
    css += "    padding: 0 !important;\n";
    css += "    color: var(--hoshi-text-color) !important;\n";
    css += "    writing-mode: " + writing_mode + " !important;\n";
    css += "    -webkit-writing-mode: " + writing_mode + " !important;\n";
    if (continuous) {
        // rootOverflowCss: the scroll axis depends on the writing direction.
        css += vertical ? "    overflow-y: hidden !important;\n"
                        : "    overflow-x: hidden !important;\n";
    } else {
        css += "    overflow: hidden !important;\n";
        css += "    width: var(--page-width, 100vw) !important;\n";
    }
    css += "}\n";

    // paginatedHtmlHeightCss.
    if (!continuous) {
        css += "html {\n";
        css += "    height: var(--page-height, 100vh) !important;\n";
        css += "}\n";
    }

    css += "body {\n";
    if (!continuous) {
        css += "    height: var(--page-height, 100vh) !important;\n";
    }
    css += "    font-family: '" + appearance.selected_font + "', serif !important;\n";
    css += "    font-size: " + px(appearance.font_size) + " !important;\n";
    css += "    -webkit-text-size-adjust: none !important;\n";
    css += "    box-sizing: border-box !important;\n";
    // bodyColumnCss: continuous reading has no page to paginate.
    if (!continuous) {
        css += "    column-width: " + layout.column_width + " !important;\n";
        css += "    column-gap: " + layout.column_gap + " !important;\n";
        if (spread) {
            css += "    column-count: 2 !important;\n";
            css += "    -webkit-column-count: 2 !important;\n";
        }
    }
    css += "    padding: " + layout.page_padding + " !important;\n";
    if (!layout.block_end_padding.empty()) {
        css += "    padding-bottom: " + layout.block_end_padding + " !important;\n";
    }
    // horizontalOverflowCss: horizontal writing breaks inside the column.
    if (!vertical) {
        css += "    column-fill: auto !important;\n";
        css += "    -webkit-column-fill: auto !important;\n";
        css += "    overflow-wrap: anywhere !important;\n";
        css += "    word-break: normal !important;\n";
        css += "    orphans: 1;\n";
        css += "    widows: 1;\n";
    }
    if (spread) {
        css += "    position: relative !important;\n";
    }
    // advancedCss.
    if (appearance.layout_advanced) {
        css += "    line-height: " + number(appearance.line_height) + " !important;\n";
        css += "    letter-spacing: " + number(appearance.character_spacing / 100.0) +
               "em !important;\n";
    }
    // gridCss. injectReader applies the start alignment in both writing
    // directions; vertical writing keeps the publisher's alignment here, which
    // tst_reader_css requires and which only changes the default alignment the
    // publisher already sets.
    if (!appearance.justify_text) {
        css += "    text-align: start !important;\n";
        css += "    hanging-punctuation: allow-end !important;\n";
        css += "    line-break: strict !important;\n";
    }
    css += "}\n";

    // publisherColumnResetCss: publisher column counts must not fight the page.
    if (!continuous) {
        css += "body * {\n";
        css += "    column-count: auto !important;\n";
        css += "    -webkit-column-count: auto !important;\n";
        css += "}\n";
    }

    // horizontalSpreadClipCss: hides the gutter between the two pages.
    if (spread) {
        css += "body::before,\n";
        css += "body::after {\n";
        css += "    content: \"\";\n";
        css += "    position: fixed;\n";
        css += "    top: 0;\n";
        css += "    bottom: 0;\n";
        css += "    width: " + vw(appearance.horizontal_padding / 2.0) + ";\n";
        css += "    background: var(--hoshi-reader-background-color);\n";
        css += "    pointer-events: none;\n";
        css += "    z-index: 2147483647;\n";
        css += "}\n";
        css += "body::before {\n";
        css += "    left: 0;\n";
        css += "}\n";
        css += "body::after {\n";
        css += "    right: 0;\n";
        css += "}\n";
    }

    // breakableTextCss.
    if (!vertical) {
        css += "p, div, span, li {\n";
        css += "    break-inside: auto !important;\n";
        css += "    -webkit-column-break-inside: auto !important;\n";
        css += "    overflow-wrap: anywhere !important;\n";
        css += "    word-break: normal !important;\n";
        css += "}\n";
        css += "pre, code {\n";
        css += "    white-space: pre-wrap !important;\n";
        css += "    overflow-wrap: anywhere !important;\n";
        css += "    word-break: break-word !important;\n";
        css += "}\n";
        css += "table {\n";
        css += "    table-layout: fixed !important;\n";
        css += "    width: 100% !important;\n";
        css += "    overflow-wrap: anywhere !important;\n";
        css += "    word-break: break-word !important;\n";
        css += "}\n";
    }

    css += "img.block-img {\n";
    css += "    max-width: " + layout.image_max_width + " !important;\n";
    css += "    max-height: " + layout.image_max_height + " !important;\n";
    css += "    width: auto !important;\n";
    css += "    height: auto !important;\n";
    css += "    display: block !important;\n";
    css += "    margin: auto !important;\n";
    css += "    break-inside: avoid !important;\n";
    css += "    -webkit-column-break-inside: avoid !important;\n";
    css += "    object-fit: contain !important;\n";
    css += "}\n";
    css += "svg {\n";
    css += "    max-width: " + layout.image_max_width + " !important;\n";
    css += "    max-height: " + layout.image_max_height + " !important;\n";
    css += "    width: 100% !important;\n";
    css += "    height: 100% !important;\n";
    css += "    display: block !important;\n";
    css += "    margin: auto !important;\n";
    css += "    break-inside: avoid !important;\n";
    css += "    -webkit-column-break-inside: avoid !important;\n";
    css += "}\n";
    // The wrapper and blur rules are always present; blurImages only decides
    // whether the host adds the class.
    css += ".blur-wrapper {\n";
    css += "    display: table;\n";
    css += "    margin: auto;\n";
    css += "    line-height: 0;\n";
    css += "    overflow: hidden;\n";
    css += "}\n";
    css += "img.block-img.blurred,\n";
    css += "svg.blurred {\n";
    css += "    filter: blur(24px) !important;\n";
    css += "    clip-path: inset(0);\n";
    css += "}\n";
    css += "::highlight(hoshi-selection) {\n";
    css += "    background-color: rgba(160, 160, 160, 0.4) !important;\n";
    css += "    color: inherit;\n";
    css += "}\n";
    css += ".hoshi-sasayaki-cue.hoshi-sasayaki-active {\n";
    css += "    color: var(--hoshi-sasayaki-text-color) !important;\n";
    css += "    background-color: var(--hoshi-sasayaki-background-color) !important;\n";
    css += "}\n";
    css += "ruby > rt, ruby > rp { -webkit-user-select: none; }\n";

    // The rules are inert until the Toggle script adds the class, so
    // injectReader always emits them, in both writing directions.
    css += "ruby.furigana-hidden > rp,\n";
    css += "ruby.furigana-hidden > rt > * {\n";
    css += "    visibility: hidden !important;\n";
    css += "}\n";
    // dimmedFuriganaCss.
    if (appearance.furigana_mode == "Dimmed") {
        css += "ruby > rt, ruby > rp { opacity: 0.4 !important; }\n";
    }
    // hiddenFuriganaCss: the bar replaces the reading along the ruby axis.
    {
        css += "ruby.furigana-hidden > rt {\n";
        css += "    color: transparent !important;\n";
        css += "    background-image: linear-gradient(rgba(150, 150, 150, 0.75), rgba(150, 150, "
               "150, 0.75)) !important;\n";
        if (vertical) {
            css += "    background-size: 0.14em 100% !important;\n";
            css += "    background-position: left center !important;\n";
        } else {
            css += "    background-size: 100% 0.14em !important;\n";
            css += "    background-position: center bottom !important;\n";
        }
        css += "    background-repeat: no-repeat !important;\n";
        css += "}\n";
    }

    css += highlight_colors() + "\n";

    // pageBreakCss.
    if (appearance.avoid_page_break) {
        css += "p {\n";
        css += "    break-inside: avoid !important;\n";
        css += "    -webkit-column-break-inside: avoid !important;\n";
        css += "}\n";
    }
    // paragraphSpacingCss: only advanced layout changes paragraph margins, and
    // vertical writing spends them on the inline axis.
    if (appearance.layout_advanced) {
        if (vertical) {
            css += "p {\n";
            css += "    margin-right: " + number(appearance.paragraph_spacing) + "em !important;\n";
            css += "    margin-left: " + number(appearance.paragraph_spacing) + "em !important;\n";
            css += "}\n";
        } else {
            css += "p {\n";
            css += "    margin-top: " + number(appearance.paragraph_spacing) + "em !important;\n";
            css += "    margin-bottom: " + number(appearance.paragraph_spacing) + "em !important;\n";
            css += "}\n";
        }
    }
    // paragraphModeCss.
    if (paragraph_mode) {
        css += "body {\n";
        css += "    column-fill: auto !important;\n";
        css += "    -webkit-column-fill: auto !important;\n";
        css += "    font-kerning: none !important;\n";
        css += "}\n";
        css += "p.hoshi-paragraph {\n";
        css += "    margin-block-start: 0 !important;\n";
        css += "    break-before: column !important;\n";
        css += "    -webkit-column-break-before: always !important;\n";
        css += "}\n";
        css += "::highlight(hoshi-animation) {\n";
        css += "    color: transparent !important;\n";
        css += "}\n";
    }

    return css;
}

std::string reader_spacer_script(const Appearance& appearance) {
    const int overlap = appearance.vertical_writing ? appearance.font_size : 0;
    std::string script;
    if (appearance.vertical_writing) {
        if (appearance.vertical_padding <= 0 && overlap <= 0) return {};
        script += "var spacer = document.createElement('div');\n";
        script += "spacer.style.height = 'calc(" + vh(appearance.vertical_padding / 2.0) + " + " +
                  px(overlap) + ")';";
        script += "\n";
        script += "spacer.style.width = '100%';\n";
    } else {
        if (appearance.horizontal_padding <= 0) return {};
        script += "var spacer = document.createElement('div');\n";
        script += "spacer.style.height = '100%';\n";
        script += "spacer.style.width = '" + vw(appearance.horizontal_padding / 2.0) + "';\n";
    }
    script += "spacer.style.display = 'block';\n";
    script += "spacer.style.breakInside = 'avoid';\n";
    script += "document.body.appendChild(spacer);\n";
    return script;
}

}  // namespace niratan::reader

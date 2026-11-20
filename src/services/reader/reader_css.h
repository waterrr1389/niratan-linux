// SPDX-License-Identifier: GPL-3.0-or-later
//
// The stylesheet the reader injects into a chapter, following
// NativeMac/NativeReaderView.swift (injectReader) together with
// services/reader/layout.h for the column and image formulas.
//
// Every rule below exists in injectReader with the same values and the same
// conditions. The strings are CSS text, not validated or normalized: the
// caller supplies the font family, the page size in pixels and the resolved
// colours. Colours are written verbatim.
//
// Layout rules:
//   - vertical writing always uses one column; the two column spread applies
//     only to paged horizontal writing outside paragraph mode;
//   - paragraph mode is only reachable while continuous mode is off;
//   - the block-end padding and spacer exist only where vertical writing
//     needs the overlap, which is the font size;
//   - the furigana rules depend on the resolved mode (Off, Dimmed, Toggle,
//     Hidden) and on the writing direction.

#pragma once

#include <string>

namespace niratan::reader {

struct Appearance {
    bool vertical_writing = true;
    bool continuous = false;
    // The caller must already have cleared this in continuous mode; the CSS
    // only decides whether the paragraph rules apply.
    bool paragraph_mode = false;
    bool two_column = false;
    bool avoid_page_break = false;
    bool justify_text = false;
    bool layout_advanced = false;
    bool blur_images = false;
    std::string selected_font;
    int font_size = 22;
    int horizontal_padding = 5;
    int vertical_padding = 0;
    double line_height = 1.65;
    double character_spacing = 0.0;
    double paragraph_spacing = 0.0;
    // Resolved FuriganaMode raw value: "Off", "Dimmed", "Toggle" or "Hidden".
    std::string furigana_mode = "Off";
};

struct PageSize {
    int width = 0;
    int height = 0;
};

struct Colors {
    // Empty text leaves the stylesheet's prefers-color-scheme default alone.
    std::string text;
    std::string background;
    std::string sasayaki_text;
    std::string sasayaki_background;
};

// The style block contents. font_face_css is inserted first and may be empty.
std::string reader_css(const Appearance& appearance, const PageSize& page, const Colors& colors,
                       const std::string& font_face_css);

// The spacer element appended to the body, or empty when the padding needs
// none. Returned as a statement block that declares `spacer`.
std::string reader_spacer_script(const Appearance& appearance);

}  // namespace niratan::reader

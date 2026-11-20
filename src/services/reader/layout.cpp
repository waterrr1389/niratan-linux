// SPDX-License-Identifier: GPL-3.0-or-later

#include "layout.h"

#include <string>

namespace niratan::reader {
namespace {

// NativeReaderView.injectReader, horizontalSpreadColumnGap.
constexpr int kSpreadColumnGap = 32;

std::string px(int value) { return std::to_string(value) + "px"; }

std::string vw(int value) { return std::to_string(value) + "vw"; }

std::string vh(int value) { return std::to_string(value) + "vh"; }

}  // namespace

PageLayout page_layout(const LayoutInput& input) {
    PageLayout layout;
    layout.page_padding = vh(input.vertical_padding / 2) + " " + vw(input.horizontal_padding / 2);

    // bottomOverlap is the font size while writing vertically and zero
    // otherwise, so the vertical gap and the block-end padding share it.
    const int overlap = input.vertical_writing ? input.font_size : 0;
    if (input.vertical_writing && overlap > 0) {
        layout.block_end_padding =
            "calc(" + vh(input.vertical_padding / 2) + " + " + px(overlap) + ")";
    }

    // imgWidth / imgHeight. Continuous horizontal reading caps the width only;
    // continuous vertical reading caps the height only.
    if (input.vertical_writing) {
        layout.image_max_width = vw(100 - input.horizontal_padding);
        layout.image_max_height =
            "calc(" + vh(100 - input.vertical_padding) + " - " +
            std::to_string(overlap * (100 - input.vertical_padding) / 100) + "px)";
    } else if (input.continuous) {
        layout.image_max_width = vw(100 - input.horizontal_padding);
        layout.image_max_height = "none";
    } else {
        layout.image_max_height = vh(100 - input.vertical_padding);
    }

    if (input.continuous) return layout;

    const bool spread = input.two_column && !input.vertical_writing && !input.paragraph_mode;
    if (input.vertical_writing) {
        layout.column_width = "var(--page-height, 100vh)";
        layout.column_gap = "calc(" + vh(input.vertical_padding) + " + " + px(overlap) + ")";
    } else if (spread) {
        layout.column_count = 2;
        layout.column_width =
            "max(1px, calc((var(--page-width, 100vw) - " + vw(input.horizontal_padding) + " - " +
            px(kSpreadColumnGap) + ") / 2))";
        layout.column_gap = px(kSpreadColumnGap);
    } else {
        layout.column_width =
            "calc(var(--page-width, 100vw) - " + vw(input.horizontal_padding) + ")";
        layout.column_gap = vw(input.horizontal_padding);
    }
    if (!input.vertical_writing) layout.image_max_width = layout.column_width;
    return layout;
}

}  // namespace niratan::reader

// SPDX-License-Identifier: GPL-3.0-or-later

#include "layout.h"

#include <array>
#include <charconv>
#include <string>

namespace niratan::reader {
namespace {

// NativeReaderView.injectReader, horizontalSpreadColumnGap.
constexpr int kSpreadColumnGap = 32;

// injectReader converts the paddings to Double before dividing, so halves keep
// their fraction. CSS reads "5" and "5.0" alike, so the shortest round-trip
// form is enough.
std::string number(double value) {
    std::array<char, 32> buffer{};
    const std::to_chars_result result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

std::string px(double value) { return number(value) + "px"; }

std::string vw(double value) { return number(value) + "vw"; }

std::string vh(double value) { return number(value) + "vh"; }

// Vertical cap shared by paged and continuous vertical writing: the full page
// height minus the overlap share that the block-end padding also reserves.
std::string vertical_image_height(int vertical_padding, int overlap) {
    const double remaining = 100.0 - vertical_padding;
    return "calc(" + vh(remaining) + " - " + px(overlap * remaining / 100.0) + ")";
}

}  // namespace

PageLayout page_layout(const LayoutInput& input) {
    PageLayout layout;
    layout.page_padding =
        vh(input.vertical_padding / 2.0) + " " + vw(input.horizontal_padding / 2.0);

    // bottomOverlap is the font size while writing vertically and zero
    // otherwise, so the vertical gap and the block-end padding share it.
    const int overlap = input.vertical_writing ? input.font_size : 0;
    if (input.vertical_writing && overlap > 0) {
        layout.block_end_padding =
            "calc(" + vh(input.vertical_padding / 2.0) + " + " + px(overlap) + ")";
    }

    // imgWidth / imgHeight. Continuous reading caps one axis only: a
    // horizontal flow has no width boundary, a vertical one has no height
    // boundary.
    if (input.vertical_writing) {
        layout.image_max_width = input.continuous ? "none" : vw(100 - input.horizontal_padding);
        layout.image_max_height = vertical_image_height(input.vertical_padding, overlap);
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

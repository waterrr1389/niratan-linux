// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reader page layout. The formulas come from NativeMac/NativeReaderView.swift,
// injectReader. Every value is a CSS length expression, because the page size
// is only known inside the web view; this function decides the expression,
// not the pixels.
//
// Two columns apply only to horizontal paged reading. Vertical writing,
// continuous scrolling and paragraph mode all force a single column, matching
// the horizontalPageColumns condition. The vertical column width is the page
// height, not the page width: a vertical column runs down the page.
//
// injectReader converts the paddings to Double before dividing, so halves
// keep their fraction: a padding of 5 gives 2.5vw. Numbers are printed in the
// shortest decimal form that round-trips (std::to_chars), so 5 prints as "5"
// where Swift prints "5.0"; CSS reads both the same. The vertical image
// height subtracts overlap * (100 - vertical_padding) / 100 pixels, computed
// in double in that order.
//
// Image caps per mode (imgWidth, imgHeight):
//   paged horizontal       width = column width     height = (100 - vp)vh
//   paged vertical         width = (100 - hp)vw     height = vertical cap
//   continuous horizontal  width = (100 - hp)vw     height = none
//   continuous vertical    width = none             height = vertical cap

#pragma once

#include <string>

namespace niratan::reader {

struct LayoutInput {
    bool vertical_writing = false;
    bool continuous = false;
    bool paragraph_mode = false;
    bool two_column = false;
    int font_size = 22;
    int horizontal_padding = 5;
    int vertical_padding = 0;
};

struct PageLayout {
    // Empty in continuous mode, which does not paginate.
    std::string column_width;
    std::string column_gap;
    int column_count = 1;
    std::string page_padding;
    // Extra padding on the block-end edge. Empty unless vertical writing needs
    // room for the overlap.
    std::string block_end_padding;
    std::string image_max_width;
    std::string image_max_height;
};

PageLayout page_layout(const LayoutInput& input);

}  // namespace niratan::reader

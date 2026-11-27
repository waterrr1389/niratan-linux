// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/layout.h"

#include <QTest>

using niratan::reader::LayoutInput;
using niratan::reader::page_layout;

// Cases follow NativeReaderView.injectReader: horizontalPageColumns,
// columnWidth, columnGap, bottomPaddingCss, imgWidth, imgHeight.
class LayoutTest : public QObject {
    Q_OBJECT

   private slots:
    void horizontal_page_is_one_column();
    void two_columns_only_when_paging_horizontally();
    void vertical_column_runs_down_the_page();
    void continuous_reading_does_not_paginate();
    void continuous_vertical_caps_height_only();
    void padding_splits_evenly();
};

LayoutInput base() {
    LayoutInput input;
    input.font_size = 22;
    input.horizontal_padding = 5;
    input.vertical_padding = 4;
    return input;
}

void LayoutTest::horizontal_page_is_one_column() {
    const auto layout = page_layout(base());
    QCOMPARE(layout.column_count, 1);
    QCOMPARE(layout.column_width, std::string("calc(var(--page-width, 100vw) - 5vw)"));
    QCOMPARE(layout.column_gap, std::string("5vw"));
    QCOMPARE(layout.block_end_padding, std::string(""));
    QCOMPARE(layout.image_max_width, layout.column_width);
    QCOMPARE(layout.image_max_height, std::string("96vh"));
}

void LayoutTest::two_columns_only_when_paging_horizontally() {
    auto input = base();
    input.two_column = true;
    const auto spread = page_layout(input);
    QCOMPARE(spread.column_count, 2);
    QCOMPARE(spread.column_gap, std::string("32px"));
    QCOMPARE(spread.column_width,
             std::string("max(1px, calc((var(--page-width, 100vw) - 5vw - 32px) / 2))"));

    input.vertical_writing = true;
    QCOMPARE(page_layout(input).column_count, 1);

    input.vertical_writing = false;
    input.continuous = true;
    QCOMPARE(page_layout(input).column_count, 1);

    input.continuous = false;
    input.paragraph_mode = true;
    QCOMPARE(page_layout(input).column_count, 1);
}

void LayoutTest::vertical_column_runs_down_the_page() {
    auto input = base();
    input.vertical_writing = true;
    const auto layout = page_layout(input);
    QCOMPARE(layout.column_width, std::string("var(--page-height, 100vh)"));
    QCOMPARE(layout.column_gap, std::string("calc(4vh + 22px)"));
    QCOMPARE(layout.block_end_padding, std::string("calc(2vh + 22px)"));
    QCOMPARE(layout.image_max_width, std::string("95vw"));
    QCOMPARE(layout.image_max_height, std::string("calc(96vh - 21.12px)"));

    input.vertical_padding = 3;
    QCOMPARE(page_layout(input).block_end_padding, std::string("calc(1.5vh + 22px)"));
}

void LayoutTest::continuous_vertical_caps_height_only() {
    auto input = base();
    input.vertical_writing = true;
    input.continuous = true;
    const auto layout = page_layout(input);
    QCOMPARE(layout.column_width, std::string(""));
    QCOMPARE(layout.image_max_width, std::string("none"));
    QCOMPARE(layout.image_max_height, std::string("calc(96vh - 21.12px)"));
}

void LayoutTest::continuous_reading_does_not_paginate() {
    auto input = base();
    input.continuous = true;
    const auto layout = page_layout(input);
    QCOMPARE(layout.column_width, std::string(""));
    QCOMPARE(layout.column_count, 1);
    QCOMPARE(layout.image_max_width, std::string("95vw"));
    QCOMPARE(layout.image_max_height, std::string("none"));
}

void LayoutTest::padding_splits_evenly() {
    auto input = base();
    input.horizontal_padding = 5;
    input.vertical_padding = 3;
    QCOMPARE(page_layout(input).page_padding, std::string("1.5vh 2.5vw"));

    input.horizontal_padding = 4;
    input.vertical_padding = 0;
    QCOMPARE(page_layout(input).page_padding, std::string("0vh 2vw"));
}

QTEST_APPLESS_MAIN(LayoutTest)
#include "tst_layout.moc"

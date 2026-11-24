// SPDX-License-Identifier: GPL-3.0-or-later
//
// A value over its caption, following Features/Reader/StatisticsView.swift
// `heroMetric`: the value in headline with tabular digits on one line, the
// label in caption2 secondary, 1 apart and leading-aligned. The value shrinks
// to fit its width rather than eliding.

import QtQuick
import Niratan

Column {
    id: metric

    property string value
    property string label

    spacing: Theme.metricSpacing

    StyledLabel {
        width: metric.width
        text: metric.value
        textStyle: Theme.headlineFont
        monospacedDigits: true
        maximumLineCount: 1
        // Shrinks to fit, as heroMetric's minimumScaleFactor(0.7), so a
        // number is never cut.
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: Math.round(Theme.headlineFont.pixelSize * Theme.metricMinimumScale)
    }

    StyledLabel {
        width: metric.width
        text: metric.label
        textStyle: Theme.caption2Font
        secondary: true
        elide: Text.ElideRight
    }
}

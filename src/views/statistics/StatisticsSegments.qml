// SPDX-License-Identifier: GPL-3.0-or-later
//
// The segmented control used by the statistics dashboard for the range, time
// grain, metric, style, target type and ranking pickers of
// Features/Bookshelf/StatisticsDashboardView.swift. It is the kit's
// SegmentedPicker, so these pickers look like every other segmented choice.

import QtQuick
import Niratan

SegmentedPicker {
    property var labels: []

    model: labels
}

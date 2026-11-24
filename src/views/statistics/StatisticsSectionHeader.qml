// SPDX-License-Identifier: GPL-3.0-or-later
//
// The title-over-detail heading of a statistics card, following
// `sectionHeader` in Features/Bookshelf/StatisticsDashboardView.swift: a
// semibold title over a secondary detail line, in the kit's card heading
// roles.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: header

    property string title
    property string detail

    spacing: Theme.dashboardHeaderSpacing

    StyledLabel {
        Layout.fillWidth: true
        text: header.title
        textStyle: Theme.title3Font
        weight: Theme.weightSemibold
        elide: Text.ElideRight
    }

    StyledLabel {
        Layout.fillWidth: true
        visible: header.detail.length > 0
        text: header.detail
        textStyle: Theme.subheadlineFont
        secondary: true
        elide: Text.ElideRight
    }
}

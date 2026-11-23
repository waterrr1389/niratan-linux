// SPDX-License-Identifier: GPL-3.0-or-later
//
// The title-over-detail heading of a statistics card, following the section
// headers in Features/Bookshelf/StatisticsDashboardView.swift: title2
// semibold over a secondary callout.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: header

    property string title
    property string detail
    spacing: 3

    Label {
        Layout.fillWidth: true
        text: header.title
        color: Theme.textPrimary
        font.pixelSize: 16
        font.bold: true
        elide: Text.ElideRight
    }
    Label {
        Layout.fillWidth: true
        visible: header.detail.length > 0
        text: header.detail
        color: Theme.textSecondary
        font.pixelSize: 12
        elide: Text.ElideRight
    }
}

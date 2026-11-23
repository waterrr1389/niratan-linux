// SPDX-License-Identifier: GPL-3.0-or-later
//
// The card surface of the statistics dashboard, following the
// `nativeGlassCardSurface` cards in
// Features/Bookshelf/StatisticsDashboardView.swift: a solid theme surface with
// a one-pixel stroke and the same 18 px inset around the content.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

Rectangle {
    id: card

    default property alias content: body.data
    radius: Theme.cardRadius
    color: Theme.cardBackground
    border.width: 1
    border.color: Theme.cardStroke
    implicitWidth: body.implicitWidth + 36
    implicitHeight: body.implicitHeight + 36

    ColumnLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 18
        spacing: 10
    }
}

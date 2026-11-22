// SPDX-License-Identifier: GPL-3.0-or-later
//
// The header of every reader side panel, following
// NativeMac/NativeReaderInspector.swift `NativeReaderInspectorHeader`: a leading
// semibold title, an optional one-line secondary subtitle, an optional
// accessory control and a 28 px xmark Close button with an 11 px symbol.
// Spacing is 8 inside, 2 between title and subtitle, and the insets are 18
// horizontal, 16 top and 10 bottom.
//
// `inspectorTitle` and `inspectorCloseButton` are the names the page-level
// tests address; the default property places accessories before Close.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: header

    property string title
    property string subtitle
    default property alias accessory: accessoryRow.data

    signal closeRequested()

    implicitHeight: Math.max(closeButton.implicitHeight, titles.implicitHeight) + 16 + 10

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.topMargin: 16
        anchors.bottomMargin: 10
        spacing: 8

        ColumnLayout {
            id: titles
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Text {
                objectName: "inspectorTitle"
                Layout.fillWidth: true
                text: header.title
                color: Theme.textPrimary
                font.pixelSize: 20
                font.bold: true
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                Layout.fillWidth: true
                visible: header.subtitle.length > 0
                text: header.subtitle
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        Row {
            id: accessoryRow
            Layout.alignment: Qt.AlignVCenter
            spacing: 8
        }

        IconButton {
            id: closeButton
            objectName: "inspectorCloseButton"
            icon: "xmark"
            diameter: 28
            iconSize: 11
            accessibleName: qsTr("Close")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: header.closeRequested()
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// An inspector header. Follows NativeMac/NativeReaderInspector.swift
// NativeReaderInspectorHeader: a title3 semibold title with an optional one-
// line caption subtitle 2 below, the trailing `accessory` items, then a close
// NativeGlassCircleButton (xmark, diameter 28, glyph 11) emitting
// closeRequested; items 8 apart, padding 18 at the sides, 16 on top and 10
// below.
import QtQuick
import Niratan

Item {
    id: header

    property string title
    property string subtitle
    default property alias accessory: accessoryRow.data

    signal closeRequested()

    width: parent ? parent.width : implicitWidth
    implicitWidth: 2 * Theme.panelHeaderPaddingHorizontal + titles.implicitWidth
                   + Theme.panelHeaderSpacing + trailing.implicitWidth
    implicitHeight: Theme.panelHeaderPaddingTop + Math.max(titles.implicitHeight, trailing.implicitHeight)
                    + Theme.panelHeaderPaddingBottom

    Column {
        id: titles

        x: Theme.panelHeaderPaddingHorizontal
        y: Theme.panelHeaderPaddingTop + (content.height - height) / 2
        width: Math.max(0, trailing.x - Theme.panelHeaderSpacing - x)
        spacing: Theme.panelHeaderTitleSpacing

        StyledLabel {
            objectName: "inspectorTitle"
            width: titles.width
            text: header.title
            textStyle: Theme.title3Font
            weight: Theme.weightSemibold
            elide: Text.ElideRight
        }

        StyledLabel {
            visible: header.subtitle !== ""
            width: titles.width
            text: header.subtitle
            textStyle: Theme.captionFont
            secondary: true
            elide: Text.ElideRight
        }
    }

    // The content band between the paddings, which both sides centre in.
    Item {
        id: content

        y: Theme.panelHeaderPaddingTop
        width: header.width
        height: header.height - Theme.panelHeaderPaddingTop - Theme.panelHeaderPaddingBottom
    }

    Row {
        id: trailing

        x: header.width - Theme.panelHeaderPaddingHorizontal - width
        y: Theme.panelHeaderPaddingTop + (content.height - height) / 2
        spacing: Theme.panelHeaderSpacing

        Row {
            id: accessoryRow

            visible: children.length > 0
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.panelHeaderSpacing
        }

        IconButton {
            objectName: "inspectorCloseButton"
            anchors.verticalCenter: parent.verticalCenter
            icon: "xmark"
            diameter: Theme.closeButtonDiameter
            iconSize: Theme.closeButtonGlyph
            accessibleName: qsTr("Close")
            onClicked: header.closeRequested()
        }
    }
}

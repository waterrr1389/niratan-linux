// SPDX-License-Identifier: GPL-3.0-or-later
//
// The open menu of a MenuPicker over a long list such as the installed font
// families. Follows NativeMac/NativeReuseViews.swift NativeGlassMenuPicker
// like the regular picker menu and keeps its look: the style's floating
// surface, compact MenuItem rows and a trailing checkmark on the current row.
// Rows come from a ListView bound to the model only while the popup is open,
// so a closed picker holds no rows and an open one only those in view.
// activated is emitted when the user chooses a row with the mouse or with
// Return, Enter or Space; opening, scrolling and dismissing choose nothing.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import Niratan
import NiratanStyle as Style

T.Popup {
    id: popup

    property var model: []
    property int currentIndex: -1
    property real minimumWidth: 240

    // The row list, for tests that count the rows created.
    readonly property ListView listView: list

    signal activated(int index)

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    height: Math.min(implicitHeight,
                     (parent?.Window.window?.height ?? implicitHeight) - topMargin - bottomMargin)

    margins: 0
    padding: 5
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    function choose(index) {
        if (index < 0 || index >= list.count)
            return
        popup.close()
        popup.activated(index)
    }

    onOpened: {
        list.currentIndex = Math.max(0, popup.currentIndex)
        list.positionViewAtIndex(list.currentIndex, ListView.Center)
        list.forceActiveFocus()
    }

    contentItem: ListView {
        id: list

        // Rows exist only in view, so the width cannot follow the widest
        // row as a Menu does; it follows the picker with a floor that fits
        // a typical family name.
        implicitWidth: Math.max(popup.minimumWidth, popup.parent?.width ?? 0)
        implicitHeight: contentHeight
        model: popup.visible ? popup.model : []
        clip: true
        cacheBuffer: 0
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: true
        highlightFollowsCurrentItem: false

        Keys.onReturnPressed: popup.choose(list.currentIndex)
        Keys.onEnterPressed: popup.choose(list.currentIndex)
        Keys.onSpacePressed: popup.choose(list.currentIndex)

        T.ScrollIndicator.vertical: ScrollIndicator {}

        delegate: MenuItem {
            id: item

            required property int index
            required property var modelData

            width: ListView.view.width
            text: String(item.modelData)
            checked: item.index === popup.currentIndex
            highlighted: item.ListView.isCurrentItem
            focusPolicy: Qt.NoFocus
            leftPadding: Theme.spacingMd
            rightPadding: Theme.spacingMd + Theme.pickerGlyphSize + Theme.spacingSm
            onHoveredChanged: if (item.hovered) list.currentIndex = item.index
            onClicked: popup.choose(item.index)

            indicator: Icon {
                x: item.width - width - Theme.spacingMd
                y: item.topPadding + (item.availableHeight - height) / 2
                size: Theme.pickerGlyphSize
                name: "checkmark"
                color: item.highlighted ? Theme.textOnAccent : Theme.textPrimary
                visible: item.checked
            }
        }
    }

    background: Style.FloatingBackground {
        implicitWidth: 180
        implicitHeight: 24
    }
}

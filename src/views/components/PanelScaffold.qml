// SPDX-License-Identifier: GPL-3.0-or-later
//
// A vertically scrolling column of sections. Follows
// NativeMac/NativeReuseViews.swift NativeSettingsForm.cardsBody (cards: the
// content min(width - 48, 840) wide and centred, padding 24 by 18, sections
// 22 apart) and Features/Video/VideoInspectorView.swift body (compact:
// padding 14 at the sides and top and 18 at the bottom, sections 20 apart,
// with an optional `header` such as PanelHeader above the scroll area and a
// divider under it).
//
// Content taller than the viewport scrolls, and scrolled to the end the last
// section keeps the bottom padding. Content never scrolls sideways: the
// column is as wide as the padding leaves and its sections fill it. The
// scroll bar shows only while the content is taller than the viewport.
import QtQuick
import QtQuick.Controls
import Niratan

Item {
    id: scaffold

    property bool compact: false
    // An item shown above the scroll area, compact panels only.
    property Item header: null
    default property alias content: columnItem.data

    readonly property Flickable flickable: flick
    // The column holding the sections.
    readonly property Item column: columnItem
    readonly property ScrollBar scrollBar: bar

    readonly property real topPadding: compact ? Theme.panelPaddingTop : Theme.formPaddingVertical
    readonly property real bottomPadding: compact ? Theme.panelPaddingBottom : Theme.formPaddingVertical
    readonly property bool showsHeader: compact && header !== null

    function attachHeader() {
        if (header) {
            header.parent = headerSlot
            header.visible = Qt.binding(() => scaffold.showsHeader)
            header.width = Qt.binding(() => headerSlot.width)
        }
    }

    onHeaderChanged: attachHeader()
    Component.onCompleted: attachHeader()

    Item {
        id: headerSlot

        width: scaffold.width
        height: scaffold.showsHeader ? scaffold.header.height : 0
    }

    Rectangle {
        id: headerDivider

        visible: scaffold.showsHeader
        y: headerSlot.height
        width: scaffold.width
        height: visible ? Theme.hairlineWidth : 0
        color: Theme.separator
    }

    Flickable {
        id: flick

        y: headerSlot.height + headerDivider.height
        width: scaffold.width
        height: scaffold.height - y
        clip: true
        contentWidth: width
        contentHeight: columnItem.height + scaffold.topPadding + scaffold.bottomPadding
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            id: bar

            visible: flick.contentHeight > flick.height
        }

        Column {
            id: columnItem

            width: scaffold.compact
                   ? Math.max(0, flick.width - 2 * Theme.panelPaddingHorizontal)
                   : Math.min(Math.max(0, flick.width - 2 * Theme.formPaddingHorizontal),
                              Theme.formMaxContentWidth)
            x: (flick.width - width) / 2
            y: scaffold.topPadding
            spacing: scaffold.compact ? Theme.panelSpacing : Theme.formSpacing

            // Every section fills the column, so none is wider than the
            // viewport and none is skipped for having no width.
            onChildrenChanged: {
                for (const child of children)
                    child.width = Qt.binding(() => columnItem.width)
            }
        }
    }
}

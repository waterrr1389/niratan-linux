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
// scroll bar shows only while the content is taller than the viewport. A
// persistent scroll bar gets its own gutter: the column narrows by the bar's
// width plus a small gap so trailing controls never sit under it.
import QtQuick
import QtQuick.Controls
import Niratan

Item {
    id: scaffold

    property bool compact: false
    // Keeps the scroll bar shown while the content overflows, for floating
    // panels where it is the only sign that more lies below.
    property bool persistentScrollBar: false
    // An item shown above the scroll area, compact panels only.
    property Item header: null
    default property alias content: columnItem.data

    readonly property Flickable flickable: flick
    // The column holding the sections.
    readonly property Item column: columnItem
    readonly property ScrollBar scrollBar: bar

    // A reader side panel overrides these with its own Swift insets.
    property real topPadding: compact ? Theme.panelPaddingTop : Theme.formPaddingVertical
    property real bottomPadding: compact ? Theme.panelPaddingBottom : Theme.formPaddingVertical
    property real horizontalPadding: compact ? Theme.panelPaddingHorizontal
                                             : Theme.formPaddingHorizontal
    property real spacing: compact ? Theme.panelSpacing : Theme.formSpacing
    readonly property bool showsHeader: compact && header !== null
    readonly property real scrollBarGutter: persistentScrollBar && bar.visible
                                            ? bar.width + Theme.spacingXs : 0

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
        // A desktop scroll view scrolls by wheel, touchpad and scroll bar and
        // never drags with the mouse. Taking no mouse button also keeps a
        // press after a wheel scroll from being spent on stopping the scroll,
        // which left the first click on a row without effect.
        acceptedButtons: Qt.NoButton

        ScrollBar.vertical: ScrollBar {
            id: bar

            visible: flick.contentHeight > flick.height
            policy: scaffold.persistentScrollBar && visible ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        }

        Column {
            id: columnItem

            width: scaffold.compact
                   ? Math.max(0, flick.width - scaffold.scrollBarGutter
                                 - 2 * scaffold.horizontalPadding)
                   : Math.min(Math.max(0, flick.width - scaffold.scrollBarGutter
                                          - 2 * scaffold.horizontalPadding),
                              Theme.formMaxContentWidth)
            x: (flick.width - scaffold.scrollBarGutter - width) / 2
            y: scaffold.topPadding
            spacing: scaffold.spacing

            // Every section fills the column, so none is wider than the
            // viewport and none is skipped for having no width.
            onChildrenChanged: {
                for (const child of children)
                    child.width = Qt.binding(() => columnItem.width)
            }
        }
    }
}

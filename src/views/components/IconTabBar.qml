// SPDX-License-Identifier: GPL-3.0-or-later
//
// A strip of equal tabs, each a glyph above a caption, in one rounded track,
// following Features/Video/VideoInspectorView.swift `VideoInspectorTabBar`:
// caption semibold titles, the selected tab in the accent on a raised pill,
// the others secondary. A click selects at once.
//
// `tabs` lists { key, title, icon } with SF Symbol icons. Each tab item takes
// `itemName` as its object name and exposes `tab`, `title`, `icon`,
// `selected` and a clicked() signal for tests.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Rectangle {
    id: strip

    property var tabs: []
    property int selectedTab: 0
    property string itemName

    signal tabSelected(int tab)

    implicitHeight: 2 * Theme.tabTrackPadding + 2 * Theme.tabItemPaddingVertical
                    + Theme.tabGlyphBox + Theme.tabItemSpacing + caption.height
    radius: Theme.tabTrackRadius
    color: Theme.inactiveSelection

    FontMetrics {
        id: caption

        font.pixelSize: Theme.captionFont.pixelSize
        font.weight: Theme.weightSemibold
    }

    Row {
        anchors.fill: parent
        anchors.margins: Theme.tabTrackPadding
        spacing: Theme.tabTrackSpacing

        Repeater {
            model: strip.tabs

            delegate: Item {
                id: tabItem

                required property int index
                required property var modelData

                readonly property string tab: modelData.key
                readonly property string title: modelData.title
                readonly property string icon: modelData.icon
                readonly property bool selected: strip.selectedTab === index

                signal clicked()

                objectName: strip.itemName
                width: (strip.width - 2 * Theme.tabTrackPadding
                        - (strip.tabs.length - 1) * Theme.tabTrackSpacing) / Math.max(1, strip.tabs.length)
                height: strip.height - 2 * Theme.tabTrackPadding
                onClicked: strip.tabSelected(index)

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.tabItemRadius
                    color: tabItem.selected ? Theme.segmentSelected : "transparent"
                }

                Column {
                    anchors.centerIn: parent
                    spacing: Theme.tabItemSpacing

                    Item {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Theme.tabGlyphBox
                        height: Theme.tabGlyphBox

                        Icon {
                            anchors.centerIn: parent
                            name: tabItem.icon
                            size: Theme.tabGlyphSize
                            color: tabItem.selected ? Theme.accent : Theme.textSecondary
                        }
                    }

                    StyledLabel {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: tabItem.title
                        textStyle: Theme.captionFont
                        weight: Theme.weightSemibold
                        color: tabItem.selected ? Theme.accent : Theme.textSecondary
                    }
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    onTapped: tabItem.clicked()
                }
            }
        }
    }
}

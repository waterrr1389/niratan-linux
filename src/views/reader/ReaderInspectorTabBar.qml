// SPDX-License-Identifier: GPL-3.0-or-later
//
// The tab strip of every reader side panel, following
// NativeMac/NativeReaderInspector.swift `NativeReaderInspectorTabBar`:
// equal-width icon-and-title tabs, the selected one semibold in the primary
// text colour with a 2 px accent underline, the others secondary. The glyph
// sits over the label with 7 px spacing, the strip is inset 12 horizontal and
// a hairline divider runs under it.
//
// Each tab is an `inspectorTab` item exposing `text`, `icon`, `selected` and a
// `clicked` signal; emitting `clicked` activates the tab, so a page test can
// drive the strip without pointer coordinates.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: bar

    property var tabs: []
    property int currentIndex: 0
    signal tabActivated(int index)

    implicitHeight: row.implicitHeight + 1

    Row {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 12
        anchors.rightMargin: 12

        Repeater {
            model: bar.tabs.length

            delegate: Item {
                id: tabItem
                required property int index

                objectName: "inspectorTab"
                width: row.width / Math.max(1, bar.tabs.length)
                height: 44

                readonly property var modelData: bar.tabs[tabItem.index]
                readonly property bool selected: bar.currentIndex === tabItem.index
                property string text: tabItem.modelData ? tabItem.modelData.text : ""
                property string icon: tabItem.modelData ? tabItem.modelData.icon : ""

                signal clicked()

                onClicked: {
                    bar.currentIndex = tabItem.index;
                    bar.tabActivated(tabItem.index);
                }

                Column {
                    anchors.fill: parent
                    spacing: 7

                    Icon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        name: tabItem.icon
                        size: 15
                        color: tabItem.selected ? Theme.textPrimary : Theme.textSecondary
                    }

                    Text {
                        width: parent.width
                        text: tabItem.text
                        color: tabItem.selected ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                        font.bold: tabItem.selected
                        font.weight: tabItem.selected ? Font.DemiBold : Font.Normal
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Rectangle {
                        width: parent.width
                        height: 2
                        radius: 1
                        color: tabItem.selected ? Theme.accent : "transparent"
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: tabItem.clicked()
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.popupDivider
    }
}

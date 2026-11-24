// SPDX-License-Identifier: GPL-3.0-or-later
//
// The tab strip of the reader side panels, following
// NativeMac/NativeReaderInspector.swift `NativeReaderInspectorTabBar`:
// equal-width tabs, each a Label (glyph beside a callout title on one line)
// over a 2 px capsule underline 7 below. The selected tab is semibold in the
// primary colour with the accent underline, the others regular and
// secondary. The strip is inset 12 horizontally and a divider at 0.6 runs
// under it. A small top padding widens the click target.
//
// `tabs` lists { text, icon } with SF Symbol icons. Each tab is an
// `inspectorTab` item exposing `text`, `icon`, `selected` and a clicked()
// signal; emitting clicked activates the tab, so a test can drive the strip
// without pointer coordinates.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: bar

    property var tabs: []
    property int currentIndex: 0
    signal tabActivated(int index)

    implicitHeight: row.implicitHeight + Theme.hairlineWidth

    Row {
        id: row

        x: Theme.inspectorTabBarInset
        width: bar.width - 2 * Theme.inspectorTabBarInset

        Repeater {
            model: bar.tabs.length

            delegate: Item {
                id: tabItem

                required property int index

                readonly property var modelData: bar.tabs[tabItem.index]
                readonly property bool selected: bar.currentIndex === tabItem.index
                property string text: tabItem.modelData ? tabItem.modelData.text : ""
                property string icon: tabItem.modelData ? tabItem.modelData.icon : ""

                signal clicked()

                objectName: "inspectorTab"
                width: row.width / Math.max(1, bar.tabs.length)
                implicitHeight: Theme.underlineTabPaddingTop + label.implicitHeight
                                + Theme.underlineTabSpacing + Theme.underlineHeight
                height: implicitHeight

                Accessible.role: Accessible.PageTab
                Accessible.name: tabItem.text
                Accessible.selected: tabItem.selected
                Accessible.onPressAction: tabItem.clicked()

                onClicked: {
                    bar.currentIndex = tabItem.index
                    bar.tabActivated(tabItem.index)
                }

                Row {
                    id: label

                    readonly property color tone: tabItem.selected ? Theme.textPrimary
                                                                   : Theme.textSecondary

                    x: Math.max(0, (tabItem.width - width) / 2)
                    y: Theme.underlineTabPaddingTop
                    spacing: Theme.chipGlyphSpacing

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: tabItem.icon
                        size: Theme.glyphSize
                        color: label.tone
                    }

                    StyledLabel {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, tabItem.width - Theme.glyphSize
                                        - Theme.chipGlyphSpacing)
                        text: tabItem.text
                        textStyle: Theme.calloutFont
                        weight: tabItem.selected ? Theme.weightSemibold : Theme.weightRegular
                        color: label.tone
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                }

                Rectangle {
                    y: tabItem.height - height
                    width: tabItem.width
                    height: Theme.underlineHeight
                    radius: height / 2
                    color: Theme.accent
                    visible: tabItem.selected
                }

                TapHandler {
                    onTapped: tabItem.clicked()
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
    }

    Rectangle {
        y: bar.height - height
        width: bar.width
        height: Theme.hairlineWidth
        color: Theme.separator
        opacity: Theme.underlineDividerOpacity
    }
}

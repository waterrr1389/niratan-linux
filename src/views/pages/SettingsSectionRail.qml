// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeSettingsReuseView.settingsSidebar and NativeSettingsSection.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

Rectangle {
    id: sidebar

    property int selected: 1
    property real toolbarInset: 0
    readonly property int toolbarHeight: 52
    readonly property var sections: [
        { key: "profiles", title: qsTr("Profiles"), symbol: "person.crop.circle.badge.checkmark" },
        { key: "appearance", title: qsTr("Appearance"), symbol: "paintpalette" },
        { key: "dictionaries", title: qsTr("Dictionaries"), symbol: "character.book.closed.ja" },
        { key: "anki", title: qsTr("Anki"), symbol: "tray.full" },
        { key: "statistics", title: qsTr("Statistics"), symbol: "chart.xyaxis.line" },
        { key: "sasayaki", title: qsTr("Sasayaki (Audiobooks)"), symbol: "waveform" },
        { key: "report", title: qsTr("Report an Issue"), symbol: "exclamationmark.bubble" }
    ]
    readonly property int lastSection: sections.length - 2

    objectName: "settingsColumn"
    color: Theme.columnBackground
    activeFocusOnTab: true
    Keys.onUpPressed: selected = Math.max(0, selected - 1)
    Keys.onDownPressed: selected = Math.min(lastSection, selected + 1)

    Text {
        objectName: "settingsTitle"
        x: 16 + sidebar.toolbarInset
        width: sidebar.width - x - 12
        height: sidebar.toolbarHeight
        verticalAlignment: Text.AlignVCenter
        text: sidebar.sections[sidebar.selected].title
        color: Theme.textPrimary
        font.pixelSize: Theme.fontTitle
        font.weight: Font.Bold
        elide: Text.ElideRight
    }

    Flickable {
        id: list

        anchors.fill: parent
        anchors.topMargin: sidebar.toolbarHeight
        contentHeight: rows.implicitHeight + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: rows

            x: 6
            width: list.width - 12
            spacing: 2

            Repeater {
                model: sidebar.sections

                delegate: Column {
                    id: entry

                    required property int index
                    required property var modelData
                    width: rows.width

                    Text {
                        visible: entry.index === 0 || entry.index === 4
                        leftPadding: 6
                        topPadding: 14
                        bottomPadding: 4
                        text: entry.index === 0 ? qsTr("Library") : qsTr("Reader")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSubheadline
                        font.weight: Font.DemiBold
                    }

                    Item {
                        visible: entry.index > sidebar.lastSection
                        height: Theme.spacingMd
                        width: parent.width
                    }

                    SourceListRow {
                        id: row

                        objectName: "settingsRow-" + entry.modelData.key
                        width: entry.width
                        title: entry.modelData.title
                        symbol: entry.modelData.symbol
                        selected: sidebar.selected === entry.index
                        emphasized: sidebar.activeFocus
                        activeFocusOnTab: entry.index > sidebar.lastSection
                        Accessible.onPressAction: row.clicked()
                        Keys.onSpacePressed: row.clicked()
                        Keys.onReturnPressed: row.clicked()
                        onClicked: {
                            sidebar.forceActiveFocus()
                            if (entry.index > sidebar.lastSection)
                                Qt.openUrlExternally("https://github.com/W1ght/Niratan/issues")
                            else
                                sidebar.selected = entry.index
                        }
                    }
                }
            }
        }
    }
}

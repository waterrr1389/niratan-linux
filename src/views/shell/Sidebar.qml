// SPDX-License-Identifier: GPL-3.0-or-later
//
// The main window's section sidebar, following
// NativeMac/NativeMacSidebarView.swift `body` and `sidebarBrandHeader`, and
// NativeMac/NativeMacSection.swift `allCases`, `title`, `detail` and
// `systemImage`. Manga is out of scope, so `book.pages` has no row.
//
// The column runs the window's full height on the sidebar tone. Its top row is
// the window toolbar's sidebar part, holding the sidebar toggle at its
// trailing edge as the macOS toolbar does; below it sit the brand header (the
// 58 px NiratanSidebarIcon with a 13 px corner radius and the title2 semibold
// "Niratan", padded 18 sideways and 12 above and below) and the section rows,
// 16 from the leading edge as the List's content margin puts them: a
// secondary 16 px glyph, the title and the caption detail.
//
// The list takes keyboard focus like the Swift List(selection:): Up and Down
// move the selection one section, clamped to the first and last. The window
// owns the selection, so a click or key only reports the new section. The
// selected row is accent-filled while the list has focus and grey otherwise.
pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Rectangle {
    id: sidebar

    property int selected: 0
    readonly property int toolbarHeight: Theme.sourceListToolbarHeight

    signal sectionSelected(int index)
    signal toggleRequested()

    readonly property var sections: [
        {
            title: qsTr("Bookshelf"),
            detail: qsTr("Bookshelf and sync"),
            icon: "books.vertical"
        },
        {
            title: qsTr("Dictionary"),
            detail: qsTr("Dictionary search"),
            icon: "character.book.closed"
        },
        {
            title: qsTr("Video"),
            detail: qsTr("Video learning"),
            icon: "play.rectangle"
        },
        {
            title: qsTr("Settings"),
            detail: qsTr("App settings"),
            icon: "gearshape"
        }
    ]

    color: Theme.sidebarBackground

    activeFocusOnTab: true
    Accessible.role: Accessible.List
    Accessible.name: qsTr("Sections")

    Keys.onUpPressed: function (event) {
        event.accepted = true
        sidebar.sectionSelected(Math.max(0, sidebar.selected - 1))
    }

    Keys.onDownPressed: function (event) {
        event.accepted = true
        sidebar.sectionSelected(Math.min(sidebar.sections.length - 1, sidebar.selected + 1))
    }

    ToolbarButton {
        anchors.right: parent.right
        anchors.rightMargin: Theme.sourceListTitleTrailing
        y: (sidebar.toolbarHeight - height) / 2
        symbol: "sidebar.leading"
        help: qsTr("Toggle Sidebar")
        onClicked: sidebar.toggleRequested()
    }

    Row {
        id: brand

        x: Theme.shellBrandInset
        y: sidebar.toolbarHeight + Theme.spacingMd
        spacing: Theme.spacingMd

        Item {
            width: Theme.shellBrandIconSize
            height: Theme.shellBrandIconSize

            RoundedImage {
                anchors.fill: parent
                radius: Theme.shellBrandIconRadius
                maskColor: sidebar.color
                source: "qrc:/qt/qml/Niratan/src/views/shell/niratan-sidebar-icon.png"
                fillMode: Image.PreserveAspectCrop
                // Twice the drawn size, for HiDPI screens.
                sourceSize.width: 2 * Theme.shellBrandIconSize
                sourceSize.height: 2 * Theme.shellBrandIconSize
            }

            Rectangle {
                anchors.fill: parent
                radius: Theme.shellBrandIconRadius
                color: "transparent"
                border.width: Theme.shellBrandIconStroke
                border.color: Theme.cardStroke
            }
        }

        StyledLabel {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Niratan")
            textStyle: Theme.title2Font
            weight: Theme.weightSemibold
        }
    }

    Column {
        id: sectionList

        x: Theme.sourceListTitleInset
        y: brand.y + brand.height + Theme.spacingMd
        width: sidebar.width - x - Theme.shellSectionTrailing
        spacing: Theme.sourceListRowSpacing

        Repeater {
            model: sidebar.sections

            delegate: SourceListRow {
                required property int index
                required property var modelData

                width: sectionList.width
                symbol: modelData.icon
                symbolColor: Theme.textSecondary
                title: modelData.title
                detail: modelData.detail
                selected: sidebar.selected === index
                emphasized: sidebar.activeFocus
                onClicked: {
                    sidebar.forceActiveFocus()
                    sidebar.sectionSelected(index)
                }
            }
        }
    }

    Rectangle {
        anchors.right: parent.right
        width: Theme.hairlineWidth
        height: parent.height
        color: Theme.separator
    }
}

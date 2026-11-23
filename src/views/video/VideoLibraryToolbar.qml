// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video library toolbar, following
// Features/Video/VideoLibraryView.swift `videoToolbarContent` (local branch),
// `searchTextBinding`, `VideoLibrarySortMenu`, `VideoLibraryLayoutPicker`,
// `LibraryCoverSizeButton`, `VideoLibrarySourceToolbarButtons` (Refresh and
// the Add menu's local-folder item) and `VideoLibraryView.presentFolderImporter`.
//
// In order: the search field ("Search Videos") writing `model.searchText`; the
// Sort Videos menu (Recent Playback, File Name, Modified Date, Folder, the
// current option checked) writing `sortOption`; the List/Posters picker
// writing `layoutMode`; the cover-size control while posters are shown,
// writing `posterSize` over 110...240 in steps of 10; Refresh calling
// `refreshAllSources`, disabled without sources or while scanning; and the Add
// menu with "Add Video Folder", which asks the page to open the directory
// picker.
//
// The row is the page's top row. Its content sits a Theme.spacingSm gap below
// the page top, standing in for the macOS window toolbar band that Swift's
// `.searchable(placement: .toolbar)` and `videoToolbarContent` share (Decision
// D1); the search capsule and every trailing control keep one vertical centre.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan
import "../icons/icon_map.js" as IconMap

Item {
    id: toolbar

    property var model
    property real toolbarInset: 0

    signal addFolderRequested()

    SearchBar {
        id: searchBar

        objectName: "searchBar"
        x: 12 + toolbar.toolbarInset
        y: Theme.spacingSm
        width: Math.min(260, Math.max(140, toolbar.width * 0.32))
        placeholderText: qsTr("Search Videos")
        text: toolbar.model ? toolbar.model.searchText : ""
        onTextChanged: {
            if (toolbar.model && toolbar.model.searchText !== text)
                toolbar.model.searchText = text
        }
    }

    ToolbarGroup {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: searchBar.verticalCenter

        ToolbarButton {
            id: sortButton

            objectName: "sortButton"
            symbol: "arrow.up.arrow.down"
            help: qsTr("Sort Videos")
            onClicked: sortMenu.popup(sortButton, 0, sortButton.height + 4)
        }

        ToolbarButton {
            objectName: "layoutListButton"
            symbol: "list.bullet"
            help: qsTr("List")
            checkable: true
            checked: toolbar.model ? toolbar.model.layoutMode === "list" : false
            onClicked: if (toolbar.model) toolbar.model.layoutMode = "list"
        }

        ToolbarButton {
            objectName: "layoutPostersButton"
            symbol: "square.grid.2x2"
            help: qsTr("Posters")
            checkable: true
            checked: toolbar.model ? toolbar.model.layoutMode === "posters" : false
            onClicked: if (toolbar.model) toolbar.model.layoutMode = "posters"
        }

        ToolbarButton {
            id: coverSizeButton

            objectName: "coverSizeButton"
            visible: toolbar.model ? toolbar.model.layoutMode === "posters" : false
            symbol: "square.resize"
            help: qsTr("Cover Size")
            onClicked: coverSizePanel.visible = !coverSizePanel.visible
        }

        ToolbarButton {
            objectName: "refreshButton"
            symbol: "arrow.clockwise"
            help: qsTr("Refresh")
            enabled: toolbar.model ? toolbar.model.hasSources && !toolbar.model.isScanning : false
            onClicked: if (toolbar.model) toolbar.model.refreshAllSources()
        }

        ToolbarButton {
            id: addButton

            objectName: "addButton"
            symbol: "plus"
            help: qsTr("Add")
            onClicked: addMenu.popup(addButton, 0, addButton.height + 4)
        }
    }

    Menu {
        id: sortMenu

        objectName: "sortMenu"

        Repeater {
            model: [
                { option: "recentPlayback", title: qsTr("Recent Playback"),
                  symbol: "clock" },
                { option: "title", title: qsTr("File Name"), symbol: "textformat" },
                { option: "modifiedDate", title: qsTr("Modified Date"), symbol: "calendar" },
                { option: "folder", title: qsTr("Folder"), symbol: "folder" }
            ]

            delegate: MenuItem {
                required property var modelData

                text: modelData.title
                icon.source: IconMap.source(modelData.symbol)
                checkable: true
                checked: toolbar.model ? toolbar.model.sortOption === modelData.option : false
                onTriggered: if (toolbar.model) toolbar.model.sortOption = modelData.option
            }
        }
    }

    Menu {
        id: addMenu

        objectName: "addMenu"

        MenuItem {
            text: qsTr("Add Video Folder")
            icon.source: IconMap.source("folder.badge.plus")
            onTriggered: toolbar.addFolderRequested()
        }
    }

    // LibraryCoverSizeButton's popover: small and large photo glyphs around a
    // 180 px slider from 110 to 240 in steps of 10. It is an in-tree panel so
    // the page's test can reach the slider; the button toggles it and changing
    // the layout closes it.
    Rectangle {
        id: coverSizePanel

        parent: toolbar
        visible: false
        width: coverSizeRow.implicitWidth + 32
        height: coverSizeRow.implicitHeight + 32
        x: Math.max(8, Math.min(toolbar.width - width - 8,
                                coverSizeButton.mapToItem(toolbar, 0, 0).x
                                + coverSizeButton.width - width))
        y: toolbar.height + 6
        radius: 10
        color: Theme.floatingBackground
        border.width: 1
        border.color: Theme.floatingStroke

        Row {
            id: coverSizeRow

            anchors.centerIn: parent
            spacing: 10

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "photo"
                size: 11
                color: Theme.textSecondary
            }

            Slider {
                id: coverSizeSlider

                objectName: "coverSizeSlider"
                width: 180
                anchors.verticalCenter: parent.verticalCenter
                from: 110
                to: 240
                stepSize: 10
                value: toolbar.model ? toolbar.model.posterSize : 160
                onMoved: if (toolbar.model) toolbar.model.posterSize = Math.round(value)
            }

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "photo"
                size: 17
                color: Theme.textSecondary
            }
        }
    }

    Connections {
        target: toolbar.model

        function onLayoutModeChanged() {
            if (!toolbar.model || toolbar.model.layoutMode !== "posters")
                coverSizePanel.visible = false
        }
    }
}

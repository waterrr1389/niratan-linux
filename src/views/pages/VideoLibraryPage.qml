// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Video section's local library, following
// Features/Video/VideoLibraryView.swift: `VideoLibraryView.body`, `content`,
// `videoToolbarContent` (the local-folder branch), `searchTextBinding`,
// `presentFolderImporter`, `libraryContent`, `libraryListRow`, `open`,
// `sectionExpansionBinding`, `shouldHideSingleSectionHeader`,
// `duplicateSectionHeaderID`, `deleteCollectionAction`, `deleteCollection`,
// `VideoLibraryPosterLayout`, `VideoLibrarySidebarView.body`,
// `sidebarSelection`, `sidebarRow`, `VideoLibrarySortMenu`,
// `VideoLibraryLayoutPicker`, `VideoLibrarySourceToolbarButtons` (Refresh and
// the Add menu's local-folder item), `VideoLibraryContentHeader.body`,
// `VideoLibraryPosterGridView.body`, `VideoLibraryCollapsibleSectionHeader`,
// `VideoLibraryCollectionActionsMenu`, `VideoLibraryPosterCardView`,
// `VideoLibraryItemContextMenu` (the local branch),
// `VideoLibraryPosterArtworkView`, `VideoThumbnailImageView`,
// `VideoThumbnailPlaceholderView`, `VideoLibraryProgressTrack`,
// `VideoLibraryRowView`, `VideoLibraryPlaybackText.state` and `.location`, and
// the Error alert, the Delete Collection alert, the scanning `LoadingOverlay`
// and the `onAppear` load/refresh.
//
// Layout follows the bookshelf detail: the mode column takes 24% of the width
// clamped to 176..230, then the detail's top row is the toolbar and below it
// the mode title with the section count and the list or poster grid. Below 760
// the mode column is hidden and the title becomes the mode menu. The list,
// grid and empty states live in VideoLibraryContent.
//
// Decision: local sources only. Media servers, remote links, source
// management, the collection and smart-collection editors and the details
// inspector are out of scope, so the page never draws their toolbar items,
// sheets or context-menu entries. Thumbnails are a later slice: every artwork
// is the film placeholder and no frame is requested. "Reveal in Finder"
// becomes "Reveal in File Manager" and opens the item's parent folder.
//
// Opening selects the row first, then emits `videoOpened(absolutePath,
// boundSubtitlePath, fromBeginning)`; the shell pushes the player, which calls
// openVideo and loadSubtitle. A row whose file disappeared shows the view
// model's error and emits nothing.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan
import "../icons/icon_map.js" as IconMap

Item {
    id: page

    signal videoOpened(string url, string subtitleUrl, bool fromBeginning)

    property real toolbarInset: 0
    readonly property int toolbarHeight: 52

    // Set by the shell; the page refreshes its history when the Video window
    // closes, keeping the section's mode, sort and selection.
    property var playerWindow

    // VideoLibraryView.showsLibrarySidebar: narrow windows spend their width
    // on the app sidebar, so the modes move into the title menu.
    readonly property bool showsLibrarySidebar: page.width >= 760

    readonly property var modeTitles: ({
        continueWatching: qsTr("Continue Watching"),
        favorites: qsTr("Favorites"),
        unwatched: qsTr("Unwatched"),
        finished: qsTr("Finished"),
        missing: qsTr("Missing"),
        recent: qsTr("Recent"),
        all: qsTr("All Videos"),
        needsReview: qsTr("Needs Review"),
        series: qsTr("Series"),
        folders: qsTr("Folders"),
        collections: qsTr("Collections")
    })

    readonly property string modeTitle: page.modeTitles[video.displayMode] ?? ""
    readonly property int sectionCount: {
        var count = 0
        var list = video.sections
        for (var i = 0; i < list.length; ++i)
            count += list[i].rows.length
        return count
    }

    // VideoLibraryView.open: select, resolve the stored progress, then hand
    // the path, subtitle and flag to the shell. A missing file leaves the
    // view model's error behind and emits nothing.
    function openItem(row, fromBeginning) {
        if (!row)
            return
        video.select(row.id)
        var url = fromBeginning ? video.openFromBeginningUrl(row.id) : video.openUrl(row.id)
        if (url.length === 0)
            return
        page.videoOpened(url, video.subtitleUrlForOpening(row.id), fromBeginning)
    }

    function revealItem(row) {
        if (!row || !row.path)
            return
        var path = row.path.substring(0, row.path.lastIndexOf("/"))
        if (path.length > 0)
            Qt.openUrlExternally("file://" + encodeURI(path))
    }

    // FolderDialog returns a URL; the stored source path is a local path.
    function folderPath(url) {
        var text = String(url)
        if (text.indexOf("file://") === 0)
            text = text.substring("file://".length)
        return decodeURIComponent(text)
    }

    // Re-reads the playback history after the player closes (B13).
    function refreshPlayback() {
        video.refreshPlaybackHistory()
    }

    VideoLibraryViewModel {
        id: video

        objectName: "videoLibraryModel"
    }

    Component.onCompleted: {
        video.load()
        video.refreshPlaybackHistory()
    }

    Connections {
        target: page.playerWindow ? page.playerWindow : null
        ignoreUnknownSignals: true
        enabled: page.playerWindow !== null && page.playerWindow !== undefined

        function onVideoClosed() {
            page.refreshPlayback()
        }
    }

    Connections {
        target: video

        function onErrorMessageChanged() {
            if (video.errorMessage.length > 0)
                dialogs.showError(video.errorMessage)
        }
    }

    VideoLibraryDialogs {
        id: dialogs

        width: page.width
        height: page.height
        z: 20
        model: video
        onFoldersChosen: function (folders) {
            var paths = []
            for (var i = 0; i < folders.length; ++i) {
                var path = page.folderPath(folders[i])
                if (path.length > 0)
                    paths.push(path)
            }
            if (paths.length > 0)
                video.addFolders(paths)
        }
    }

    VideoLibrarySidebar {
        id: sidebar

        objectName: "librarySidebar"
        width: page.showsLibrarySidebar
               ? Math.min(230, Math.max(176, page.width * 0.24)) : 0
        height: parent.height
        visible: page.showsLibrarySidebar
        model: video
        toolbarInset: page.toolbarInset
    }

    Rectangle {
        x: sidebar.width
        width: page.showsLibrarySidebar ? 1 : 0
        height: parent.height
        color: Theme.separator
    }

    Item {
        id: detail

        x: sidebar.width + (page.showsLibrarySidebar ? 1 : 0)
        width: page.width - x
        height: page.height

        VideoLibraryToolbar {
            id: toolbar

            width: parent.width
            height: page.toolbarHeight
            model: video
            toolbarInset: page.toolbarInset
            onAddFolderRequested: dialogs.openFolderPicker()
        }

        // VideoLibraryContentHeader: the mode title and the visible row count;
        // narrow windows turn the title into the mode menu.
        Item {
            id: contentHeader

            anchors.top: toolbar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 14 + 22 + 10
            visible: video.hasSources

            Row {
                x: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    id: headerTitle

                    anchors.baseline: headerCount.baseline
                    text: page.modeTitle
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontTitle2
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    id: headerCount

                    objectName: "headerCount"
                    text: page.sectionCount
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontTitle
                    font.features: { "tnum": 1 }
                }

                Icon {
                    visible: !page.showsLibrarySidebar
                    anchors.verticalCenter: parent.verticalCenter
                    name: "chevron.down"
                    size: 10
                    color: Theme.textSecondary
                }
            }

            // The title becomes the mode menu while the mode column is
            // hidden (VideoLibraryContentHeader.modeItems).
            Item {
                id: modeMenuButton

                objectName: "modeMenu"
                visible: !page.showsLibrarySidebar
                anchors.fill: parent

                signal clicked()

                onClicked: modeMenuPopup.popup()

                TapHandler {
                    onTapped: modeMenuButton.clicked()
                }
            }

            Menu {
                id: modeMenuPopup

                Repeater {
                    model: [
                        { mode: "continueWatching", title: qsTr("Continue Watching"),
                          symbol: "play.circle" },
                        { mode: "recent", title: qsTr("Recent"), symbol: "clock" },
                        { mode: "all", title: qsTr("All Videos"), symbol: "film.stack" },
                        { mode: "unwatched", title: qsTr("Unwatched"),
                          symbol: "circle.dashed" },
                        { mode: "finished", title: qsTr("Finished"),
                          symbol: "checkmark.circle" },
                        { mode: "favorites", title: qsTr("Favorites"), symbol: "star" },
                        { mode: "missing", title: qsTr("Missing"),
                          symbol: "exclamationmark.triangle" }
                    ]

                    delegate: MenuItem {
                        required property var modelData

                        text: modelData.title
                        icon.source: IconMap.source(modelData.symbol)
                        onTriggered: video.displayMode = modelData.mode
                    }
                }

                MenuSeparator {}

                Repeater {
                    model: [
                        { mode: "series", title: qsTr("Series"), symbol: "rectangle.stack" },
                        { mode: "folders", title: qsTr("Folders"), symbol: "folder" },
                        { mode: "collections", title: qsTr("Collections"),
                          symbol: "square.stack" },
                        { mode: "needsReview", title: qsTr("Needs Review"), symbol: "tray" }
                    ]

                    delegate: MenuItem {
                        required property var modelData

                        text: modelData.title
                        icon.source: IconMap.source(modelData.symbol)
                        onTriggered: video.displayMode = modelData.mode
                    }
                }
            }
        }

        VideoLibraryContent {
            id: content

            anchors.top: contentHeader.visible ? contentHeader.bottom : toolbar.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            model: video
            onOpenRequested: function (row, fromBeginning) {
                page.openItem(row, fromBeginning)
            }
            onMarkWatchedRequested: function (row) {
                if (row)
                    video.markWatched(row.id)
            }
            onClearProgressRequested: function (row) {
                if (row)
                    video.clearProgress(row.id)
            }
            onRevealRequested: function (row) {
                page.revealItem(row)
            }
            onDeleteCollectionRequested: function (collectionId) {
                dialogs.askDeleteCollection(collectionId)
            }
            onAddFolderRequested: dialogs.openFolderPicker()
        }

        // The scanning LoadingOverlay.
        Item {
            id: scanningOverlay

            objectName: "scanningOverlay"
            anchors.fill: parent
            z: 3
            visible: video.isScanning

            Rectangle {
                anchors.fill: parent
                color: Theme.windowBackground
                opacity: 0.85
            }

            Column {
                anchors.centerIn: parent
                spacing: 12

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: scanningOverlay.visible
                }

                Text {
                    text: qsTr("Scanning Video Folders...")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontBody
                }
            }
        }
    }
}

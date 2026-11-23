// SPDX-License-Identifier: GPL-3.0-or-later
//
// The application shell: a 240-wide section sidebar beside the selected
// detail, following NativeMac/NativeMacRootView.swift `rootContent` and
// `selectedSection`, NativeMac/NativeMacDetailView.swift `body`, and the
// WindowGroup frame in NativeMac/HoshiNativeMacApp.swift (900x620 minimum).
// The window opens at 960x640 and its title is the selected section's title.
//
// Decision: every window keeps the native Linux title bar. Below it the
// sidebar runs the full height with the sidebar toggle in its top row, where
// the macOS window toolbar puts it; the toggle collapses the sidebar to width
// 0 and restores it to 240. While it is collapsed the toggle floats at the
// detail's leading edge, and a page that draws its own toolbar row is told to
// leave room for it (`toolbarInset`). Pages that do not draw a toolbar row yet
// sit below a plain 52 px band.
//
// Sections follow NativeMacSection: Bookshelf, Dictionary, Video and Settings;
// manga is out of scope. Bookshelf shows LibraryPage and keeps its toolbar
// statistics page as an overlay with a back that returns to the shelf.
// Dictionary hides its back control. Settings draws its own toolbar and
// source list, following NativeMac/NativeReuseViews.swift NativeSettingsReuseView.
// Video shows VideoLibraryPage only; opening a video shows or raises the
// single dedicated VideoWindow, which holds PlayerPage. The main window draws
// no player controls and stays independent of that window.
//
// The reader is not a section: opening a book opens or raises the single
// ReaderWindow after ReaderViewModel.openBook succeeds, following
// ReaderWindowPresenter.makeWindow.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

ApplicationWindow {
    id: window

    visible: true
    width: 960
    height: 640
    minimumWidth: 900
    minimumHeight: 620
    title: window.sectionTitles[window.sectionIndex]
    color: Theme.windowBackground

    property int sectionIndex: 0
    property bool sidebarVisible: true
    property int bookshelfDepth: 1
    // Counts reader window closes, so the bookshelf reloads its progress the
    // way Swift's readerWindowProgressDidChange makes it call loadBooks().
    property int readerCloses: 0

    readonly property int toolbarHeight: 52
    readonly property bool pageOwnsToolbar: window.sectionIndex === 3
                                            || window.sectionIndex === 2
                                            || window.sectionIndex === 1
                                            || window.sectionIndex === 0

    readonly property var sectionTitles: [
        qsTr("Bookshelf"),
        qsTr("Dictionary"),
        qsTr("Video"),
        qsTr("Settings")
    ]

    // The reader model loads and watches the active profile's settings.
    Binding {
        target: ThemeState
        property: "uiTheme"
        value: ReaderViewModel.uiTheme
    }

    function openReader(folder) {
        if (!ReaderViewModel.openBook(folder))
            return
        readerWindow.show()
        readerWindow.raise()
        readerWindow.requestActivate()
    }


    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar

            Layout.preferredWidth: window.sidebarVisible ? 240 : 0
            Layout.fillHeight: true
            visible: window.sidebarVisible
            selected: window.sectionIndex
            Component.onCompleted: sidebar.forceActiveFocus()
            onSectionSelected: function (index) {
                window.sectionIndex = index
            }
            onToggleRequested: window.sidebarVisible = false
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Loader {
                id: detailLoader

                anchors.fill: parent
                anchors.topMargin: window.pageOwnsToolbar ? 0 : window.toolbarHeight
                sourceComponent: window.sectionIndex === 0 ? bookshelfDetail
                    : window.sectionIndex === 1 ? dictionaryDetail
                    : window.sectionIndex === 2 ? videoDetail
                    : settingsDetail
            }

            ToolbarButton {
                x: 12
                y: (window.toolbarHeight - height) / 2
                visible: !window.sidebarVisible
                symbol: "sidebar.leading"
                help: qsTr("Toggle Sidebar")
                onClicked: window.sidebarVisible = true
            }
        }
    }

    ReaderWindow {
        id: readerWindow

        onVisibleChanged: {
            if (!readerWindow.visible)
                ++window.readerCloses
        }
    }

    VideoWindow {
        id: videoWindow

        ownerScreen: window.screen
    }

    Component {
        id: bookshelfDetail

        StackView {
            id: bookshelfStack

            onDepthChanged: window.bookshelfDepth = bookshelfStack.depth
            Component.onDestruction: window.bookshelfDepth = 1

            initialItem: LibraryPage {
                toolbarInset: window.sidebarVisible ? 0 : 44
                readerCloses: window.readerCloses
                onBookOpened: function (folder) {
                    window.openReader(folder)
                }
                onStatisticsRequested: bookshelfStack.push(statisticsDetail)
            }

            Component {
                id: statisticsDetail

                StatisticsPage {
                    toolbarInset: window.sidebarVisible ? 0 : 44
                    onBackRequested: bookshelfStack.pop()
                }
            }
        }
    }

    Component {
        id: dictionaryDetail

        DictionaryPage {
            toolbarInset: window.sidebarVisible ? 0 : 44
        }
    }

    Component {
        id: videoDetail

        VideoLibraryPage {
            toolbarInset: window.sidebarVisible ? 0 : 44
            playerWindow: videoWindow
            onVideoOpened: function (url, subtitleUrl, fromBeginning) {
                videoWindow.openVideo(url, subtitleUrl, fromBeginning)
            }
        }
    }

    Component {
        id: settingsDetail

        SettingsPage {
            toolbarInset: window.sidebarVisible ? 0 : 44
        }
    }
}

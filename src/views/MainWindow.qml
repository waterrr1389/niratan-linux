// SPDX-License-Identifier: GPL-3.0-or-later
//
// The application shell: a 240-wide section sidebar beside the selected
// detail, following NativeMac/NativeMacRootView.swift `rootContent` and
// `selectedSection`, NativeMac/NativeMacDetailView.swift `body`, and the
// WindowGroup frame in NativeMac/HoshiNativeMacApp.swift (900x620 minimum).
// The window opens at 960x640 and its title is the selected section's title.
//
// Decision: every window keeps the native Linux title bar. Below it, a 48 px
// shell toolbar carries only the sidebar toggle at its 12 px leading inset
// (34 px IconButton, SF Symbol `sidebar.leading`); the toggle collapses the
// app sidebar to width 0 and restores it to 240. The detail keeps the freed
// width either way.
//
// Sections follow NativeMacSection: Bookshelf, Dictionary, Video and Settings;
// manga is out of scope. Bookshelf shows LibraryPage and keeps its toolbar
// statistics page as an overlay with a back that returns to the shelf.
// Dictionary hides its dead back control; Settings keeps its band and back
// control until the settings page is split, and its rail still reaches Anki,
// Dictionaries and Profiles inside the Settings detail. The Video section is
// the interim PlayerPage.
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: shellToolbar

            Layout.fillWidth: true
            Layout.preferredHeight: 48

            IconButton {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                diameter: 34
                icon: "sidebar.leading"
                accessibleName: qsTr("Toggle Sidebar")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Toggle Sidebar")
                ToolTip.delay: 700
                onClicked: window.sidebarVisible = !window.sidebarVisible
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Sidebar {
                Layout.preferredWidth: window.sidebarVisible ? 240 : 0
                Layout.fillHeight: true
                visible: window.sidebarVisible
                selected: window.sectionIndex
                onSectionSelected: function (index) {
                    window.sectionIndex = index
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Loader {
                    anchors.fill: parent
                    sourceComponent: window.sectionIndex === 0 ? bookshelfDetail
                        : window.sectionIndex === 1 ? dictionaryDetail
                        : window.sectionIndex === 2 ? videoDetail
                        : settingsDetail
                }
            }
        }
    }

    ReaderWindow {
        id: readerWindow
    }

    Component {
        id: bookshelfDetail

        StackView {
            id: bookshelfStack

            initialItem: LibraryPage {
                onBookOpened: function (folder) {
                    window.openReader(folder)
                }
                onStatisticsRequested: bookshelfStack.push(statisticsDetail)
                onDictionaryRequested: window.sectionIndex = 1
                onVideoRequested: window.sectionIndex = 2
                onSettingsRequested: window.sectionIndex = 3
            }

            Component {
                id: statisticsDetail

                StatisticsPage {
                    onBackRequested: bookshelfStack.pop()
                }
            }
        }
    }

    Component {
        id: dictionaryDetail

        DictionaryPage {}
    }

    Component {
        id: videoDetail

        PlayerPage {
            onBackRequested: window.sectionIndex = 0
        }
    }

    Component {
        id: settingsDetail

        StackView {
            id: settingsStack

            initialItem: SettingsPage {
                onAnkiRequested: settingsStack.push(ankiDetail)
                onDictionariesRequested: settingsStack.push(dictionariesDetail)
                onProfilesRequested: settingsStack.push(profilesDetail)
            }

            Component {
                id: ankiDetail

                AnkiSettingsPage {
                    onBackRequested: settingsStack.pop()
                }
            }

            Component {
                id: dictionariesDetail

                DictionarySettingsPage {
                    onBackRequested: settingsStack.pop()
                }
            }

            Component {
                id: profilesDetail

                ProfilesPage {
                    onBackRequested: settingsStack.pop()
                }
            }
        }
    }
}

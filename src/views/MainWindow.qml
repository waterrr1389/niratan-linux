// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

// The application shell: the bookshelf, then the pages pushed over it.
// Like Niratan's main window, the bookshelf stays mounted so closing a page
// returns to the same shelf state.
ApplicationWindow {
    id: window
    visible: true
    width: 960
    height: 640
    minimumWidth: 900
    minimumHeight: 620
    title: "Niratan"
    color: Theme.windowBackground

    // The reader model loads and watches the active profile's settings.
    Binding {
        target: ThemeState
        property: "uiTheme"
        value: ReaderViewModel.uiTheme
    }

    StackView {
        id: stack
        anchors.fill: parent

        initialItem: LibraryPage {
            onBookOpened: function (folder) {
                if (ReaderViewModel.openBook(folder))
                    stack.push(readerPage)
            }
            onDictionaryRequested: stack.push(dictionaryPage)
            onVideoRequested: stack.push(playerPage)
            onProfilesRequested: stack.push(profilesPage)
            onStatisticsRequested: stack.push(statisticsPage)
            onSettingsRequested: stack.push(settingsPage)
        }
    }

    Component {
        id: readerPage
        ReaderPage {
            onBackRequested: {
                ReaderViewModel.closeBook()
                stack.pop()
            }
        }
    }

    Component {
        id: dictionaryPage
        DictionaryPage {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: playerPage
        PlayerPage {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: profilesPage
        ProfilesPage {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: statisticsPage
        StatisticsPage {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: settingsPage
        SettingsPage {
            onBackRequested: stack.pop()
            onAnkiRequested: stack.push(ankiPage)
        }
    }

    Component {
        id: ankiPage
        AnkiSettingsPage {
            onBackRequested: stack.pop()
        }
    }
}

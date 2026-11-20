// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

// The application shell: the bookshelf, the reader and the dictionary page.
// Like Niratan's main window, the bookshelf stays mounted and the reader is a
// pushed page, so closing the reader returns to the same shelf state.
ApplicationWindow {
    id: window
    visible: true
    width: 960
    height: 640
    minimumWidth: 900
    minimumHeight: 620
    title: "Niratan"
    color: Theme.windowBackground

    StackView {
        id: stack
        anchors.fill: parent

        initialItem: LibraryPage {
            onBookOpened: function (folder) {
                if (ReaderViewModel.openBook(folder))
                    stack.push(readerPage)
            }
            onDictionaryRequested: stack.push(dictionaryPage)
        }
    }

    Component {
        id: readerPage
        ReaderPage {
            onBackRequested: {
                ReaderViewModel.saveBookmark()
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
}

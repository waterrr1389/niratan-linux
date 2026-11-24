// SPDX-License-Identifier: GPL-3.0-or-later
//
// Local verification sheet for the reader's lookup surface and the mining
// toast, shown with NIRATAN_THEME_PREVIEW=lookup. The real
// ReaderLookupSurface looks up a fixed word with the installed dictionaries
// and shows its results beside a stand-in selection; two real
// AnkiMiningToast instances show the added and failed states. It is not part
// of any user flow and wires nothing to the reader or to Anki.
import QtQuick
import Niratan

Rectangle {
    id: sheet

    color: Theme.windowBackground

    // The left half: a stand-in page with the selection the popup anchors to.
    Item {
        id: stage

        width: parent.width / 2
        height: parent.height

        Rectangle {
            id: selection

            x: Theme.spacingXl
            y: parent.height / 3
            width: Theme.lookupControlWidth
            height: Theme.lookupControlHeight
            color: Qt.alpha(Theme.accent, 0.3)
        }

        ReaderLookupSurface {
            id: surface

            popupModel: popup
            miningModel: mining
            player: sasayaki
            selectionAnchor: Qt.rect(selection.x, selection.y, selection.width, selection.height)
            visible: popup.hasResults
        }
    }

    // The right half: the toast in its added and failed states.
    Item {
        x: parent.width / 2
        width: parent.width / 2
        height: parent.height / 2

        AnkiMiningToast {
            status: "added"
            message: "食べる"
        }
    }

    Item {
        x: parent.width / 2
        y: parent.height / 2
        width: parent.width / 2
        height: parent.height / 2

        AnkiMiningToast {
            status: "failed"
            message: qsTr("AnkiConnect is not reachable.")
        }
    }

    DictionaryPopupModel {
        id: popup

        dark: Theme.dark
        background: Theme.popupBackground
        textColor: Theme.popupText
        onHtmlChanged: {
            if (hasResults)
                surface.showResults(html)
        }
    }

    MiningViewModel {
        id: mining
    }

    SasayakiViewModel {
        id: sasayaki
    }

    Component.onCompleted: popup.lookup("食べる")
}

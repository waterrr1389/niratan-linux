// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

// ReaderGoToView.chaptersTab places the reading count beside the title.
ListView {
    id: chaptersList

    signal chapterSelected()

    clip: true
    model: ReaderViewModel.chapters
    currentIndex: {
        const offset = ReaderViewModel.characterOffset
        let best = -1
        let start = 0
        for (let i = 0; i < model.length; ++i) {
            const candidate = model[i].characterStart
            if (candidate >= start && candidate <= offset) {
                best = i
                start = candidate
            }
        }
        return best
    }

    delegate: ItemDelegate {
        id: chapterItem
        required property var modelData
        width: ListView.view.width
        leftPadding: padding + 12 * chapterItem.modelData.level
        text: chapterItem.modelData.title
        highlighted: ListView.isCurrentItem

        contentItem: RowLayout {
            spacing: Theme.spacingSm
            Label {
                Layout.fillWidth: true
                text: chapterItem.text
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font: chapterItem.font
                color: Theme.textPrimary
            }
            Label {
                readonly property int characterStart: chapterItem.modelData.characterStart
                visible: characterStart >= 0
                text: (ReaderViewModel.englishContent ? Math.ceil(characterStart / 5) : characterStart)
                    .toLocaleString(Qt.locale(), "f", 0)
                color: Theme.textSecondary
                font.pixelSize: 11
            }
        }

        onClicked: {
            ReaderViewModel.goToContentsEntry(chapterItem.modelData.index,
                                              chapterItem.modelData.fragment)
            chaptersList.chapterSelected()
        }
    }
}

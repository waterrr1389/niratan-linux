// SPDX-License-Identifier: GPL-3.0-or-later
//
// SwiftUI ContentUnavailableView as the video library shows it
// (Features/Video/VideoLibraryView.swift `libraryContent`): a large
// secondary glyph, a bold title and the wrapped secondary description,
// centred. Actions placed as children follow the description.
import QtQuick
import Niratan

Column {
    id: view

    property string symbol
    property string title
    property string message

    spacing: Theme.spacingSm

    Icon {
        anchors.horizontalCenter: parent.horizontalCenter
        size: Theme.unavailableGlyph
        name: view.symbol
        color: Theme.textSecondary
    }

    StyledLabel {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: view.title
        textStyle: Theme.title2Font
        weight: Theme.weightBold
    }

    StyledLabel {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: view.message
        textStyle: Theme.calloutFont
        secondary: true
    }
}

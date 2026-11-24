// SPDX-License-Identifier: GPL-3.0-or-later
//
// ReaderGoToView.swift searchResultsContent, chaptersTab and highlightsTab,
// and GalleryView.swift body: a centered unavailable state or progress cue.
import QtQuick
import QtQuick.Controls
import Niratan

Column {
    id: status

    property string text
    property string symbol
    property bool busy: false
    property real horizontalInset: Theme.panelHeaderPaddingHorizontal

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.leftMargin: horizontalInset
    anchors.rightMargin: horizontalInset
    anchors.verticalCenter: parent.verticalCenter
    spacing: Theme.spacingSm

    BusyIndicator {
        anchors.horizontalCenter: parent.horizontalCenter
        running: status.visible && status.busy
        visible: status.busy
    }

    Icon {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: !status.busy && status.symbol.length > 0
        name: status.symbol
        size: Theme.largeTitleFont.pixelSize
        color: Theme.textSecondary
    }

    StyledLabel {
        width: parent.width
        visible: status.text.length > 0
        text: status.text
        secondary: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}

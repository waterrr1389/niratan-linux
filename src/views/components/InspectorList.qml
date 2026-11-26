// SPDX-License-Identifier: GPL-3.0-or-later
//
// Vertical lists from ReaderGoToView.swift and GalleryView.swift, with the
// same bounds and overflow indicator as PanelScaffold.
import QtQuick
import QtQuick.Controls

ListView {
    id: list

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick
    // As in PanelScaffold: no mouse dragging, so a click right after a wheel
    // scroll reaches the row instead of stopping the scroll.
    acceptedButtons: Qt.NoButton

    ScrollBar.vertical: ScrollBar {
        visible: list.contentHeight > list.height
        policy: list.contentHeight > list.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
    }
}

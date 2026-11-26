// SPDX-License-Identifier: GPL-3.0-or-later
//
// A blocking progress card, following Util/Extensions.swift `LoadingOverlay`:
// a dimmed window with a spinner above a one-line message on a rounded
// surface. It shows while `active` is true and cannot be dismissed by the user.

import QtQuick
import QtQuick.Controls
import Niratan

Popup {
    id: overlay

    property string message: qsTr("Loading...")
    property bool active: false
    // Names the message label for a page test; empty leaves it anonymous.
    property string messageObjectName

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    closePolicy: Popup.NoAutoClose
    padding: Theme.spacingXl
    visible: overlay.active

    background: Rectangle {
        radius: Theme.spacingMd
        color: Theme.cardBackground
        border.width: Theme.hairlineWidth
        border.color: Theme.cardStroke
    }

    contentItem: Column {
        spacing: Theme.spacingMd

        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: overlay.visible
        }

        StyledLabel {
            objectName: overlay.messageObjectName
            anchors.horizontalCenter: parent.horizontalCenter
            text: overlay.message
            textStyle: Theme.bodyFont
            elide: Text.ElideRight
        }
    }
}

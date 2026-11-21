// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Popup/PopupView.swift (topToast, showMiningToast and
// AnkiMiningToastView): status tint, sizing, and move plus fade. Replace the
// glass with a themed floating surface and the SF Symbols with desktop icons.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Layouts
import Niratan

Item {
    id: toast

    required property string status
    required property string message
    property real topInset: 0

    // Retain the last content while the dismissed toast moves offscreen.
    property string displayedStatus: ""
    property string displayedMessage: ""
    onStatusChanged: {
        if (status.length) {
            displayedStatus = status
            displayedMessage = message
        }
    }
    onMessageChanged: {
        if (status.length)
            displayedMessage = message
    }

    readonly property color tint: {
        switch (displayedStatus) {
        case "added": return Theme.popupSuccess
        case "duplicate": return Theme.popupWarning
        case "failed": return Theme.popupDanger
        default: return Theme.popupInfo
        }
    }
    readonly property string title: {
        switch (displayedStatus) {
        case "added": return qsTr("Card Added")
        case "duplicate": return qsTr("Duplicate Found")
        case "failed": return qsTr("Add Failed")
        default: return qsTr("Sent to Anki")
        }
    }
    readonly property string iconName: {
        switch (displayedStatus) {
        case "added": return "emblem-default"
        case "duplicate": return "edit-copy"
        case "failed": return "dialog-warning"
        default: return "mail-send"
        }
    }
    readonly property string fallbackIcon: {
        switch (displayedStatus) {
        case "added": return "\u2713"
        case "duplicate": return "\u29c9"
        case "failed": return "\u26a0\ufe0e"
        default: return "\u27a4"
        }
    }
    readonly property int motionDuration: Math.round(Theme.motionNormal / 1.4)

    anchors.top: parent.top
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.topMargin: Math.max(18, topInset + 18)
    width: Math.max(0, Math.min(420, parent.width - 36))
    height: content.implicitHeight + 28
    z: 1000
    enabled: false
    visible: opacity > 0
    opacity: status.length ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: toast.motionDuration; easing.type: Easing.InOutQuad }
    }
    transform: Translate {
        y: toast.status.length ? 0 : -(toast.height + toast.anchors.topMargin)
        Behavior on y {
            NumberAnimation { duration: toast.motionDuration; easing.type: Easing.InOutQuad }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 28
        color: Qt.tint(Theme.floatingBackground, Qt.alpha(toast.tint, 0.16))
        border.width: Theme.cardBorderWidth
        border.color: Qt.alpha(toast.tint, 0.32)
    }

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 22
        anchors.topMargin: 14
        anchors.bottomMargin: 14
        spacing: 13

        Rectangle {
            Layout.preferredWidth: 42
            Layout.preferredHeight: 42
            radius: width / 2
            color: Qt.alpha(toast.tint, 0.16)

            IconImage {
                id: icon
                anchors.centerIn: parent
                width: 24
                height: 24
                sourceSize.width: 24
                sourceSize.height: 24
                name: toast.iconName
                color: toast.tint
            }
            Text {
                anchors.centerIn: parent
                visible: icon.status !== Image.Ready
                text: toast.fallbackIcon
                color: toast.tint
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Text {
                Layout.fillWidth: true
                text: toast.title
                textFormat: Text.PlainText
                color: Theme.textPrimary
                font.pixelSize: Theme.fontTitle
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: toast.displayedMessage
                textFormat: Text.PlainText
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                maximumLineCount: 2
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }
        }
    }
}

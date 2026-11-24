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
    anchors.topMargin: Math.max(Theme.toastInset, topInset + Theme.toastInset)
    width: Math.max(0, Math.min(Theme.toastMaximumWidth, parent.width - 2 * Theme.toastInset))
    height: content.implicitHeight + 2 * Theme.toastPaddingVertical
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
        radius: Theme.toastRadius
        color: Qt.tint(Theme.floatingBackground, Qt.alpha(toast.tint, 0.16))
        border.width: Theme.cardBorderWidth
        border.color: Qt.alpha(toast.tint, 0.32)
    }

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.leftMargin: Theme.toastInset
        anchors.rightMargin: Theme.toastPaddingTrailing
        anchors.topMargin: Theme.toastPaddingVertical
        anchors.bottomMargin: Theme.toastPaddingVertical
        spacing: Theme.toastSpacing

        Rectangle {
            Layout.preferredWidth: Theme.toastBadgeDiameter
            Layout.preferredHeight: Theme.toastBadgeDiameter
            radius: width / 2
            color: Qt.alpha(toast.tint, 0.16)

            IconImage {
                id: icon
                anchors.centerIn: parent
                width: Theme.toastGlyph
                height: Theme.toastGlyph
                sourceSize.width: Theme.toastGlyph
                sourceSize.height: Theme.toastGlyph
                name: toast.iconName
                color: toast.tint
            }
            StyledLabel {
                anchors.centerIn: parent
                visible: icon.status !== Image.Ready
                text: toast.fallbackIcon
                color: toast.tint
                textStyle: Theme.toastGlyphFont
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.toastLineSpacing

            StyledLabel {
                Layout.fillWidth: true
                text: toast.title
                textFormat: Text.PlainText
                textStyle: Theme.headlineFont
                elide: Text.ElideRight
            }
            StyledLabel {
                Layout.fillWidth: true
                text: toast.displayedMessage
                textFormat: Text.PlainText
                textStyle: Theme.calloutFont
                secondary: true
                maximumLineCount: 2
                wrapMode: Text.Wrap
                elide: Text.ElideRight
            }
        }
    }
}

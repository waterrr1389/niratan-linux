// SPDX-License-Identifier: GPL-3.0-or-later
//
// A modal alert, the Linux form of SwiftUI's .alert(_:isPresented:actions:
// message:) as Features/Reader/Search/ReaderGoToView.swift raises Jump to and
// Invalid input: a narrow card with a headline title, an optional text field,
// a secondary message and its buttons trailing, the default one prominent.
//
// `acceptText` names the default button, which calls accept(); a non-empty
// `rejectText` adds a cancel button that calls reject(). With `hasField` a
// text field is shown; `fieldName` is its object name and `fieldText` its
// text; `selectFieldOnOpen` selects it on open, so typing replaces it.
// Return accepts, Escape rejects (or closes an alert without cancel).

import QtQuick
import QtQuick.Controls
import Niratan

Dialog {
    id: alert

    property string message
    property string acceptText: qsTr("OK")
    property string rejectText
    property bool acceptDestructive: false
    property bool hasField: false
    // Select the field's text once it is focused, so typing replaces it.
    property bool selectFieldOnOpen: false
    property alias fieldText: field.text
    property alias fieldName: field.objectName
    property alias fieldPlaceholder: field.placeholderText

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: Math.min(Theme.alertWidth, parent ? parent.width : Theme.alertWidth)
    padding: Theme.alertPadding
    topPadding: Theme.alertPadding
    bottomPadding: Theme.alertPadding
    focus: true

    onOpened: {
        if (alert.hasField) {
            field.forceActiveFocus()
            if (alert.selectFieldOnOpen)
                field.selectAll()
        }
    }

    background: Rectangle {
        radius: Theme.cardRadius
        color: Theme.cardBackground
        border.width: Theme.hairlineWidth
        border.color: Theme.cardStroke
    }

    header: null
    footer: null

    contentItem: Column {
        spacing: Theme.alertSpacing

        Keys.onReturnPressed: alert.accept()
        Keys.onEnterPressed: alert.accept()

        StyledLabel {
            width: parent.width
            visible: alert.title !== ""
            text: alert.title
            textStyle: Theme.headlineFont
            wrapMode: Text.Wrap
        }

        StyledLabel {
            width: parent.width
            visible: alert.message !== ""
            text: alert.message
            textStyle: Theme.footnoteFont
            secondary: true
            wrapMode: Text.Wrap
        }

        TextField {
            id: field

            width: parent.width
            visible: alert.hasField
            implicitHeight: Theme.fieldHeight
            leftPadding: Theme.fieldPaddingHorizontal
            rightPadding: Theme.fieldPaddingHorizontal
            font.pixelSize: Theme.bodyFont.pixelSize
            color: Theme.textPrimary
            placeholderTextColor: Theme.textTertiary
            onAccepted: alert.accept()

            background: Rectangle {
                radius: Theme.controlRadius
                color: Theme.fieldBackground
                border.width: field.activeFocus ? Theme.focusRingWidth : Theme.hairlineWidth
                border.color: field.activeFocus ? Theme.accent : Theme.controlStroke
            }
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.capsuleSpacing

            FormButton {
                visible: alert.rejectText !== ""
                text: alert.rejectText
                sizeToLabel: true
                onClicked: alert.reject()
            }

            FormButton {
                text: alert.acceptText
                sizeToLabel: true
                prominent: !alert.acceptDestructive
                destructive: alert.acceptDestructive
                onClicked: alert.accept()
            }
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pop-up button in the Niratan control style, after the macOS pop-up button
// and Mac Niratan's toolbar pickers (Pictures/video.png): a capsule like the
// push button with the current text and an up-down chevron, opening a menu
// that marks the current item.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import Niratan

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    implicitContentWidthPolicy: T.ComboBox.WidestText
    padding: 4
    leftPadding: 12 + (control.mirrored && indicator ? indicator.width + spacing : 0)
    rightPadding: 10 + (!control.mirrored && indicator ? indicator.width + spacing : 0)
    spacing: 6
    opacity: enabled ? 1 : 0.45

    delegate: T.ItemDelegate {
        id: row

        required property var model
        required property int index

        width: ListView.view.width
        implicitHeight: 24
        leftPadding: 24
        rightPadding: 12
        text: row.model[control.textRole]
        font: control.font
        highlighted: control.highlightedIndex === row.index
        hoverEnabled: control.hoverEnabled

        contentItem: Text {
            text: row.text
            font: row.font
            color: row.highlighted ? Theme.textOnAccent : Theme.textPrimary
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 5
            color: row.highlighted ? Theme.accent : "transparent"

            Icon {
                x: 7
                anchors.verticalCenter: parent.verticalCenter
                size: 12
                name: "checkmark"
                color: row.highlighted ? Theme.textOnAccent : Theme.textPrimary
                visible: control.currentIndex === row.index
            }
        }
    }

    indicator: Icon {
        x: control.mirrored ? control.padding + 6 : control.width - width - 10
        y: control.topPadding + (control.availableHeight - height) / 2
        size: 12
        name: "chevron.up.chevron.down"
        color: Theme.textSecondary
    }

    contentItem: T.TextField {
        text: control.editable ? control.editText : control.displayText
        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        selectByMouse: control.selectTextByMouse
        font: control.font
        color: Theme.textPrimary
        selectionColor: Theme.accent
        selectedTextColor: Theme.textOnAccent
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: Theme.controlHeight
        radius: height / 2
        visible: !control.flat || control.down || control.hovered
        color: control.editable ? Theme.fieldBackground : Theme.controlBackground
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.controlStroke

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.down ? Theme.pressTint
                                : control.hovered && !control.editable ? Theme.hoverTint
                                                                       : "transparent"
        }
    }

    popup: T.Popup {
        y: control.height + 4
        // As wide as the widest item, as a macOS pop-up menu is; the margins
        // keep a long or wide list inside the window.
        width: Math.max(control.width, control.implicitContentWidth + 24 + 12 + leftPadding + rightPadding)
        height: contentItem.implicitHeight + topPadding + bottomPadding
        margins: 6
        padding: 5
        font: control.font

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0

            T.ScrollBar.vertical: ScrollBar {}
        }

        background: FloatingBackground {}
    }
}

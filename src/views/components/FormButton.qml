// SPDX-License-Identifier: GPL-3.0-or-later
//
// A capsule action button. Follows NativeMac/NativeReuseViews.swift
// NativeSettingsActionButtonStyle: body semibold on one line, padding 16,
// 32 high, primary text or the danger colour for a destructive action, 0.58
// opacity when disabled and scaled to 0.97 while pressed. Glass is the solid
// control surface with a hairline; the pressed tint follows
// NativeSettingsActionButtonGlassSurface.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.AbstractButton {
    id: control

    property bool destructive: false
    // An SF Symbol name drawn before the text, as NativeSettingsActionButton
    // labels Import and Create Profile.
    property string glyph
    // The default action of a dialog: accent fill with on-accent text.
    property bool prominent: false
    property bool sizeToLabel: false
    // controlSize(.small): caption semibold on a chip-high capsule, as
    // ReaderGoToView.bookSummary draws Jump to.
    property bool small: false

    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: Math.max(small ? Theme.chipHeight : Theme.capsuleHeight,
                             implicitContentHeight + topPadding + bottomPadding)
    leftPadding: small ? Theme.chipPaddingHorizontal : Theme.buttonPaddingHorizontal
    rightPadding: small ? Theme.chipPaddingHorizontal : Theme.buttonPaddingHorizontal
    opacity: enabled ? 1 : Theme.buttonDisabledOpacity
    scale: down ? Theme.buttonPressedScale : 1

    Behavior on scale {
        NumberAnimation { duration: Theme.motionFast }
    }

    contentItem: Item {
        implicitWidth: control.sizeToLabel
            ? label.implicitWidth + (control.glyph !== "" ? glyphIcon.width + labelRow.spacing : 0)
            : labelRow.implicitWidth
        implicitHeight: labelRow.implicitHeight

        Row {
            id: labelRow

            anchors.centerIn: parent
            spacing: Theme.chipGlyphSpacing

            Icon {
                id: glyphIcon
                anchors.verticalCenter: parent.verticalCenter
                visible: control.glyph !== ""
                name: control.glyph
                size: control.small ? Theme.chipGlyphSize : Theme.glyphSize
                color: label.color
            }

            StyledLabel {
                id: label

                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, Math.min(implicitWidth, control.availableWidth
                                - (control.glyph !== "" ? (control.small ? Theme.chipGlyphSize : Theme.glyphSize)
                                                         + Theme.chipGlyphSpacing : 0)))
                text: control.text
                textStyle: control.small ? Theme.captionFont : Theme.bodyFont
                weight: Theme.weightSemibold
                color: control.destructive ? Theme.popupDanger
                       : control.prominent ? Theme.textOnAccent : Theme.textPrimary
                elide: Text.ElideRight
            }
        }
    }

    background: Rectangle {
        radius: height / 2
        color: control.prominent ? Theme.accent : Theme.controlBackground
        border.width: control.visualFocus ? Theme.focusRingWidth : Theme.hairlineWidth
        border.color: control.visualFocus ? Theme.accent : Theme.controlStroke

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.down ? Theme.pressTint : control.hovered ? Theme.hoverTint : "transparent"
        }
    }
}

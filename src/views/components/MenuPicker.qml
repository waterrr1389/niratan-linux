// SPDX-License-Identifier: GPL-3.0-or-later
//
// A capsule pop-up button over a menu of strings. Follows
// NativeMac/NativeReuseViews.swift NativeGlassMenuPicker: the current text in
// body semibold primary and a 12 px chevron.up.chevron.down glyph 8 apart,
// padding 14, at least `minimumWidth` wide, one line elided. The open menu
// lists the model with a trailing checkmark on the current item. activated is
// emitted on a user choice only; the page writes currentIndex back. Glass is
// the solid control surface with a hairline.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import Niratan

T.AbstractButton {
    id: picker

    property var model: []
    property int currentIndex: -1
    property real minimumWidth: Theme.pickerMinimumWidth
    // A placeholder is not a menu choice. An unavailable stored value can
    // also be shown here until the user chooses a replacement.
    property string placeholderText
    property bool constrainMenuHeight: false

    readonly property Menu menu: popupMenu

    signal activated(int index)

    readonly property string currentText: currentIndex >= 0 && model && currentIndex < model.length
                                          ? String(model[currentIndex]) : placeholderText

    implicitWidth: Math.max(minimumWidth, currentLabel.implicitWidth + Theme.capsuleSpacing
                                          + Theme.pickerGlyphSize + leftPadding + rightPadding)
    implicitHeight: Theme.capsuleHeight
    leftPadding: Theme.pickerPaddingHorizontal
    rightPadding: Theme.pickerPaddingHorizontal

    Accessible.role: Accessible.ComboBox
    Accessible.name: currentText

    onClicked: popupMenu.popup(picker, 0, picker.height + Theme.spacingXs)

    background: Rectangle {
        radius: height / 2
        color: Theme.controlBackground
        border.width: Theme.hairlineWidth
        border.color: picker.visualFocus ? Theme.accent : Theme.controlStroke

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: picker.down ? Theme.pressTint : picker.hovered ? Theme.hoverTint : "transparent"
        }
    }

    // The text and glyph are centred together, as SwiftUI centres a label
    // inside its minimum-width frame.
    contentItem: Item {
        id: content

        implicitHeight: currentLabel.implicitHeight

        Row {
            anchors.centerIn: parent
            spacing: Theme.capsuleSpacing

            StyledLabel {
                id: currentLabel

                width: Math.min(implicitWidth,
                                content.width - Theme.capsuleSpacing - Theme.pickerGlyphSize)
                anchors.verticalCenter: parent.verticalCenter
                text: picker.currentText
                weight: Theme.weightSemibold
                elide: Text.ElideRight
            }

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "chevron.up.chevron.down"
                size: Theme.pickerGlyphSize
                color: Theme.textPrimary
            }
        }
    }

    Menu {
        id: popupMenu

        Binding {
            target: popupMenu
            property: "height"
            when: picker.constrainMenuHeight
            value: Math.min(popupMenu.implicitHeight,
                            (picker.Window.window?.height ?? popupMenu.implicitHeight)
                            - popupMenu.topMargin - popupMenu.bottomMargin)
        }

        Instantiator {
            model: picker.model

            delegate: MenuItem {
                id: item

                required property int index
                required property var modelData

                text: String(item.modelData)
                checked: item.index === picker.currentIndex
                leftPadding: Theme.spacingMd
                rightPadding: Theme.spacingMd + Theme.pickerGlyphSize + Theme.spacingSm
                onTriggered: picker.activated(item.index)

                indicator: Icon {
                    x: item.width - width - Theme.spacingMd
                    y: item.topPadding + (item.availableHeight - height) / 2
                    size: Theme.pickerGlyphSize
                    name: "checkmark"
                    color: item.highlighted ? Theme.textOnAccent : Theme.textPrimary
                    visible: item.checked
                }
            }

            onObjectAdded: (index, object) => popupMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => popupMenu.removeItem(object)
        }
    }
}

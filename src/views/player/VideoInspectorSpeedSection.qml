// SPDX-License-Identifier: GPL-3.0-or-later
//
// Playback Speed, following VideoInspectorView.swift `videoTab`'s speed block
// and `VideoInspectorChipButtonStyle`: the current `Nx` label, two rows of
// preset chips, the tortoise/hare slider and the custom field with its `x`.
// Presets, slider and field go through the Inspector model; its
// `speedRequested` reaches the page through the panel.
//
// Object names: inspectorSpeedPreset (repeated; `speedValue`, `selected`),
// inspectorSpeedSlider, inspectorSpeedField.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan
import NiratanStyle as Style

VideoInspectorSection {
    id: section

    required property var model

    signal speedRequested(real value)

    function presetText(speed) {
        if (Math.abs(speed - 0.25) < 0.001)
            return "0.25x"
        if (Math.abs(speed - Math.trunc(speed)) < 0.001)
            return String(Math.trunc(speed)) + "x"
        return speed.toFixed(1) + "x"
    }

    sectionTitle: qsTr("Playback Speed")
    headerAccessory: StyledLabel {
        text: section.model.speedLabel
        textStyle: Theme.captionFont
        weight: Theme.weightSemibold
        secondary: true
        monospacedDigits: true
    }

    Connections {
        target: section.model

        function onSpeedRequested(value) {
            section.speedRequested(value)
        }
    }

    Column {
        width: parent ? parent.width : 0
        padding: Theme.compactRowPadding
        spacing: Theme.compactRowSpacing

        Column {
            width: parent.width - 2 * parent.padding
            spacing: Theme.chipSpacing

            Repeater {
                model: section.model.speedRows

                delegate: Row {
                    id: speedRow

                    required property var modelData

                    width: parent ? parent.width : 0
                    spacing: Theme.chipSpacing

                    Repeater {
                        model: speedRow.modelData

                        delegate: ChipButton {
                            id: preset

                            required property var modelData
                            property real speedValue: modelData

                            objectName: "inspectorSpeedPreset"
                            width: (speedRow.width - (speedRow.modelData.length - 1) * speedRow.spacing)
                                   / speedRow.modelData.length
                            implicitHeight: Theme.largeChipHeight
                            text: section.presetText(speedValue)
                            font.features: { "tnum": 1 }
                            selected: section.model.speedLabel !== ""
                                      && section.model.isSpeedSelected(modelData)
                            onClicked: section.model.chooseSpeed(modelData)
                        }
                    }
                }
            }
        }

        Row {
            id: sliderRow

            width: parent.width - 2 * parent.padding
            spacing: Theme.compactRowSpacing

            StyledLabel {
                id: slow

                anchors.verticalCenter: parent.verticalCenter
                text: "0.3×"
                textStyle: Theme.captionFont
                secondary: true
            }

            Style.Slider {
                objectName: "inspectorSpeedSlider"
                anchors.verticalCenter: parent.verticalCenter
                width: sliderRow.width - slow.width - fast.width - field.width - 3 * sliderRow.spacing
                small: true
                from: 0.3
                to: 5.0
                stepSize: 0.1
                snapMode: Slider.SnapAlways
                value: section.model.speed >= 0.3 ? section.model.speed : 0.3
                Accessible.name: qsTr("Playback Speed")
                onMoved: section.model.chooseSpeed(value)
            }

            StyledLabel {
                id: fast

                anchors.verticalCenter: parent.verticalCenter
                text: "5×"
                textStyle: Theme.captionFont
                secondary: true
            }

            InlineField {
                id: field

                anchors.verticalCenter: parent.verticalCenter
                fieldName: "inspectorSpeedField"
                suffix: "x"
                text: section.model.speedInputText
                onEdited: text => section.model.setSpeedInput(text)
                onAccepted: text => {
                    section.model.setSpeedInput(text)
                    section.model.commitSpeedInput()
                }
            }
        }
    }
}

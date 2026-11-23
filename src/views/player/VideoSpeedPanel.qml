// SPDX-License-Identifier: GPL-3.0-or-later
//
// The playback-speed popup, following Features/Video/VideoControlsView.swift
// `speedControlPanel`, `speedPresetRows`, `setSpeed`, `synchronizeSpeedInput`
// and `commitSpeedInput`, and Features/Video/Playback/PlaybackEngine.swift
// `VideoPlaybackSpeed`.
//
// 264 wide, header "Playback Speed" with the current label, the eight presets
// 0.25, 0.5, 1, 1.5, 2, 3, 4, 5 in two rows of four, the 0.3...5 slider in 0.1
// steps and a custom field. The field is trimmed, a comma becomes a dot, and a
// value that is not a number resets to the current label; otherwise
// `VideoPlaybackSpeed.normalized` applies: at most 0.25 stays 0.25, else round
// to 0.1 and clamp to 0.3...5. The label is 0.25, a whole number without
// decimals, or one decimal otherwise. Committing emits `speedChangeRequested`.
// A preset button has objectName speedPreset, exposes `speedValue` and
// `selected`, and prints the speed label with "x"; `inputText` is the custom
// field's contents and `commitInput()` applies it. The slider is speedSlider,
// from 0.3 to 5 in 0.1 steps.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Rectangle {
    id: panel

    objectName: "speedPanel"

    property real speed: 1
    property string inputText: ""

    readonly property string currentLabel: panel.label(panel.speed)
    readonly property var presets: [0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0]
    readonly property real sliderValue: Math.min(5, Math.max(0.3, panel.normalized(panel.speed)))

    implicitWidth: 264
    implicitHeight: column.implicitHeight + 28
    color: Theme.popupBackground
    radius: 20
    border.width: Theme.cardBorderWidth
    border.color: Theme.popupDivider

    signal speedChangeRequested(real value)

    Component.onCompleted: inputText = panel.label(panel.speed)

    onSpeedChanged: inputText = panel.label(panel.speed)

    function normalized(value) {
        if (!isFinite(value))
            return 1
        if (value <= 0.25)
            return 0.25
        return Math.min(5, Math.max(0.3, Math.round(value * 10) / 10))
    }

    function label(value) {
        const normalizedValue = panel.normalized(value)
        if (normalizedValue === 0.25)
            return "0.25"
        if (normalizedValue === Math.trunc(normalizedValue))
            return String(Math.trunc(normalizedValue))
        return normalizedValue.toFixed(1)
    }

    function isPresetSelected(value) {
        return Math.abs(panel.normalized(panel.speed) - panel.normalized(value)) < 0.001
    }

    function setSpeed(value) {
        const normalizedValue = panel.normalized(value)
        inputText = panel.label(normalizedValue)
        panel.speedChangeRequested(normalizedValue)
    }

    function commitInput() {
        const text = String(panel.inputText).trim().replace(/,/g, ".")
        if (text.length === 0) {
            inputText = panel.label(panel.speed)
            return
        }
        const value = Number(text)
        if (isNaN(value)) {
            inputText = panel.label(panel.speed)
            return
        }
        panel.setSpeed(value)
    }

    HoverHandler {
        id: panelHover
    }

    Column {
        id: column

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 10

        RowLayout {
            width: parent.width
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Playback Speed")
                color: Theme.popupText
                font.pixelSize: 13
                font.bold: true
            }

            Label {
                text: panel.currentLabel
                color: Theme.popupTextSecondary
                font.pixelSize: 13
                font.features: ({ "tnum": 1 })
            }
        }

        GridLayout {
            width: parent.width
            columns: 4
            columnSpacing: 6
            rowSpacing: 6

            Repeater {
                model: panel.presets

                delegate: Button {
                    id: preset

                    required property var modelData

                    objectName: "speedPreset"
                    readonly property real speedValue: preset.modelData
                    readonly property bool selected: panel.isPresetSelected(preset.modelData)

                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    focusPolicy: Qt.NoFocus
                    text: panel.label(preset.modelData) + "x"
                    onClicked: panel.setSpeed(preset.modelData)

                    contentItem: Text {
                        text: preset.text
                        color: preset.selected ? Theme.windowBackground : Theme.popupText
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 6
                        color: preset.selected ? Theme.accent : Theme.popupDivider
                    }
                }
            }
        }

        RowLayout {
            width: parent.width
            spacing: 8

            Slider {
                id: speedSlider

                objectName: "speedSlider"
                Layout.fillWidth: true
                focusPolicy: Qt.NoFocus
                from: 0.3
                to: 5.0
                stepSize: 0.1
                value: panel.sliderValue
                onMoved: panel.setSpeed(speedSlider.value)
            }

            TextField {
                id: speedInput

                Layout.preferredWidth: 62
                objectName: "speedInput"
                text: panel.inputText
                placeholderText: qsTr("Custom")
                Accessible.name: qsTr("Custom playback speed")
                color: Theme.popupTextSecondary
                font.pixelSize: 11
                horizontalAlignment: TextInput.AlignRight
                onTextChanged: panel.inputText = speedInput.text
                onAccepted: panel.commitInput()
            }

            Label {
                text: "x"
                color: Theme.popupTextSecondary
            }
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Subtitle Timing, following VideoInspectorView.swift
// `subtitleTimingSection` and `VideoInspectorStepperDisplay`: the footer
// sentence, the Back/Forward 50 and 1000 ms steps around the signed value, the
// -10000...10000 ms slider and the `Offset (ms)` field with its keyboard
// glyph. Every change goes through the Inspector model.
//
// Object names: inspectorSubtitleBack1000, inspectorSubtitleBack50,
// inspectorSubtitleForward50, inspectorSubtitleForward1000,
// inspectorSubtitleValue, inspectorSubtitleSlider,
// inspectorSubtitleOffsetField.

import QtQuick
import QtQuick.Controls
import Niratan
import NiratanStyle as Style

VideoInspectorSection {
    id: section

    required property var model

    sectionTitle: qsTr("Subtitle Timing")
    footer: qsTr("Positive values delay subtitles; negative values show subtitles earlier.")

    Column {
        width: parent ? parent.width : 0
        bottomPadding: Theme.compactRowPadding

        StepperDisplay {
            valueName: "inspectorSubtitleValue"
            valueText: section.model.subtitleValueText
            neutral: section.model.subtitleMilliseconds === 0
            decrements: [
                { name: "inspectorSubtitleBack1000", glyph: "chevron.left.2", help: qsTr("Back 1000 ms"), step: -1000 },
                { name: "inspectorSubtitleBack50", glyph: "chevron.left", help: qsTr("Back 50 ms"), step: -50 }
            ]
            increments: [
                { name: "inspectorSubtitleForward50", glyph: "chevron.right", help: qsTr("Forward 50 ms"), step: 50 },
                { name: "inspectorSubtitleForward1000", glyph: "chevron.right.2", help: qsTr("Forward 1000 ms"), step: 1000 }
            ]
            onStepped: step => section.model.stepSubtitleMilliseconds(step)
        }

        Item {
            width: parent ? parent.width : 0
            implicitHeight: subtitleSlider.implicitHeight

            Style.Slider {
                id: subtitleSlider

                objectName: "inspectorSubtitleSlider"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.compactRowPadding
                anchors.rightMargin: Theme.compactRowPadding
                small: true
                from: -10000
                to: 10000
                stepSize: 50
                snapMode: Slider.SnapAlways
                value: section.model.subtitleSliderMilliseconds
                Accessible.name: qsTr("Subtitle Timing")
                onMoved: section.model.setSubtitleSliderMilliseconds(value)
            }
        }
    }

    FormDivider {}

    FormRow {
        glyph: "keyboard"
        label: qsTr("Offset (ms)")

        InlineField {
            fieldName: "inspectorSubtitleOffsetField"
            fieldWidth: Theme.wideInlineFieldWidth
            glyph: "keyboard"
            text: section.model.subtitleInputText
            onEdited: text => section.model.setSubtitleInput(text)
            onAccepted: text => {
                section.model.setSubtitleInput(text)
                section.model.commitSubtitleInput()
            }
        }
    }
}

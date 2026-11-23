// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Inspector's Audio tab, following VideoInspectorView.swift `audioTab`:
// the Audio Track card and Audio Timing.
//
// The track card lists `Off` first, selected when no audio track is, then the
// audio rows; a row click selects that mpv track through the panel. Timing is
// the shared stepper: `%+.1f s` in the accent when non-zero, `-0.5 s`/`+0.5 s`
// steps clamped to +-30 s, and a Reset link disabled at exactly 0.
//
// Object names: inspectorChoiceRow (repeated), inspectorAudioMinus,
// inspectorAudioPlus, inspectorAudioValue, inspectorAudioReset.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Column {
    id: tab

    required property var model

    signal selectTrackRequested(string type, int id)
    signal audioDelayRequested(real seconds)

    spacing: Theme.panelSpacing

    Connections {
        target: tab.model

        function onAudioDelayRequested(seconds) {
            tab.audioDelayRequested(seconds)
        }
    }

    VideoInspectorTrackSection {
        title: qsTr("Audio Track")
        type: "audio"
        emptyAfterOff: true
        tracks: tab.model.audioTracks
        onSelectTrackRequested: (type, id) => tab.selectTrackRequested(type, id)
    }

    VideoInspectorSection {
        sectionTitle: qsTr("Audio Timing")

        StepperDisplay {
            valueName: "inspectorAudioValue"
            valueText: tab.model.audioValueText
            neutral: tab.model.audioNeutral
            decrements: [{ name: "inspectorAudioMinus", glyph: "minus", help: qsTr("-0.5 s"), step: -0.5 }]
            increments: [{ name: "inspectorAudioPlus", glyph: "plus", help: qsTr("+0.5 s"), step: 0.5 }]
            onStepped: step => tab.model.stepAudioDelay(step)
        }

        FormDivider {}

        Item {
            width: parent ? parent.width : 0
            implicitHeight: reset.implicitHeight + 2 * Theme.spacingSm

            LinkButton {
                id: reset

                objectName: "inspectorAudioReset"
                small: true
                anchors.centerIn: parent
                text: qsTr("Reset")
                enabled: !tab.model.audioNeutral
                onClicked: tab.model.setAudioDelay(0)
            }
        }
    }
}

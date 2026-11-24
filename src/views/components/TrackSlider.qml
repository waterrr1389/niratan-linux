// SPDX-License-Identifier: GPL-3.0-or-later
//
// A thin capsule track that grows and shows a round knob while hovered or
// dragged, following Features/Video/VideoControlsView.swift
// `VideoTimelineTrack` as `VideoVolumeTrack` uses it: 3 px at rest, 5 px when
// emphasized, a white knob (Theme.trackSliderKnob) whose centre sits on the filled end and may
// overhang the track's ends. A press anywhere on the track jumps there.
//
// It is a Slider underneath, so `value`, `from`, `to`, `stepSize` and moved()
// keep their usual meaning. `onOverlay` is the compact layout's control
// treatment: the track and fill take the overlay foreground so they read over
// the video's dark scrim in both themes, as VideoVolumeTrack passes
// controlTreatment through to VideoTimelineTrack.

import QtQuick
import QtQuick.Templates as T
import Niratan

T.Slider {
    id: control

    readonly property bool emphasized: hovered || pressed
    property bool onOverlay: false

    implicitWidth: Theme.trackSliderWidth
    implicitHeight: Theme.trackSliderHitHeight
    padding: 0
    hoverEnabled: true

    background: Item {
        Rectangle {
            id: track

            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: control.emphasized ? Theme.trackSliderEmphasizedHeight : Theme.trackSliderHeight
            radius: height / 2
            color: control.onOverlay ? Theme.chromeOverlayTrack : Theme.cardStroke
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(track.height, control.visualPosition * parent.width)
            height: track.height
            radius: height / 2
            color: control.onOverlay ? Theme.overlayForeground : Theme.textPrimary
            opacity: Theme.trackSliderFillOpacity
        }
    }

    handle: Rectangle {
        x: control.visualPosition * control.availableWidth - width / 2
        y: (control.availableHeight - height) / 2
        width: Theme.trackSliderKnob
        height: Theme.trackSliderKnob
        radius: width / 2
        visible: control.emphasized
        color: Theme.knob
        border.width: Theme.hairlineWidth
        border.color: Theme.controlStroke
    }
}

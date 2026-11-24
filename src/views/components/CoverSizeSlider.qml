// SPDX-License-Identifier: GPL-3.0-or-later
//
// The cover size control of a library toolbar popover, following
// NativeMac/NativeReuseViews.swift `LibraryCoverSizeButton`: a small and a
// large photo glyph around a slider from 110 to 240 in steps of 10. `moved`
// reports the rounded width the user picked.
import QtQuick
import QtQuick.Controls
import Niratan

Row {
    id: control

    property real value: Theme.videoPosterDefaultWidth
    property alias sliderObjectName: slider.objectName

    signal moved(int value)

    spacing: Theme.coverSizeSpacing

    Icon {
        anchors.verticalCenter: parent.verticalCenter
        name: "photo"
        size: Theme.coverSizeSmallGlyph
        color: Theme.textSecondary
    }

    Slider {
        id: slider

        width: Theme.coverSizeSliderWidth
        anchors.verticalCenter: parent.verticalCenter
        from: Theme.coverSizeMinimum
        to: Theme.coverSizeMaximum
        stepSize: Theme.coverSizeStep
        value: control.value
        onMoved: control.moved(Math.round(value))
    }

    Icon {
        anchors.verticalCenter: parent.verticalCenter
        name: "photo"
        size: Theme.coverSizeLargeGlyph
        color: Theme.textSecondary
    }
}

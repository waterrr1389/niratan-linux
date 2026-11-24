// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player's on-screen display, following
// Features/Video/VideoPlayerScreen+OSD.swift: inset from the top and leading
// edge of the video stage, a title with an optional value and a meter under
// them, fading out with the page's OSD timer. The panel behind it is the
// floating theme surface so the text stays readable over any frame; the text
// uses the kit's headline and callout roles and the meter is the kit's
// progress track.
pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Rectangle {
    id: osd

    property string title
    property string value
    property real meter: -1

    anchors.topMargin: Theme.osdTopInset
    anchors.leftMargin: Theme.osdLeadingInset
    width: content.implicitWidth
    height: content.implicitHeight
    radius: Theme.osdRadius
    color: Theme.floatingBackground
    border.width: Theme.cardBorderWidth
    border.color: Theme.floatingStroke
    opacity: osd.title.length > 0 ? 1 : 0
    visible: osd.title.length > 0 || opacity > 0

    Behavior on opacity {
        NumberAnimation {
            duration: 120
        }
    }

    Column {
        id: content

        padding: Theme.osdPadding
        spacing: Theme.osdSpacing

        Row {
            spacing: Theme.spacingSm

            StyledLabel {
                text: osd.title
                textStyle: Theme.headlineFont
            }

            StyledLabel {
                text: osd.value
                textStyle: Theme.calloutFont
                secondary: true
                monospacedDigits: true
            }
        }

        ProgressTrack {
            id: meterBar

            visible: osd.meter >= 0
            width: Theme.osdMeterWidth
            value: osd.meter
        }
    }
}

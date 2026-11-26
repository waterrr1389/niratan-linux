// SPDX-License-Identifier: GPL-3.0-or-later
//
// VideoMiningHistorySidebar.swift transcriptToolbar's "Align to Current Time"
// row. The toolbar appears from the unfiltered transcript rows and each button
// follows its own cue-direction availability.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

RowLayout {
    id: toolbar

    objectName: "transcriptAlignToolbar"
    required property PlayerStudyViewModel study
    visible: toolbar.study.hasTranscriptRows
    spacing: Theme.spacingXs

    StyledLabel {
        Layout.fillWidth: true
        textStyle: Theme.captionFont
        secondary: true
        elide: Text.ElideRight
        text: qsTr("Align to Current Time")
    }
    FormButton {
        objectName: "alignPreviousSubtitleButton"
        small: true
        sizeToLabel: true
        text: qsTr("Previous")
        enabled: toolbar.study.canAlignPreviousSubtitle
        ToolTip.text: qsTr("Align Previous Subtitle to Current Time")
        onClicked: toolbar.study.alignPreviousSubtitleToCurrentTime()
    }
    FormButton {
        objectName: "alignNextSubtitleButton"
        small: true
        sizeToLabel: true
        text: qsTr("Next")
        enabled: toolbar.study.canAlignNextSubtitle
        ToolTip.text: qsTr("Align Next Subtitle to Current Time")
        onClicked: toolbar.study.alignNextSubtitleToCurrentTime()
    }
}

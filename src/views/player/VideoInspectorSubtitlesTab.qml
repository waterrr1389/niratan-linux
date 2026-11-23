// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Inspector's Subtitles tab, following VideoInspectorView.swift
// `subtitlesTab`, `subtitleSourcesSection`, `subtitleTrackDetail`,
// `subtitleTimingSection` and `subtitleAppearanceSection`.
//
// Subtitle Track lists `Off` first, then the subtitle rows with the codec or
// `External Subtitles · codec` detail; `Open Subtitles` opens a file dialog and
// hands the chosen file to the page. There is no online `Find Subtitles` row
// (Decision). Subtitle Timing steps the delay by 50/1000 ms, follows the
// slider and the `Offset (ms)` field, all through the Inspector model whose
// requests reach the page as this tab's signals. Subtitle Appearance writes
// the stored `videoSubtitle*` keys directly through the engine; Restore
// Defaults writes UserConfig's appearance defaults. The mask rows are not
// drawn (Decision).
//
// Object names: inspectorChoiceRow (repeated), inspectorOpenSubtitles,
// inspectorSubtitleFileDialog, inspectorOpenTranscript, and those of
// VideoInspectorSubtitleTiming and VideoInspectorSubtitleAppearance.

import QtQuick
import QtQuick.Dialogs
import Niratan

Column {
    id: tab

    required property var model
    required property var engine

    signal selectTrackRequested(string type, int id)
    signal subtitleDelayRequested(real seconds)
    signal openSubtitleFileRequested(var file)
    signal openTranscriptRequested()

    spacing: Theme.panelSpacing

    Connections {
        target: tab.model

        function onSubtitleDelayRequested(seconds) {
            tab.subtitleDelayRequested(seconds)
        }
    }

    VideoInspectorTrackSection {
        id: tracks

        title: qsTr("Subtitle Track")
        type: "sub"
        tracks: tab.model.subtitleTracks
        onSelectTrackRequested: (type, id) => tab.selectTrackRequested(type, id)

        FormDivider {}

        Item {
            width: parent ? parent.width : 0
            implicitHeight: openButton.implicitHeight + 2 * Theme.compactSectionPadding

            ChipButton {
                id: openButton

                objectName: "inspectorOpenSubtitles"
                x: Theme.compactSectionPadding
                width: parent.width - 2 * Theme.compactSectionPadding
                anchors.verticalCenter: parent.verticalCenter
                implicitHeight: Theme.largeChipHeight
                glyph: "plus"
                text: qsTr("Open Subtitles")
                onClicked: subtitleDialog.open()
            }
        }
    }

    VideoInspectorSubtitleTiming {
        model: tab.model
    }

    VideoInspectorSubtitleAppearance {
        engine: tab.engine
    }

    VideoInspectorSection {
        sectionTitle: qsTr("Transcript")

        FormRow {
            objectName: "inspectorOpenTranscript"
            glyph: "text.alignleft"
            label: qsTr("Open Transcript")
            clickable: true
            onTapped: clicked()

            signal clicked()

            onClicked: tab.openTranscriptRequested()

            Icon {
                name: "chevron.right"
                size: Theme.tinyGlyphSize
                color: Theme.textTertiary
            }
        }
    }

    FileDialog {
        id: subtitleDialog

        objectName: "inspectorSubtitleFileDialog"
        title: qsTr("Open Subtitles")
        nameFilters: [qsTr("Subtitle files (*.srt *.ass *.ssa *.vtt)"), qsTr("All files (*)")]
        onAccepted: tab.openSubtitleFileRequested(selectedFile)
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// One track card, following VideoInspectorView.swift `trackSection`: the
// model's rows for one track type (Off first where the model adds it), a
// divider between rows, and `No tracks` when the list is empty. A row click
// asks for that track.
//
// Object names: inspectorChoiceRow (repeated, with rowType, rowId, title,
// detail and selected).

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

VideoInspectorSection {
    id: card

    property string type
    property var tracks: []
    // The model adds Off first for audio and subtitles; only Off means no
    // tracks.
    readonly property bool onlyOff: tracks.length === 1 && tracks[0].id === 0
    // Audio shows No tracks under a lone Off; subtitles keep Open Subtitles
    // instead.
    property bool emptyAfterOff: false

    signal selectTrackRequested(string type, int id)

    Repeater {
        model: card.tracks

        delegate: Column {
            id: trackRow

            required property int index
            required property var modelData

            width: parent ? parent.width : 0

            FormDivider {
                visible: trackRow.index > 0
            }

            FormChoiceRow {
                objectName: "inspectorChoiceRow"
                rowType: card.type
                rowId: trackRow.modelData.id
                title: trackRow.modelData.title
                detail: trackRow.modelData.detail
                selected: trackRow.modelData.selected
                onClicked: card.selectTrackRequested(card.type, trackRow.modelData.id)
            }
        }
    }

    FormDivider {
        visible: card.emptyAfterOff && card.onlyOff
    }

    StyledLabel {
        width: parent ? parent.width : 0
        height: Theme.emptyRowHeight
        visible: card.tracks.length === 0 || (card.emptyAfterOff && card.onlyOff)
        text: qsTr("No tracks")
        textStyle: Theme.calloutFont
        secondary: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

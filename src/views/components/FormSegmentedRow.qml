// SPDX-License-Identifier: GPL-3.0-or-later
//
// A row with a SegmentedPicker. Follows Features/Settings/AppearanceView.swift:
// the Interface row puts the picker in the trailing column, and the Theme
// card spans the row with a fills-width picker padded 12 by 10 when
// `fillsWidth` is set and there is no label. activated is forwarded from the
// picker on a user choice only.
import QtQuick
import Niratan

FormRow {
    id: row

    property var model: []
    property int currentIndex: -1
    property real minimumSegmentWidth: Theme.segmentMinimumWidth
    property bool fillsWidth: false
    property var enabledSegments: []

    signal activated(int index)

    readonly property bool spans: fillsWidth && label === ""

    accessoryItem: picker
    fillsRow: spans

    SegmentedPicker {
        id: picker

        width: row.spans ? row.width - 2 * Theme.fillRowPaddingHorizontal : implicitWidth
        model: row.model
        currentIndex: row.currentIndex
        minimumSegmentWidth: row.minimumSegmentWidth
        fillsWidth: row.spans
        enabledSegments: row.enabledSegments
        Accessible.name: row.label
        onActivated: index => row.activated(index)
    }
}

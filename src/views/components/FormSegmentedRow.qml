// SPDX-License-Identifier: GPL-3.0-or-later
//
// A row with a SegmentedPicker. Follows Features/Settings/AppearanceView.swift:
// the Interface row puts the picker in the trailing column, and the Theme
// card spans the row with a fills-width picker padded 12 by 10 when
// `fillsWidth` is set and there is no label. A labelled row too narrow for
// label and picker on one line stacks the picker under the label, full
// width, as the reader's side panel needs. activated is forwarded from the
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
    // A labelled picker whose titles cannot share the line with the label
    // moves below it and fills the row, so neither label nor titles are cut.
    stacked: label !== "" && width > 0
             && labelX + labelItem.implicitWidth + spacing + minimumGap
                + picker.naturalWidth + padding > width

    accessoryItem: picker
    fillsRow: spans

    SegmentedPicker {
        id: picker

        width: row.spans || row.stacked ? row.width - 2 * Theme.fillRowPaddingHorizontal
                                        : implicitWidth
        model: row.model
        currentIndex: row.currentIndex
        minimumSegmentWidth: row.minimumSegmentWidth
        fillsWidth: row.spans || row.stacked
        enabledSegments: row.enabledSegments
        Accessible.name: row.label
        onActivated: index => row.activated(index)
    }
}

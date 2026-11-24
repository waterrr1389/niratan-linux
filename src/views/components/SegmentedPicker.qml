// SPDX-License-Identifier: GPL-3.0-or-later
//
// A capsule segmented control. Follows NativeMac/NativeReuseViews.swift
// NativeGlassSegmentedPicker: segments in callout semibold on one line,
// padding 10 by 4 and at least `minimumSegmentWidth` wide, inside a capsule
// track padded 2. The selected segment is raised in primary text, the others
// secondary; a disabled segment (`enabledSegments[i] === false`) draws at
// 0.45 and ignores clicks. A 16 px divider at 0.42 separates neighbours and
// is hidden when either neighbour is selected. With `fillsWidth` each
// segment keeps its title width and the rest of the picker's width is shared
// equally, so titles stay whole while they fit at all. Left and Right move to the nearest
// enabled neighbour. activated is emitted on a user choice of another
// segment only; the page writes currentIndex back. Glass is the solid track
// with a raised segment.
pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: picker

    property var model: []
    property int currentIndex: -1
    property real minimumSegmentWidth: Theme.segmentMinimumWidth
    property bool fillsWidth: false
    property var enabledSegments: []

    signal activated(int index)

    readonly property int count: model ? model.length : 0

    // The width the picker needs to show every title in full, independent of
    // fillsWidth; a row compares it with its room to decide whether to stack.
    readonly property real naturalWidth: {
        let total = 2 * Theme.segmentTrackPadding
        for (let index = 0; index < segments.count; ++index) {
            const item = segments.itemAt(index)
            if (item)
                total += item.naturalWidth
        }
        return total
    }

    // Sum of the titles alone; a filling picker shares what is left equally,
    // so a long title keeps its text when the equal split would cut it.
    readonly property real titlesWidth: {
        let total = 0
        for (let index = 0; index < segments.count; ++index) {
            const item = segments.itemAt(index)
            if (item)
                total += item.titleWidth
        }
        return total
    }
    // The segment at index, or null.
    function segmentItem(index: int): Item {
        return segments.itemAt(index)
    }

    // The divider between segment index and index + 1, or null.
    function dividerItem(index: int): Item {
        return dividers.itemAt(index)
    }

    function segmentEnabled(index: int): bool {
        return !enabledSegments || index >= enabledSegments.length || enabledSegments[index] !== false
    }

    function choose(index: int) {
        if (index !== currentIndex && index >= 0 && index < count && segmentEnabled(index))
            activated(index)
    }

    function step(direction: int) {
        for (let index = currentIndex + direction; index >= 0 && index < count; index += direction) {
            if (segmentEnabled(index)) {
                activated(index)
                return
            }
        }
    }

    implicitWidth: segmentRow.implicitWidth + 2 * Theme.segmentTrackPadding
    implicitHeight: segmentRow.implicitHeight + 2 * Theme.segmentTrackPadding

    activeFocusOnTab: true
    Accessible.role: Accessible.PageTabList
    Keys.onLeftPressed: step(-1)
    Keys.onRightPressed: step(1)

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.trackBackground
        border.width: picker.activeFocus ? Theme.focusRingWidth : 0
        border.color: Theme.accent
    }

    Row {
        id: segmentRow

        x: Theme.segmentTrackPadding
        y: Theme.segmentTrackPadding

        Repeater {
            id: segments

            model: picker.model

            delegate: Item {
                id: segment

                required property int index
                required property var modelData

                readonly property bool selected: segment.index === picker.currentIndex
                // The segment's title; tests and assistive tools find a
                // segment by it and click it.
                readonly property string text: String(segment.modelData)

                signal clicked()

                objectName: "segment"
                onClicked: picker.choose(segment.index)
                readonly property real titleWidth: Math.ceil(segmentLabel.implicitWidth)
                readonly property real naturalWidth: Math.max(
                    picker.minimumSegmentWidth,
                    segment.titleWidth + 2 * Theme.segmentPaddingHorizontal)

                width: picker.fillsWidth
                       ? segment.titleWidth
                         + (picker.width - 2 * Theme.segmentTrackPadding - picker.titlesWidth)
                           / Math.max(1, picker.count)
                       : segment.naturalWidth
                implicitHeight: segmentLabel.implicitHeight + 2 * Theme.segmentPaddingVertical
                opacity: picker.segmentEnabled(segment.index) ? 1 : Theme.disabledOpacity

                Accessible.role: Accessible.PageTab
                Accessible.name: segmentLabel.text
                Accessible.selected: segment.selected

                Rectangle {
                    anchors.fill: parent
                    visible: segment.selected
                    radius: height / 2
                    color: Theme.segmentSelected
                    border.width: Theme.hairlineWidth
                    border.color: Theme.controlStroke
                }

                StyledLabel {
                    id: segmentLabel

                    // Centred and cut only when the segment is narrower than
                    // its title; the side padding gives way first.
                    width: Math.min(segment.titleWidth, segment.width)
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter
                    text: segment.text
                    textStyle: Theme.calloutFont
                    weight: Theme.weightSemibold
                    secondary: !segment.selected
                    elide: Text.ElideRight
                }

                TapHandler {
                    enabled: picker.segmentEnabled(segment.index)
                    onTapped: segment.clicked()
                }
            }
        }
    }

    Repeater {
        id: dividers

        model: Math.max(0, picker.count - 1)

        delegate: Rectangle {
            id: divider

            required property int index

            readonly property Item leading: segments.count > divider.index
                                            ? segments.itemAt(divider.index) : null

            x: divider.leading
               ? segmentRow.x + divider.leading.x + divider.leading.width - width / 2 : 0
            y: (picker.height - height) / 2
            width: Theme.hairlineWidth
            height: Theme.segmentDividerHeight
            color: Theme.separator
            opacity: divider.index === picker.currentIndex || divider.index + 1 === picker.currentIndex
                     ? 0 : Theme.segmentDividerOpacity
        }
    }
}

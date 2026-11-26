// SPDX-License-Identifier: GPL-3.0-or-later
//
// One book of the shelf grid, following Features/Bookshelf/BookView.swift
// `ShelfBookCard`, `ShelfCoverFrame` and `ShelfProgressStrip`, and
// BookshelfLayout's sizes. The card is `coverWidth` wide: a glass frame with
// a 7 px corner radius, a 3 px padding, a hairline and a soft shadow holds the
// cover at the 0.709 aspect with a 6 px corner radius, and the progress strip
// under it (a 3 px secondary capsule track and fill with the percentage in
// 9 px medium tabular digits). The title follows in 16 px, or 13 px below a
// 140 px cover, on at most two lines in a 40 px tall box.
//
// A finished book (progress at least 0.999) carries a grey filled check at the
// cover's bottom trailing corner; while selecting, a chosen book carries the
// accent check at the top trailing corner instead. A book without a cover
// shows the grey placeholder.
// BookView.shelfDragAppearance lifts the source and outlines the target.
import QtQuick
import Niratan

Item {
    id: card

    property string title
    property url cover
    property real progress: 0
    property string percent
    property bool finished: false
    property bool chosen: false
    property real coverWidth: Theme.bookCoverDefaultWidth
    property bool dragSource: false
    property bool dragTarget: false

    signal clicked()
    signal contextMenuRequested()

    readonly property real coverImageWidth: card.coverWidth - 2 * Theme.bookFrameInset
    readonly property real coverImageHeight: Math.round(card.coverImageWidth / Theme.bookCoverAspect)
    readonly property bool compact: card.coverWidth < Theme.bookCompactWidth

    width: card.coverWidth
    height: frame.height + Theme.bookTitleSpacing + Theme.bookTitleHeight
    y: dragSource ? -Theme.bookDragLift : 0
    scale: dragSource ? Theme.bookDragSourceScale : dragTarget ? Theme.bookDragTargetScale : 1

    Behavior on y { NumberAnimation { duration: Theme.bookDragDuration } }
    Behavior on scale { NumberAnimation { duration: Theme.bookDragDuration } }

    Accessible.role: Accessible.Button
    Accessible.name: card.title

    // The frame's drop shadow, black at 12% blurred by 3 and lowered by 2 in
    // Swift, drawn as two offset fills so the software renderer shows it too.
    Rectangle {
        x: frame.x - Theme.hairlineWidth
        y: frame.y + Theme.hairlineWidth
        width: frame.width + 2 * Theme.hairlineWidth
        height: frame.height + Theme.bookFrameInset
        radius: Theme.bookFrameRadius + Theme.hairlineWidth
        color: Theme.shadow
        opacity: card.dragSource ? Theme.bookShadowOpacity + Theme.bookDragShadowOpacity
                                 : Theme.bookShadowOpacity
    }

    Rectangle {
        id: frame

        width: card.coverWidth
        height: card.coverImageHeight + Theme.bookStripHeight + 3 * Theme.bookFrameInset
        radius: Theme.bookFrameRadius
        color: Theme.cardBackground

        Rectangle {
            x: Theme.bookFrameInset
            y: Theme.bookFrameInset
            width: card.coverImageWidth
            height: card.coverImageHeight
            radius: Theme.bookCoverRadius
            color: Theme.trackBackground
            visible: coverImage.status !== Image.Ready
        }

        RoundedImage {
            id: coverImage

            x: Theme.bookFrameInset
            y: Theme.bookFrameInset
            width: card.coverImageWidth
            height: card.coverImageHeight
            radius: Theme.bookCoverRadius
            // The mask stops at the frame's edge.
            maskWidth: Theme.bookFrameInset
            maskColor: frame.color
            source: card.cover
            fillMode: Image.PreserveAspectCrop
            sourceSize.width: 2 * width
            sourceSize.height: 2 * height
            visible: card.cover.toString().length > 0
        }

        CheckBadge {
            anchors.right: coverImage.right
            anchors.bottom: coverImage.bottom
            anchors.margins: Theme.bookBadgeInset
            visible: card.finished && !card.chosen
            tint: Theme.systemGray
        }

        CheckBadge {
            anchors.right: coverImage.right
            anchors.top: coverImage.top
            anchors.margins: Theme.bookBadgeInset
            visible: card.chosen
            tint: Theme.accent
        }

        Row {
            x: Theme.bookStripInset
            y: card.coverImageHeight + 2 * Theme.bookFrameInset
            width: card.coverImageWidth - 2 * (Theme.bookStripInset - Theme.bookFrameInset)
            height: Theme.bookStripHeight
            spacing: Theme.videoMetaSpacing

            ProgressTrack {
                width: parent.width - percentText.width - parent.spacing
                height: Theme.bookStripTrackHeight
                anchors.verticalCenter: parent.verticalCenter
                value: card.progress
                trackColor: Theme.shelfProgressTrack
                fillColor: Theme.shelfProgressFill
            }

            StyledLabel {
                id: percentText

                anchors.verticalCenter: parent.verticalCenter
                text: card.percent + "%"
                textStyle: Theme.bookPercentFont
                secondary: true
                monospacedDigits: true
            }
        }

        // The hairline, primary at 6%, drawn over the cover's corner mask.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: Theme.hairlineWidth
            border.color: Theme.cardStroke
        }
    }

    StyledLabel {
        y: frame.height + Theme.bookTitleSpacing
        width: card.coverWidth
        height: Theme.bookTitleHeight
        text: card.title
        textStyle: card.compact ? Theme.calloutFont : Theme.title3Font
        // SF's line height, so two lines fit the title box as in Swift.
        lineHeightMode: Text.FixedHeight
        lineHeight: card.compact ? Theme.bookCompactTitleLineHeight : Theme.bookTitleLineHeight
        maximumLineCount: 2
        wrapMode: Text.Wrap
        elide: Text.ElideRight
        verticalAlignment: Text.AlignTop
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Theme.bookDragOutlineInset
        radius: Theme.bookDragOutlineRadius
        opacity: card.dragTarget ? Theme.bookDragOutlineOpacity : 0
        color: "transparent"
        border.color: Theme.accent
        border.width: Theme.bookDragOutlineWidth
        Behavior on opacity { NumberAnimation { duration: Theme.bookDragDuration } }
    }

    TapHandler {
        onTapped: card.clicked()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: card.contextMenuRequested()
    }
}

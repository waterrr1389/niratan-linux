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
    property real coverWidth: 160

    signal clicked()
    signal contextMenuRequested()

    readonly property real coverImageWidth: card.coverWidth - 6
    readonly property real coverImageHeight: Math.round(card.coverImageWidth / 0.709)

    width: card.coverWidth
    height: frame.height + 6 + 40

    Accessible.role: Accessible.Button
    Accessible.name: card.title

    // The frame's drop shadow, black at 12% blurred by 3 and lowered by 2 in
    // Swift, drawn as two offset fills so the software renderer shows it too.
    Rectangle {
        x: frame.x - 1
        y: frame.y + 1
        width: frame.width + 2
        height: frame.height + 3
        radius: 8
        color: Theme.shadow
        opacity: 0.3
    }

    Rectangle {
        id: frame

        width: card.coverWidth
        height: 3 + card.coverImageHeight + 3 + 10 + 3
        radius: 7
        color: Theme.cardBackground

        Rectangle {
            x: 3
            y: 3
            width: card.coverImageWidth
            height: card.coverImageHeight
            radius: 6
            color: Theme.trackBackground
            visible: coverImage.status !== Image.Ready
        }

        RoundedImage {
            id: coverImage

            x: 3
            y: 3
            width: card.coverImageWidth
            height: card.coverImageHeight
            radius: 6
            // The mask stops at the frame's edge, 3 px out.
            maskWidth: 3
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
            anchors.margins: 6
            visible: card.finished && !card.chosen
            tint: Theme.systemGray
        }

        CheckBadge {
            anchors.right: coverImage.right
            anchors.top: coverImage.top
            anchors.margins: 6
            visible: card.chosen
            tint: Theme.accent
        }

        Row {
            x: 5
            y: 3 + card.coverImageHeight + 3
            width: card.coverImageWidth - 4
            height: 10
            spacing: 5

            Item {
                width: parent.width - percentText.width - parent.spacing
                height: 3
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: Theme.shelfProgressTrack
                }
                Rectangle {
                    width: parent.width * Math.min(Math.max(card.progress, 0), 1)
                    height: parent.height
                    radius: height / 2
                    color: Theme.shelfProgressFill
                }
            }

            Text {
                id: percentText

                anchors.verticalCenter: parent.verticalCenter
                text: card.percent + "%"
                color: Theme.textSecondary
                font.pixelSize: 9
                font.weight: Font.Medium
                font.features: { "tnum": 1 }
            }
        }

        // The hairline, primary at 6%, drawn over the cover's corner mask.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: Theme.cardStroke
        }
    }

    Text {
        y: frame.height + 6
        width: card.coverWidth
        height: 40
        text: card.title
        color: Theme.textPrimary
        font.pixelSize: card.coverWidth < 140 ? 13 : 16
        // SF's line height, so two lines fit the 40 px box as in Swift.
        lineHeightMode: Text.FixedHeight
        lineHeight: card.coverWidth < 140 ? 16 : 19
        maximumLineCount: 2
        wrapMode: Text.Wrap
        elide: Text.ElideRight
        verticalAlignment: Text.AlignTop
    }

    TapHandler {
        onTapped: card.clicked()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: card.contextMenuRequested()
    }

    // checkmark.circle.fill at 22 pt: a white check on a filled circle.
    component CheckBadge: Item {
        property color tint

        width: 22
        height: 22

        Rectangle {
            anchors.centerIn: parent
            width: 16
            height: 16
            radius: 8
            color: Theme.knob
        }
        Icon {
            anchors.fill: parent
            size: 22
            name: "checkmark.circle.fill"
            color: parent.tint
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Inspector's Episodes tab, following VideoInspectorView.swift
// `episodesTab` and `VideoInspectorEpisodeRow`: one card listing the folder's
// episodes numbered from 1, the current one with an accent badge, semibold
// title and a speaker glyph; the count sits on the header line and an empty
// list shows `No episodes`. Clicking a row asks the page to open it.
//
// Object names: inspectorEpisodeRow (with `number`, `isCurrent`,
// `episodeUrl`), inspectorCountLabel, inspectorEmptyLabel.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Column {
    id: tab

    required property var model

    signal selectEpisodeRequested(var url)

    VideoInspectorSection {
        sectionTitle: qsTr("Episodes")
        headerAccessory: StyledLabel {
            objectName: "inspectorCountLabel"
            visible: tab.model.episodes.length > 0
            text: String(tab.model.episodes.length)
            textStyle: Theme.captionFont
            weight: Theme.weightSemibold
            secondary: true
            monospacedDigits: true
        }

        Repeater {
            model: tab.model.episodes

            delegate: Column {
                id: episodeRow

                required property int index
                required property var modelData
                property int number: modelData.number
                property bool isCurrent: modelData.current
                property var episodeUrl: modelData.url

                signal clicked()

                objectName: "inspectorEpisodeRow"
                width: parent ? parent.width : 0
                onClicked: tab.selectEpisodeRequested(episodeUrl)

                FormDivider {
                    visible: episodeRow.index > 0
                }

                Item {
                    width: parent.width
                    height: Math.max(Theme.compactRowMinHeight,
                                     title.implicitHeight + 2 * Theme.rowWrappedPadding)

                    Rectangle {
                        anchors.fill: parent
                        color: rowTap.pressed ? Theme.pressTint
                            : rowHover.hovered ? Theme.hoverTint : "transparent"
                    }

                    HoverHandler {
                        id: rowHover

                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        id: rowTap

                        onTapped: episodeRow.clicked()
                    }

                    Rectangle {
                        id: badge

                        x: Theme.compactRowPadding
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(Theme.episodeBadgeMinWidth,
                                        numberLabel.implicitWidth + Theme.episodeBadgePadding)
                        height: Theme.episodeBadgeHeight
                        radius: Theme.episodeBadgeRadius
                        color: episodeRow.isCurrent ? Theme.accent : Theme.inactiveSelection

                        StyledLabel {
                            id: numberLabel

                            anchors.centerIn: parent
                            text: String(episodeRow.number)
                            textStyle: Theme.captionFont
                            weight: Theme.weightBold
                            monospacedDigits: true
                            color: episodeRow.isCurrent ? Theme.textOnAccent : Theme.textSecondary
                        }
                    }

                    StyledLabel {
                        id: title

                        x: badge.x + badge.width + Theme.compactRowSpacing
                        width: (playing.visible ? playing.x - Theme.compactRowSpacing
                                                : parent.width - Theme.compactRowPadding) - x
                        anchors.verticalCenter: parent.verticalCenter
                        text: String(episodeRow.modelData.title)
                        textStyle: Theme.calloutFont
                        weight: episodeRow.isCurrent ? Theme.weightSemibold : Theme.weightRegular
                        wrapMode: Text.Wrap
                        maximumLineCount: Theme.compactLabelMaximumLines
                        elide: Text.ElideMiddle
                    }

                    Icon {
                        id: playing

                        x: parent.width - Theme.compactRowPadding - width
                        anchors.verticalCenter: parent.verticalCenter
                        visible: episodeRow.isCurrent
                        name: "speaker.wave.2.fill"
                        size: Theme.episodeGlyphSize
                        color: Theme.accent
                    }
                }
            }
        }

        StyledLabel {
            objectName: "inspectorEmptyLabel"
            width: parent ? parent.width : 0
            height: Theme.emptyRowHeight
            visible: tab.model.episodes.length === 0
            text: qsTr("No episodes")
            textStyle: Theme.calloutFont
            secondary: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}

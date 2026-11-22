// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Resources tab of the Sasayaki panel, following
// Features/Sasayaki/SasayakiSheet.swift (resourcesTab) and
// Features/Sasayaki/SasayakiMatchView.swift (SasayakiSubtitleMatchSection:
// fileSection, searchSection, currentMatchSection, matchRate(for:),
// matchButtonLabel, fileNameView) in the `.inspector` presentation of
// NativeMac/NativeReuseViews.swift (NativeSettingsForm, NativeSettingsSectionCard,
// NativeSettingsRow, NativeSettingsSeparator, NativeSettingsButtonRow).
//
// D8 removes the transcription section; D7 removes the ッツ Sync section. The
// file pickers keep the port's filters: audio accepts mp3 and m4b, subtitles
// srt and txt, each with all files. The match button writes through the
// model's matchSubtitles; a failed save or match keeps the exposed state.
//
// The inspector presentation has no glass, so a section is its title over a
// zero-spacing column with a vertical 14 padding and a hairline divider below.
// The form keeps the 18 horizontal and 18 bottom padding.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Niratan

Item {
    id: root

    required property SasayakiViewModel player

    readonly property bool canMatch: player.subtitleName.length > 0 && !player.matching

    FileDialog {
        id: audioDialog
        title: qsTr("Load Audio")
        nameFilters: [qsTr("Audiobooks (*.mp3 *.m4b)"), qsTr("All files (*)")]
        onAccepted: root.player.openAudio(audioDialog.selectedFile)
    }

    FileDialog {
        id: subtitleDialog
        title: qsTr("Load Subtitles")
        nameFilters: [qsTr("SubRip subtitles (*.srt)"), qsTr("Plain text (*.txt)"),
            qsTr("All files (*)")]
        onAccepted: root.player.openSubtitles(subtitleDialog.selectedFile)
    }

    // NativeSettingsSectionCard in the inspector presentation.
    component SectionCard: ColumnLayout {
        id: card

        property string title: ""

        default property alias content: body.data

        Layout.fillWidth: true
        Layout.leftMargin: 18
        Layout.rightMargin: 18
        Layout.topMargin: 14
        Layout.bottomMargin: 14
        spacing: 4

        Label {
            Layout.fillWidth: true
            Layout.bottomMargin: 2
            visible: card.title.length > 0
            text: card.title
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            font.bold: true
        }

        ColumnLayout {
            id: body

            Layout.fillWidth: true
            spacing: 0
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.popupDivider
            opacity: 0.6
        }
    }

    // NativeSettingsRow in the inspector presentation.
    component SettingsRow: RowLayout {
        Layout.fillWidth: true
        Layout.minimumHeight: 32
        Layout.leftMargin: 0
        spacing: 10
    }

    // NativeSettingsSeparator renders nothing in the inspector presentation
    // (`if presentation != .inspector`), so no in-card separator is drawn; the
    // section card's bottom hairline is the only divider.

    ScrollView {
        id: scroll

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 0

            SectionCard {
                title: qsTr("Audio")

                SettingsRow {
                    Label {
                        text: qsTr("Load Audio")
                        color: Theme.textPrimary
                        font.pixelSize: 13
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Load Audio")
                        Accessible.name: qsTr("Load Audio")
                        onClicked: audioDialog.open()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.topMargin: 10
                    Layout.bottomMargin: 10
                    visible: root.player.error.length > 0
                    text: root.player.error
                    color: Theme.popupDanger
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }

            SectionCard {
                title: qsTr("Subtitle Match")

                SettingsRow {
                    Label {
                        Layout.fillWidth: true
                        text: root.player.subtitleName.length > 0
                              ? root.player.subtitleName : qsTr("No file selected")
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Button {
                        text: qsTr("Open")
                        Accessible.name: qsTr("Open")
                        onClicked: subtitleDialog.open()
                    }
                }
            }

            SectionCard {
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    SettingsRow {
                        Label {
                            text: qsTr("Search Window")
                            color: Theme.textPrimary
                            font.pixelSize: 13
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Label {
                            text: root.player.matchSearchWindow.toString()
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    Slider {
                        id: searchWindowSlider

                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                        Layout.bottomMargin: 12
                        from: 50
                        to: 1000
                        stepSize: 50
                        snapMode: Slider.SnapAlways
                        value: root.player.matchSearchWindow
                        enabled: !root.player.matching
                        Accessible.name: qsTr("Search Window")

                        function commit() {
                            root.player.matchSearchWindow = value;
                            value = Qt.binding(function() { return root.player.matchSearchWindow });
                        }

                        onMoved: if (!pressed) commit()
                        onPressedChanged: if (!pressed && value !== root.player.matchSearchWindow) commit()
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        spacing: 8

                        Button {
                            enabled: root.canMatch
                            onClicked: root.player.matchSubtitles(root.player.matchSearchWindow)

                            contentItem: RowLayout {
                                spacing: 6

                                BusyIndicator {
                                    Layout.preferredWidth: 14
                                    Layout.preferredHeight: 14
                                    running: root.player.matching
                                    visible: root.player.matching
                                }

                                Label {
                                    text: root.player.matching ? qsTr("Matching…") : qsTr("Match")
                                    color: Theme.textPrimary
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            SectionCard {
                visible: root.player.hasMatch
                title: qsTr("Current Match")

                SettingsRow {
                    Label {
                        text: qsTr("Match Rate")
                        color: Theme.textPrimary
                        font.pixelSize: 13
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Label {
                        text: root.player.matchRateLabel
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        font.bold: true
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.bottomMargin: 12
                    visible: root.player.inferredCount > 0
                    text: qsTr("Includes %1 subtitles located from surrounding narration. Their highlights may be approximate.").arg(root.player.inferredCount)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.WordWrap
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 30
                Layout.rightMargin: 30
                Layout.topMargin: 12
                visible: root.player.matchError.length > 0
                text: root.player.matchError
                color: Theme.popupDanger
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 18
            }
        }
    }
}

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

            InspectorFormSection {
                title: qsTr("Audio")

                InspectorFormRow {
                    StyledLabel {
                        text: qsTr("Load Audio")
                        textStyle: Theme.calloutFont
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    FormButton {
                        sizeToLabel: true
                        text: qsTr("Load Audio")
                        Accessible.name: qsTr("Load Audio")
                        onClicked: audioDialog.open()
                    }
                }

                StyledLabel {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingMd
                    Layout.rightMargin: Theme.spacingMd
                    Layout.topMargin: Theme.sliderRowPaddingVertical
                    Layout.bottomMargin: Theme.sliderRowPaddingVertical
                    visible: root.player.error.length > 0
                    text: root.player.error
                    color: Theme.popupDanger
                    textStyle: Theme.calloutFont
                    wrapMode: Text.WordWrap
                }
            }

            InspectorFormSection {
                title: qsTr("Subtitle Match")

                InspectorFormRow {
                    StyledLabel {
                        Layout.fillWidth: true
                        text: root.player.subtitleName.length > 0
                              ? root.player.subtitleName : qsTr("No file selected")
                        textStyle: Theme.calloutFont
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    FormButton {
                        sizeToLabel: true
                        text: qsTr("Open")
                        Accessible.name: qsTr("Open")
                        onClicked: subtitleDialog.open()
                    }
                }
            }

            InspectorFormSection {
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd

                    InspectorFormRow {
                        StyledLabel {
                            text: qsTr("Search Window")
                            textStyle: Theme.calloutFont
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        StyledLabel {
                            text: root.player.matchSearchWindow.toString()
                            textStyle: Theme.calloutFont
                            weight: Theme.weightSemibold
                        }
                    }

                    Slider {
                        id: searchWindowSlider

                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.spacingLg
                        Layout.rightMargin: Theme.spacingLg
                        Layout.bottomMargin: Theme.spacingMd
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

                    FormButtonRow {
                        Layout.fillWidth: true
                        compact: true

                        FormButton {
                            sizeToLabel: true
                            text: root.player.matching ? qsTr("Matching…") : qsTr("Match")
                            busy: root.player.matching
                            enabled: root.canMatch
                            onClicked: root.player.matchSubtitles(root.player.matchSearchWindow)
                        }
                    }
                }
            }

            InspectorFormSection {
                visible: root.player.hasMatch
                title: qsTr("Current Match")

                InspectorFormRow {
                    StyledLabel {
                        text: qsTr("Match Rate")
                        textStyle: Theme.calloutFont
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StyledLabel {
                        text: root.player.matchRateLabel
                        secondary: true
                        textStyle: Theme.calloutFont
                        weight: Theme.weightSemibold
                    }
                }

                StyledLabel {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingLg
                    Layout.rightMargin: Theme.spacingLg
                    Layout.bottomMargin: Theme.spacingMd
                    visible: root.player.inferredCount > 0
                    text: qsTr("Includes %1 subtitles located from surrounding narration. Their highlights may be approximate.").arg(root.player.inferredCount)
                    secondary: true
                    textStyle: Theme.captionFont
                    wrapMode: Text.WordWrap
                }
            }

            StyledLabel {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.readerInspectorFormInset + Theme.spacingMd
                Layout.rightMargin: Theme.readerInspectorFormInset + Theme.spacingMd
                Layout.topMargin: Theme.spacingMd
                visible: root.player.matchError.length > 0
                text: root.player.matchError
                color: Theme.popupDanger
                textStyle: Theme.calloutFont
                wrapMode: Text.WordWrap
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.readerInspectorFormInset
            }
        }
    }
}

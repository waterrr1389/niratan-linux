// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Sasayaki inspector for the reader page, following
// Features/Sasayaki/SasayakiSheet.swift (the playback header, the audio and
// subtitle pickers, the cue list and the rate and delay controls) and
// Features/Sasayaki/SasayakiMatchView.swift (the subtitle match section).
//
// ReaderPage owns the Sasayaki model and points this panel at it, then opens
// the panel from the toolbar. Cue ranges and the active highlight are applied
// by the page through window.hoshiReader after each chapter loads. The panel is
// a right-side drawer, so it opens over the page and leaves the text behind it
// visible; `opened` tells the page when it covers the text.
//
// The rate and delay sliders write through the model, which clamps and
// persists them. The scrubber is bound to the model's position and only seeks
// on release, so dragging does not fight the playback ticks.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Niratan

Item {
    id: root

    // The page's model; the panel never creates its own.
    required property SasayakiViewModel player
    readonly property bool opened: panel.visible
    signal closed

    function open() {
        panel.open();
    }

    function close() {
        panel.close();
    }

    function formatTime(seconds) {
        var total = Math.max(0, Math.floor(seconds));
        var hours = Math.floor(total / 3600);
        var minutes = Math.floor((total % 3600) / 60);
        var rest = total % 60;
        return (hours < 10 ? "0" : "") + hours + ":" + (minutes < 10 ? "0" : "") + minutes + ":" + (rest < 10 ? "0" : "") + rest;
    }

    function formatCueTime(seconds) {
        var total = Math.max(0, Math.floor(seconds));
        var hours = Math.floor(total / 3600);
        var minutes = Math.floor((total % 3600) / 60);
        var rest = total % 60;
        var paddedRest = (rest < 10 ? "0" : "") + rest;
        if (hours > 0)
            return hours + ":" + (minutes < 10 ? "0" : "") + minutes + ":" + paddedRest;
        return minutes + ":" + paddedRest;
    }

    Connections {
        target: root.player

        function onCuesChanged() {
            cueList.syncCurrent();
        }

        function onCurrentCueChanged() {
            cueList.syncCurrent();
        }

        function onMatchApplied() {
            // ReaderPage owns this panel. Its apply path also highlights the
            // current cue after installing the ranges in the web view.
            var readerPage = root.parent;
            if (readerPage && typeof readerPage.applySasayakiCues === "function")
                readerPage.applySasayakiCues();
        }
    }

    FileDialog {
        id: audioDialog
        title: qsTr("Load Audio")
        nameFilters: [qsTr("Audiobooks (*.mp3 *.m4b)"), qsTr("All files (*)")]
        onAccepted: root.player.openAudio(audioDialog.selectedFile)
    }

    FileDialog {
        id: subtitleDialog
        title: qsTr("Load Subtitles")
        nameFilters: [qsTr("SubRip subtitles (*.srt)"), qsTr("All files (*)")]
        onAccepted: root.player.openSubtitles(subtitleDialog.selectedFile)
    }

    Drawer {
        id: panel
        edge: Qt.RightEdge
        width: Math.min(420, (Overlay.overlay ? Overlay.overlay.width : 640) * 0.75)
        height: Overlay.overlay ? Overlay.overlay.height : 640
        modal: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onAboutToShow: root.player.matchSearchWindow = undefined
        onAboutToHide: root.player.cancelSubtitleMatch()
        onOpened: {
            cueList.syncCurrent();
        }
        onClosed: root.closed()

        background: Rectangle {
            color: Theme.cardBackground
            border.width: Theme.cardBorderWidth
            border.color: Theme.cardStroke
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 16
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Sasayaki")
                        color: Theme.textPrimary
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: ReaderViewModel.hasBook ? ReaderViewModel.title : qsTr("No book open")
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                ToolButton {
                    text: "\u2715"
                    onClicked: panel.close()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 8

                Button {
                    Layout.fillWidth: true
                    text: qsTr("Load Audio")
                    onClicked: audioDialog.open()
                }

                Button {
                    Layout.fillWidth: true
                    text: qsTr("Load Subtitles")
                    onClicked: subtitleDialog.open()
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                visible: player.audioName.length > 0
                text: player.audioName
                color: Theme.textSecondary
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                visible: player.error.length > 0
                text: player.error
                color: Theme.popupDanger
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 12
                spacing: 2
                visible: player.hasAudio

                Slider {
                    id: scrubber
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(player.duration, 1)
                    live: false
                    value: player.position
                    enabled: player.duration > 0
                    onMoved: player.seek(scrubber.value)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: root.formatTime(player.position)
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Label {
                        text: "-" + root.formatTime(Math.max(player.duration - player.position, 0))
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                spacing: 6
                visible: player.hasAudio

                ToolButton {
                    text: "\u221215"
                    font.pixelSize: 11
                    onClicked: player.skip(-15)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Back 15 seconds")
                }

                ToolButton {
                    text: "\u00AB"
                    font.pixelSize: 18
                    onClicked: player.previousCue()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Previous cue")
                }

                Button {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    text: player.playing ? "\u275A\u275A" : "\u25B6"
                    font.pixelSize: 18
                    onClicked: player.togglePlayback()
                    ToolTip.visible: hovered
                    ToolTip.text: player.playing ? qsTr("Pause") : qsTr("Play")
                }

                ToolButton {
                    text: "\u00BB"
                    font.pixelSize: 18
                    onClicked: player.nextCue()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Next cue")
                }

                ToolButton {
                    text: "+15"
                    font.pixelSize: 11
                    onClicked: player.skip(15)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Forward 15 seconds")
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                columns: 3
                columnSpacing: 8
                rowSpacing: 2

                Label {
                    text: qsTr("Delay")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }

                Slider {
                    id: delaySlider
                    Layout.fillWidth: true
                    from: -2
                    to: 2
                    stepSize: 0.05
                    value: player.delay
                    onMoved: player.setDelay(delaySlider.value)
                }

                Label {
                    Layout.preferredWidth: 48
                    horizontalAlignment: Text.AlignRight
                    text: (player.delay >= 0 ? "+" : "") + player.delay.toFixed(2) + "s"
                    color: Theme.textPrimary
                    font.pixelSize: 11
                }

                Label {
                    text: qsTr("Speed")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }

                Slider {
                    id: rateSlider
                    Layout.fillWidth: true
                    from: 0.5
                    to: 2.5
                    stepSize: 0.05
                    value: player.rate
                    onMoved: player.setRate(rateSlider.value)
                }

                Label {
                    Layout.preferredWidth: 48
                    horizontalAlignment: Text.AlignRight
                    text: player.rate.toFixed(2) + "x"
                    color: Theme.textPrimary
                    font.pixelSize: 11
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 12
                Layout.preferredHeight: currentCueText.implicitHeight + 20
                visible: player.cues.length > 0
                color: Theme.windowBackground
                radius: 10

                Label {
                    id: currentCueText
                    anchors.fill: parent
                    anchors.margins: 10
                    text: player.currentCueText.length > 0 ? player.currentCueText : qsTr("No cue at this position")
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    maximumLineCount: 5
                    elide: Text.ElideRight
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 12
                spacing: 4
                visible: root.player.hasAudio

                Label {
                    text: qsTr("Subtitle Match")
                    color: Theme.textPrimary
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: root.player.subtitleName || qsTr("No subtitle file selected")
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }

                    Button {
                        text: root.player.matching ? qsTr("Matching…") : qsTr("Match")
                        enabled: root.player.subtitleName.length > 0 && !root.player.matching
                        onClicked: root.player.matchSubtitles(root.player.matchSearchWindow)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Search Window")
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }

                    Slider {
                        id: searchWindowSlider
                        Layout.fillWidth: true
                        from: 50
                        to: 1000
                        stepSize: 50
                        snapMode: Slider.SnapAlways
                        value: root.player.matchSearchWindow
                        enabled: !root.player.matching
                        Accessible.name: qsTr("Search Window")
                        onMoved: root.player.matchSearchWindow = searchWindowSlider.value
                    }

                    Label {
                        Layout.preferredWidth: 48
                        horizontalAlignment: Text.AlignRight
                        text: root.player.matchSearchWindow.toString()
                        color: Theme.textPrimary
                        font.pixelSize: 11
                    }
                }

                ProgressBar {
                    Layout.fillWidth: true
                    visible: root.player.matching
                    indeterminate: true
                    Accessible.name: qsTr("Matching subtitles")
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.player.hasMatch
                    text: qsTr("Match rate: %1").arg(root.player.matchRateLabel)
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.player.hasMatch && root.player.inferredCount > 0
                    text: qsTr("%n inferred from surrounding narration; highlights may be approximate.", "", root.player.inferredCount)
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.player.matchError.length > 0
                    text: root.player.matchError
                    color: Theme.popupDanger
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: !root.player.hasMatch
                    text: qsTr("Match the subtitles to this book to highlight narration in the reader.")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            ListView {
                id: cueList
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: 12
                Layout.bottomMargin: 8
                clip: true
                model: player.cues
                spacing: 0

                function syncCurrent() {
                    if (root.player.currentCueId.length === 0)
                        return;
                    for (var index = 0; index < root.player.cues.length; ++index) {
                        if (root.player.cues[index].id === root.player.currentCueId) {
                            cueList.currentIndex = index;
                            cueList.positionViewAtIndex(index, ListView.Contain);
                            return;
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: cueItem
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    highlighted: cueItem.modelData.id === root.player.currentCueId
                    onClicked: root.player.jumpCue(cueItem.modelData.id)

                    contentItem: Label {
                        text: root.formatCueTime(cueItem.modelData.start) + "  " + cueItem.modelData.text
                        color: cueItem.highlighted ? Theme.accent : Theme.textPrimary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}

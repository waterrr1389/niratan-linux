// SPDX-License-Identifier: GPL-3.0-or-later
//
// Docked tabs and width bounds follow VideoMiningHistorySidebar.swift.
// Flat rows keep the cue text dominant; the timestamp column makes the list
// read as a timeline rather than a stack of cards.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Niratan

Rectangle {
    id: sidebar

    required property PlayerStudyViewModel study
    property bool playbackReady: false
    property var subtitleTracks: []
    property bool extractingTranscript: false
    property int preferredWidth: 340
    property int selectedTab: 0
    readonly property int minimumWidth: 320
    readonly property int maximumWidth: 560

    signal closeRequested()
    signal subtitleFileRequested(url file)
    signal subtitleTrackRequested(int trackId, bool extract)
    signal transcriptRequested()

    implicitWidth: preferredWidth
    color: Theme.windowBackground

    function timeLabel(seconds: real): string {
        const total = Math.floor(Math.max(0, seconds))
        const remainder = total % 60
        const minutes = Math.floor(total / 60) % 60
        const hours = Math.floor(total / 3600)
        const secondLabel = (remainder < 10 ? "0" : "") + remainder
        if (hours > 0)
            return hours + ":" + (minutes < 10 ? "0" : "") + minutes + ":" + secondLabel
        return minutes + ":" + secondLabel
    }

    onVisibleChanged: {
        if (sidebar.visible && sidebar.study) {
            sidebar.study.refreshHistory()
            sidebar.revealCurrent()
        }
    }

    function revealCurrent() {
        if (sidebar.selectedTab === 1 && sidebar.study.currentVisibleRow >= 0)
            transcript.positionViewAtIndex(sidebar.study.currentVisibleRow, ListView.Center)
        else if (sidebar.selectedTab === 2 && sidebar.study.currentChapter >= 0)
            chapterList.positionViewAtIndex(sidebar.study.currentChapter, ListView.Center)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        anchors.topMargin: Theme.spacingMd
        anchors.bottomMargin: Theme.spacingSm
        spacing: Theme.spacingSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs

            Repeater {
                model: [qsTr("History"), qsTr("Transcript"), qsTr("Chapters")]
                delegate: SidebarButton {
                    id: tab
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true
                    text: tab.modelData
                    selected: sidebar.selectedTab === tab.index
                    onClicked: sidebar.selectedTab = tab.index
                }
            }

            SidebarButton {
                text: "\u00D7"
                Accessible.name: qsTr("Close study sidebar")
                onClicked: sidebar.closeRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.cardBorderWidth
            color: Theme.cardStroke
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sidebar.selectedTab

            ColumnLayout {
                spacing: Theme.spacingXs

                RowLayout {
                    SidebarButton {
                        text: qsTr("Mine Current Subtitle")
                        enabled: sidebar.study.canMine
                        onClicked: sidebar.study.mineCurrentSubtitle()
                    }
                    Item { Layout.fillWidth: true }
                    SidebarButton {
                        text: qsTr("Clear")
                        enabled: sidebar.study.history.length > 0
                        onClicked: clearDialog.open()
                    }
                }

                EmptyLabel {
                    visible: history.count === 0
                    text: sidebar.study.historyLimit === 0
                        ? qsTr("Mining history is disabled.")
                        : qsTr("Saved subtitles will appear here.")
                }

                ListView {
                    id: history
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: count > 0
                    clip: true
                    model: sidebar.study.history
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Item {
                        id: record
                        required property var modelData
                        width: history.width - Theme.spacingMd
                        height: summary.implicitHeight + Theme.spacingMd

                        ColumnLayout {
                            id: summary
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.spacingSm
                            spacing: Theme.spacingXs

                            Label {
                                Layout.fillWidth: true
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                text: record.modelData.videoTitle
                            }
                            Label {
                                Layout.fillWidth: true
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                text: record.modelData.text
                            }
                            Label {
                                Layout.fillWidth: true
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                                text: qsTr("%1, %2").arg(sidebar.timeLabel(record.modelData.start))
                                                    .arg(record.modelData.relativeDate)
                            }
                            RowLayout {
                                SidebarButton {
                                    text: qsTr("Jump")
                                    onClicked: sidebar.study.jumpToHistory(record.modelData.id)
                                }
                                SidebarButton {
                                    text: qsTr("Copy")
                                    onClicked: sidebar.study.copyHistory(record.modelData.id)
                                }
                                SidebarButton {
                                    text: qsTr("Delete")
                                    onClicked: sidebar.study.removeHistory(record.modelData.id)
                                }
                            }
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: Theme.cardBorderWidth
                            color: Theme.cardStroke
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: Theme.spacingSm

                RowLayout {
                    Layout.fillWidth: true
                    visible: sidebar.subtitleTracks.length > 0

                    ComboBox {
                        id: trackSelector
                        Layout.fillWidth: true
                        enabled: sidebar.playbackReady
                        Accessible.name: qsTr("Subtitle track")
                        textRole: "label"
                        model: [{ "id": -1, "label": qsTr("Subtitles off"), "external": true }]
                            .concat(sidebar.subtitleTracks.map(track => ({
                                "id": track.id, "external": track.isExternal,
                                "label": track.title || track.language
                                    || qsTr("Subtitle Track %1").arg(track.id)
                            })))
                        currentIndex: sidebar.subtitleTracks.findIndex(track => track.isSelected) + 1
                        readonly property bool embedded: currentIndex > 0
                            && currentIndex < model.length && !model[currentIndex].external
                        onActivated: function(index) {
                            const track = trackSelector.model[index]
                            sidebar.subtitleTrackRequested(track.id, !track.external)
                        }
                    }

                    SidebarButton {
                        visible: trackSelector.embedded
                        enabled: sidebar.playbackReady && !sidebar.extractingTranscript
                        text: qsTr("Extract transcript")
                        onClicked: sidebar.transcriptRequested()
                    }
                }

                RowLayout {
                    TextField {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 80
                        placeholderText: qsTr("Search transcript")
                        text: sidebar.study.search
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textSecondary
                        selectionColor: Theme.accent
                        selectedTextColor: Theme.popupBackground
                        onTextEdited: {
                            sidebar.study.search = text
                            transcript.positionViewAtBeginning()
                        }
                        background: Rectangle {
                            color: Theme.cardBackground
                            border.color: parent.activeFocus ? Theme.accent : Theme.cardStroke
                            radius: Theme.controlRadius
                        }
                    }
                    SidebarButton {
                        text: qsTr("Load subtitles")
                        enabled: sidebar.playbackReady
                        onClicked: subtitleDialog.open()
                    }
                }

                SidebarButton {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Show earlier cues")
                    visible: sidebar.study.canShowEarlier
                    onClicked: {
                        sidebar.study.showEarlier()
                        transcript.positionViewAtBeginning()
                    }
                }

                EmptyLabel {
                    visible: transcript.count === 0
                    text: sidebar.extractingTranscript ? qsTr("Extracting transcript…")
                        : sidebar.study.transcriptError.length > 0 ? sidebar.study.transcriptError
                        : sidebar.study.search.trim().length > 0 ? qsTr("No matching cues.")
                        : trackSelector.embedded ? qsTr("Extract the selected track to see its transcript.")
                        : qsTr("Select a subtitle track or load an external subtitle file.")
                }

                ListView {
                    id: transcript
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: count > 0
                    clip: true
                    model: sidebar.study.transcript
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: TimelineRow {
                        id: cue
                        required property var modelData
                        width: transcript.width - Theme.spacingMd
                        marker: sidebar.timeLabel(cue.modelData.start)
                        title: cue.modelData.text
                        selected: cue.modelData.index === sidebar.study.currentRow
                        onClicked: sidebar.study.seekToRow(cue.modelData.index)
                    }
                }

                SidebarButton {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Show later cues")
                    visible: sidebar.study.canShowLater
                    onClicked: sidebar.study.showLater()
                }
            }

            Item {
                ListView {
                    id: chapterList
                    anchors.fill: parent
                    clip: true
                    visible: count > 0
                    model: sidebar.study.chapters
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: TimelineRow {
                        id: chapter
                        required property int index
                        required property var modelData
                        width: chapterList.width - Theme.spacingMd
                        marker: String(chapter.index + 1)
                        title: chapter.modelData.title || qsTr("Chapter %1").arg(chapter.index + 1)
                        detail: sidebar.timeLabel(chapter.modelData.start)
                        selected: chapter.index === sidebar.study.currentChapter
                        onClicked: sidebar.study.playChapter(chapter.index)
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: parent.width
                    visible: chapterList.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Theme.textSecondary
                    text: qsTr("This video has no chapter markers.")
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            wrapMode: Text.Wrap
            text: sidebar.study.lastMiningNotice
        }
    }

    Connections {
        target: sidebar.study
        function onCurrentRowChanged() {
            if (sidebar.visible && sidebar.selectedTab === 1 && sidebar.study.search.trim() === ""
                    && sidebar.study.currentVisibleRow >= 0)
                transcript.positionViewAtIndex(sidebar.study.currentVisibleRow, ListView.Contain)
        }
        function onCurrentChapterChanged() {
            if (sidebar.visible && sidebar.selectedTab === 2 && sidebar.study.currentChapter >= 0)
                chapterList.positionViewAtIndex(sidebar.study.currentChapter, ListView.Contain)
        }
    }

    onSelectedTabChanged: {
        if (sidebar.visible)
            sidebar.revealCurrent()
    }

    Rectangle {
        anchors.left: parent.left
        height: parent.height
        width: resize.pressed || resize.containsMouse ? 2 : Theme.cardBorderWidth
        color: resize.pressed || resize.containsMouse ? Theme.accent : Theme.cardStroke
    }

    MouseArea {
        id: resize
        anchors.left: parent.left
        height: parent.height
        width: 10
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        property real pointerStart: 0
        property int widthStart: 0
        onPressed: function(mouse) {
            resize.widthStart = sidebar.width
            resize.pointerStart = resize.mapToItem(null, mouse.x, mouse.y).x
        }
        onPositionChanged: function(mouse) {
            if (resize.pressed) {
                const pointer = resize.mapToItem(null, mouse.x, mouse.y).x
                sidebar.preferredWidth = Math.round(Math.max(sidebar.minimumWidth,
                    Math.min(sidebar.maximumWidth, resize.widthStart + resize.pointerStart - pointer)))
            }
        }
    }

    FileDialog {
        id: subtitleDialog
        title: qsTr("Load Subtitle File")
        nameFilters: [qsTr("Subtitles (*.srt *.vtt *.ass *.ssa)"), qsTr("All files (*)")]
        onAccepted: sidebar.subtitleFileRequested(selectedFile)
    }

    MessageDialog {
        id: clearDialog
        title: qsTr("Clear Mining History?")
        text: qsTr("This removes every saved subtitle from mining history.")
        buttons: MessageDialog.Ok | MessageDialog.Cancel
        onAccepted: sidebar.study.clearHistory()
    }

    component EmptyLabel: Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        color: Theme.textSecondary
    }

    component SidebarButton: Button {
        id: button
        property bool selected: false
        focusPolicy: Qt.TabFocus
        leftPadding: Theme.spacingSm
        rightPadding: Theme.spacingSm
        contentItem: Label {
            text: button.text
            color: button.enabled ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            font.bold: button.selected
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: Theme.controlRadius
            color: button.selected || button.hovered || button.down
                ? Theme.cardBackground : Theme.windowBackground
            border.width: Theme.cardBorderWidth
            border.color: button.selected || button.activeFocus ? Theme.accent : Theme.cardStroke
        }
    }

    component TimelineRow: Button {
        id: row
        required property string marker
        required property string title
        property string detail: ""
        property bool selected: false
        text: row.title
        focusPolicy: Qt.TabFocus
        padding: Theme.spacingSm
        contentItem: RowLayout {
            spacing: Theme.spacingSm
            Label {
                Layout.minimumWidth: 48
                Layout.alignment: Qt.AlignTop
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                text: row.marker
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXs
                Label {
                    Layout.fillWidth: true
                    font.pixelSize: Theme.fontBody
                    font.bold: row.selected
                    color: Theme.textPrimary
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    text: row.title
                }
                Label {
                    visible: row.detail.length > 0
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    text: row.detail
                }
            }
        }
        background: Rectangle {
            color: row.selected || row.hovered || row.down ? Theme.cardBackground : Theme.windowBackground
            border.width: row.activeFocus ? Theme.cardBorderWidth : 0
            border.color: Theme.accent
            Rectangle {
                height: parent.height
                width: 2
                visible: row.selected
                color: Theme.accent
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: Theme.cardBorderWidth
                color: Theme.cardStroke
            }
        }
    }
}

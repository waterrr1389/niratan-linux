// SPDX-License-Identifier: GPL-3.0-or-later
//
// Docked tabs follow VideoMiningHistorySidebar.swift; transcript states,
// active intervals and window growth follow SubtitleTranscriptView.swift.
// A search lists every match with its "N Matches" caption in the scrolling
// results and highlighted ranges in each row (VideoStudySearch); the current
// chapter row shows its raw playback progress (chapterRow).

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
    property real loopStart: -1
    property real loopEnd: -1
    required property int preferredWidth
    property int selectedTab: 0
    property bool followingTranscript: true
    readonly property int minimumWidth: Theme.studySidebarMinimumWidth
    readonly property int maximumWidth: Theme.studySidebarMaximumWidth

    signal closeRequested()
    signal widthSaveRequested(int width)
    signal subtitleFileRequested(url file)
    signal subtitleTrackRequested(int trackId, bool extract)
    signal transcriptRequested()
    signal loopStartRequested(real time)
    signal loopEndRequested(real time)

    implicitWidth: resize.previewWidth || preferredWidth
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
        if (sidebar.selectedTab === 1)
            sidebar.revealTranscriptRow()
        else if (sidebar.selectedTab === 2 && sidebar.study.currentChapter >= 0)
            chapterList.positionViewAtIndex(sidebar.study.currentChapter, ListView.Center)
    }

    function revealTranscriptRow() {
        if (sidebar.visible && sidebar.selectedTab === 1 && sidebar.followingTranscript
                && sidebar.study.search.trim() === "" && sidebar.study.currentVisibleRow >= 0)
            transcript.positionRow(sidebar.study.currentVisibleRow, ListView.Center)
    }

    onFollowingTranscriptChanged: sidebar.study.setTranscriptFollowing(sidebar.followingTranscript)

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

            SegmentedPicker {
                objectName: "studyTabs"
                Layout.fillWidth: true
                fillsWidth: true
                model: [qsTr("History"), qsTr("Transcript"), qsTr("Chapters")]
                currentIndex: sidebar.selectedTab
                onActivated: index => sidebar.selectedTab = index
            }

            IconButton {
                icon: "xmark"
                diameter: Theme.closeButtonDiameter
                iconSize: Theme.closeButtonGlyph
                accessibleName: qsTr("Close study sidebar")
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
                        enabled: sidebar.study.historyLimit > 0 && history.count > 0
                        onClicked: clearDialog.open()
                    }
                }

                EmptyLabel {
                    visible: sidebar.study.historyLimit === 0 || history.count === 0
                    text: sidebar.study.historyLimit === 0
                        ? qsTr("Mining history is disabled.")
                        : qsTr("Saved subtitles will appear here.")
                }

                ListView {
                    id: history
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: sidebar.study.historyLimit > 0 && count > 0
                    clip: true
                    model: sidebar.study.history
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Item {
                        id: record
                        required property var modelData
                        width: history.width - Theme.spacingMd
                        height: summary.implicitHeight + Theme.spacingMd

                        HoverHandler { id: recordHover }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: sidebar.study.jumpToHistory(record.modelData.id)
                        }

                        ColumnLayout {
                            id: summary
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.spacingSm
                            spacing: Theme.spacingXs

                            StyledLabel {
                                Layout.fillWidth: true
                                secondary: true
                                textStyle: Theme.captionFont
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                text: record.modelData.videoTitle
                            }
                            StyledLabel {
                                Layout.fillWidth: true
                                textStyle: Theme.bodyFont
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                                text: record.modelData.text
                            }
                            StyledLabel {
                                Layout.fillWidth: true
                                secondary: true
                                textStyle: Theme.captionFont
                                text: qsTr("%1, %2").arg(sidebar.timeLabel(record.modelData.start))
                                                    .arg(record.modelData.relativeDate)
                            }
                            RowLayout {
                                SidebarButton {
                                    text: qsTr("Jump")
                                    onClicked: sidebar.study.jumpToHistory(record.modelData.id)
                                }
                                RowLayout {
                                    visible: recordHover.hovered
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
                        objectName: "transcriptSearchField"
                        Layout.fillWidth: true
                        Layout.minimumWidth: Theme.studySearchMinimumWidth
                        placeholderText: qsTr("Search transcript")
                        text: sidebar.study.search
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textSecondary
                        selectionColor: Theme.accent
                        selectedTextColor: Theme.popupBackground
                        onTextEdited: {
                            sidebar.study.search = text
                            // Only results start at the top; clearing leaves the
                            // recenter to onSearchChanged/revealTranscriptRow.
                            if (sidebar.study.searchActive)
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

                TranscriptAlignToolbar {
                    Layout.fillWidth: true
                    study: sidebar.study
                }

                Item {
                    id: transcriptEmptyState
                    objectName: "transcriptEmptyState"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: transcript.count === 0
                    ColumnLayout {
                        anchors.centerIn: parent
                        width: transcriptEmptyState.width
                        spacing: Theme.spacingXs
                        BusyIndicator {
                            Layout.alignment: Qt.AlignHCenter
                            running: transcriptEmptyState.visible && sidebar.extractingTranscript
                            visible: running
                        }
                        EmptyLabel {
                            visible: text.length > 0
                            weight: Theme.weightSemibold
                            secondary: false
                            text: sidebar.extractingTranscript ? qsTr("Loading Transcript")
                                : sidebar.study.transcriptError.length > 0 ? qsTr("Transcript Unavailable") : ""
                        }
                        EmptyLabel {
                            textFormat: Text.PlainText
                            text: sidebar.extractingTranscript ? qsTr("Reading the selected subtitle track…")
                                : sidebar.study.transcriptError.length > 0 ? sidebar.study.transcriptError
                                : sidebar.study.searchActive ? qsTr("No matching cues.")
                                : trackSelector.embedded ? qsTr("Extract the selected track to see its transcript.")
                                : qsTr("Select a subtitle track or load an external subtitle file.")
                        }
                    }
                }

                SidebarButton {
                    objectName: "resumeFollowingButton"
                    visible: !sidebar.followingTranscript && transcript.count > 0
                        && !sidebar.study.searchActive
                    text: qsTr("Resume Following")
                    onClicked: {
                        sidebar.followingTranscript = true
                        sidebar.revealTranscriptRow()
                    }
                }

                ListView {
                    id: transcript
                    objectName: "transcriptList"
                    property bool programmaticScroll: false
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: count > 0
                    clip: true
                    model: sidebar.study.transcript
                    boundsBehavior: Flickable.StopAtBounds
                    header: Item {
                        width: transcript.width
                        height: searchCount.visible
                            ? searchCount.implicitHeight + Theme.spacingXs : 0

                        StyledLabel {
                            id: searchCount
                            objectName: "searchMatchCountLabel"
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: Theme.spacingSm
                            anchors.rightMargin: Theme.spacingSm
                            visible: sidebar.study.searchActive
                                && sidebar.study.searchMatchCount > 0
                            textStyle: Theme.captionFont
                            weight: Theme.weightSemibold
                            secondary: true
                            text: qsTr("%1 Matches").arg(sidebar.study.searchMatchCount)
                        }
                    }
                    ScrollBar.vertical: ScrollBar {
                        onPressedChanged: {
                            if (pressed)
                                sidebar.followingTranscript = false
                            else
                                transcript.extendWindowNearEdge()
                        }
                    }
                    onMovementStarted: if (!programmaticScroll) sidebar.followingTranscript = false
                    onMovementEnded: if (!programmaticScroll) transcript.extendWindowNearEdge()

                    function positionRow(index: int, mode: int) {
                        transcript.programmaticScroll = true
                        transcript.positionViewAtIndex(index, mode)
                        transcript.programmaticScroll = false
                    }

                    function extendWindowNearEdge() {
                        if (!transcript.visible || transcript.count === 0)
                            return
                        const top = transcript.indexAt(1, transcript.contentY + 1)
                        const anchor = transcript.itemAtIndex(top)
                        if (!anchor)
                            return
                        const earlier = sidebar.study.canShowEarlier && top < 2
                        const later = sidebar.study.canShowLater && transcript.contentHeight + transcript.originY
                            - transcript.contentY - transcript.height <= 2 * anchor.height
                        if (!earlier && !later)
                            return
                        // Replacing the window resets the list model; keep the top cue in place.
                        const cueIndex = sidebar.study.transcript[top].index
                        const offset = transcript.contentY - anchor.y
                        if (earlier)
                            sidebar.study.showEarlier()
                        else
                            sidebar.study.showLater()
                        transcript.forceLayout()
                        const index = sidebar.study.transcript.findIndex(cue => cue.index === cueIndex)
                        transcript.positionRow(index, ListView.Beginning)
                        const restored = transcript.itemAtIndex(index)
                        if (restored && restored.y === transcript.contentY)
                            transcript.contentY += offset
                    }

                    delegate: RowLayout {
                        id: cue
                        required property var modelData
                        readonly property bool marksStart: sidebar.loopStart >= modelData.start
                            && sidebar.loopStart <= modelData.end
                        readonly property bool marksEnd: sidebar.loopEnd >= modelData.start
                            && sidebar.loopEnd <= modelData.end
                        readonly property bool showMarkers: cueHover.hovered || marksStart || marksEnd
                            || cueRow.activeFocus || markA.activeFocus || markB.activeFocus
                        width: transcript.width - Theme.spacingMd
                        spacing: Theme.spacingXs

                        HoverHandler { id: cueHover }

                        StudyTimelineRow {
                            id: cueRow
                            objectName: "transcriptRow"
                            Layout.fillWidth: true
                            marker: sidebar.timeLabel(cue.modelData.start)
                            title: cue.modelData.text
                            highlights: cue.modelData.highlights || []
                            selected: cue.modelData.start <= sidebar.study.adjustedTime
                                && sidebar.study.adjustedTime <= cue.modelData.end
                            onClicked: sidebar.study.seekToRow(cue.modelData.index)
                        }

                        StudyLoopMarker {
                            id: markA
                            opacity: cue.showMarkers ? 1 : 0
                            enabled: sidebar.playbackReady
                            selected: cue.marksStart
                            text: "A"
                            Accessible.name: qsTr("Set A Point")
                            ToolTip.text: qsTr("Set A Point")
                            onClicked: sidebar.loopStartRequested(cue.modelData.start)
                        }

                        StudyLoopMarker {
                            id: markB
                            opacity: cue.showMarkers ? 1 : 0
                            enabled: sidebar.playbackReady && sidebar.loopStart >= 0
                            selected: cue.marksEnd
                            text: "B"
                            Accessible.name: qsTr("Set B Point")
                            ToolTip.text: qsTr("Set B Point")
                            onClicked: sidebar.loopEndRequested(cue.modelData.end)
                        }
                    }
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

                    delegate: Item {
                        id: chapterItem
                        objectName: "chapterRow"
                        required property int index
                        required property var modelData
                        width: chapterList.width - Theme.spacingMd
                        height: chapter.height + (chapterProgress.visible
                            ? chapterProgress.implicitHeight + Theme.spacingXs : 0)

                        StudyTimelineRow {
                            id: chapter
                            width: parent.width
                            marker: String(chapterItem.index + 1)
                            title: chapterItem.modelData.title
                                || qsTr("Chapter %1").arg(chapterItem.index + 1)
                            detail: sidebar.timeLabel(chapterItem.modelData.start)
                                + (chapterItem.modelData.duration > 0
                                    ? " · " + sidebar.timeLabel(chapterItem.modelData.duration) : "")
                            selected: chapterItem.index === sidebar.study.currentChapter
                            onClicked: sidebar.study.playChapter(chapterItem.index)
                        }

                        ProgressTrack {
                            id: chapterProgress
                            objectName: "chapterProgressBar"
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: Theme.spacingSm
                            anchors.rightMargin: Theme.spacingSm
                            y: chapter.height + Theme.spacingXs
                            value: sidebar.study.currentChapterProgress
                            visible: chapterItem.index === sidebar.study.currentChapter
                                && sidebar.study.currentChapterProgress >= 0
                        }
                    }
                }
                StyledLabel {
                    anchors.centerIn: parent
                    width: parent.width
                    visible: chapterList.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    secondary: true
                    text: qsTr("This video has no chapter markers.")
                }
            }
        }

        StyledLabel {
            Layout.fillWidth: true
            visible: text.length > 0
            secondary: true
            textStyle: Theme.captionFont
            wrapMode: Text.Wrap
            text: sidebar.study.lastMiningNotice
        }
    }

    Connections {
        target: sidebar.study
        function onCurrentVisibleRowChanged() { sidebar.revealTranscriptRow() }
        function onTranscriptDocumentChanged() {
            sidebar.followingTranscript = true
            sidebar.revealTranscriptRow()
        }
        function onSearchChanged() {
            if (!sidebar.study.searchActive) {
                sidebar.followingTranscript = true
                sidebar.revealTranscriptRow()
            }
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
        width: resize.pressed || resize.containsMouse ? Theme.studyResizeActiveWidth
                                                      : Theme.cardBorderWidth
        color: resize.pressed || resize.containsMouse ? Theme.accent : Theme.cardStroke
    }

    MouseArea {
        id: resize
        anchors.left: parent.left
        height: parent.height
        width: Theme.studyResizeHitWidth
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        property real pointerStart: 0
        property int widthStart: 0
        property int previewWidth: 0
        onPressed: function(mouse) {
            resize.widthStart = sidebar.width
            resize.pointerStart = resize.mapToItem(null, mouse.x, mouse.y).x
        }
        onPositionChanged: function(mouse) {
            if (resize.pressed) {
                const pointer = resize.mapToItem(null, mouse.x, mouse.y).x
                resize.previewWidth = Math.round(Math.max(sidebar.minimumWidth,
                    Math.min(sidebar.maximumWidth, resize.widthStart + resize.pointerStart - pointer)))
            }
        }
        onReleased: {
            if (resize.previewWidth > 0 && resize.previewWidth !== sidebar.preferredWidth)
                sidebar.widthSaveRequested(resize.previewWidth)
            resize.previewWidth = 0
        }
        onCanceled: resize.previewWidth = 0
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

    component EmptyLabel: StyledLabel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        textStyle: Theme.calloutFont
        secondary: true
    }

    // The sidebar's actions are the kit's small capsule buttons.
    component SidebarButton: FormButton {
        small: true
        sizeToLabel: true
        focusPolicy: Qt.TabFocus
    }
}

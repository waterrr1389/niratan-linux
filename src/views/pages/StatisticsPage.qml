// SPDX-License-Identifier: GPL-3.0-or-later
//
// The statistics dashboard, following Features/Bookshelf/StatisticsDashboardView.swift
// (`body`, `header`, `dashboardLayout`, `fullWidthTrendSection`, `trendSection`,
// `todaySection`, `targetSettingsSection`, `weekSection`, `calendarSection`,
// `selectedRangeSection`, `speedSummarySection`, `bookRankingSection`,
// `shelfComparisonSection`, `emptyDashboardState`,
// `loadingDashboardPlaceholder`, `dashboardLayoutWithLoadingOverlay` and
// `isInitialSnapshotLoading`) over the aggregation in
// StatisticsDashboardModels.swift. Values arrive already aggregated from
// StatisticsViewModel; this page only formats and draws them. The cards live
// in src/views/statistics/.
//
// The page owns the shell's toolbar row: PageHeader is a fixed band above the
// ScrollView and stays at the page top while the cards scroll. Decision D1
// keeps the back control in that band instead of a macOS window toolbar.
// `toolbarInset` leaves room for the floating sidebar toggle when the main
// sidebar is collapsed.
//
// The cards follow the Swift body: Range & Trend first and full width, then
// Today, Goal, This Week, Reading Calendar, Selected Range, Speed Summary,
// Book Ranking and Shelf Comparison. Selected Range is its own card, and the
// trend card keeps only its header, controls, hint and chart. Decision D2
// keeps the two-column adaptation at >= 840 px; the default window is below
// it, so its detail page is single column.
//
// Before the first snapshot has loaded the loading card is shown alone. The
// empty state waits for a load that found no days, and a later reload keeps
// the loaded dashboard visible (`isInitialSnapshotLoading`).
//
// The calendar and shelf table scroll horizontally instead of shrinking their
// cells. Every colour is a Theme token; the data series is Theme.accent.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: page

    signal backRequested()

    property real toolbarInset: 0
    property int chartStyle: 0

    readonly property int outerMargin: 24
    readonly property int cardSpacing: 16
    readonly property bool wideLayout: page.width >= 840
    readonly property var weekdayLabels: [
        qsTr("Mon"), qsTr("Tue"), qsTr("Wed"), qsTr("Thu"),
        qsTr("Fri"), qsTr("Sat"), qsTr("Sun")
    ]

    function tint(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }

    function formatCharacters(value) {
        return Number(value).toLocaleString(Qt.locale(), "f", 0)
    }

    function formatDuration(seconds) {
        var minutes = Math.max(Math.round(seconds / 60), 0)
        if (minutes < 60)
            return qsTr("%1m").arg(minutes)
        var hours = Math.floor(minutes / 60)
        var rest = minutes % 60
        if (rest === 0)
            return qsTr("%1h").arg(hours)
        return qsTr("%1h %2m").arg(hours).arg(rest)
    }

    function formatSpeed(value) {
        return value < 0 ? "-" : qsTr("%1/h").arg(page.formatCharacters(value))
    }

    function fullDate(key) {
        return Qt.formatDate(new Date(key + "T12:00:00"), Qt.locale().dateFormat(Locale.LongFormat))
    }

    function dailyTargetText() {
        if (StatisticsViewModel.targetType === 1)
            return page.formatDuration(StatisticsViewModel.durationTargetMinutes * 60)
        return qsTr("%1 chars").arg(page.formatCharacters(StatisticsViewModel.characterTarget))
    }

    function rangeTitle() {
        var mode = StatisticsViewModel.rangeMode
        if (mode === 0)
            return qsTr("Recent year")
        if (mode === 1)
            return Qt.formatDate(StatisticsViewModel.rangeStart, "yyyy-M")
        if (mode === 2)
            return qsTr("%1-%2").arg(Qt.formatDate(StatisticsViewModel.rangeStart, "M/d"))
                                    .arg(Qt.formatDate(StatisticsViewModel.rangeEnd, "M/d"))
        return Qt.formatDate(StatisticsViewModel.rangeStart, "M/d")
    }

    function openBookDetails(item) {
        bookDetails.showBook(item)
    }

    Dialog {
        id: bookDetails
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(page.width - 24, 780)
        height: Math.min(page.height - 24, 560)
        modal: true
        standardButtons: Dialog.Close
        property var book: ({})
        property string dateKey: ""
        property string sessionId: ""
        readonly property var day: StatisticsViewModel.bookDays.find(
            item => item.dateKey === dateKey)

        function chooseSession(session) {
            sessionId = session ? session.id : ""
            countField.text = session ? String(session.characters) : ""
            hoursField.text = session ? String(session.hours) : ""
            minutesField.text = session ? String(session.minutes) : ""
        }

        function selectDay(item, preferred) {
            dateKey = item ? item.dateKey : ""
            var sessions = item ? item.sessions : []
            chooseSession(sessions.find(session => session.id === preferred)
                ?? (sessions.length === 1 ? sessions[0] : null))
        }

        function restoreSelection(date, session) {
            var days = StatisticsViewModel.bookDays
            selectDay(days.find(item => item.dateKey === date) ?? days[days.length - 1], session)
        }

        function showBook(item) {
            book = item
            StatisticsViewModel.openBookDetails(item.id)
            restoreSelection("", "")
            open()
        }

        title: qsTr("Statistics")
        contentItem: ColumnLayout {
            RowLayout {
                Layout.fillWidth: true
                Image {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 64
                    source: bookDetails.book.cover ?? ""
                    fillMode: Image.PreserveAspectFit
                }
                Label {
                    Layout.fillWidth: true
                    text: bookDetails.book.title ?? ""
                    color: Theme.textPrimary
                    font.bold: true
                    elide: Text.ElideRight
                }
            }
            Label {
                Layout.fillWidth: true
                visible: StatisticsViewModel.detailError.length > 0
                text: StatisticsViewModel.detailError
                color: Theme.popupDanger
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.AlertMessage
            }
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: bookDetails.width >= 620 ? 2 : 1
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 270
                    Layout.preferredHeight: 160
                    clip: true
                    model: StatisticsViewModel.bookDays
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: detailDay
                        required property var modelData
                        width: ListView.view.width
                        highlighted: detailDay.modelData.dateKey === bookDetails.dateKey
                        text: qsTr("%1\n%2 · %3 · %n session(s)", "", detailDay.modelData.sessions.length)
                            .arg(page.fullDate(detailDay.modelData.dateKey))
                            .arg(page.formatCharacters(detailDay.modelData.characters))
                            .arg(page.formatDuration(detailDay.modelData.duration))
                        onClicked: bookDetails.selectDay(detailDay.modelData, "")
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: StatisticsViewModel.bookDays.length === 0
                        text: qsTr("No reading records")
                        color: Theme.textSecondary
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Label {
                        Layout.fillWidth: true
                        text: bookDetails.day ? page.fullDate(bookDetails.dateKey)
                            : qsTr("Select a day to edit its reading sessions.")
                        color: Theme.textPrimary
                        wrapMode: Text.WordWrap
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: 120
                        clip: true
                        model: bookDetails.day?.sessions ?? []
                        ScrollBar.vertical: ScrollBar {}
                        delegate: ItemDelegate {
                            id: detailSession
                            required property var modelData
                            width: ListView.view.width
                            highlighted: detailSession.modelData.id === bookDetails.sessionId
                            text: qsTr("%1–%2\n%3 · %4 · %5")
                                .arg(Qt.formatTime(detailSession.modelData.start, Qt.locale().timeFormat(Locale.ShortFormat)))
                                .arg(Qt.formatTime(detailSession.modelData.end, Qt.locale().timeFormat(Locale.ShortFormat)))
                                .arg(page.formatCharacters(detailSession.modelData.characters))
                                .arg(page.formatDuration(detailSession.modelData.duration))
                                .arg(page.formatSpeed(detailSession.modelData.speed))
                            onClicked: bookDetails.chooseSession(detailSession.modelData)
                        }
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm
                        visible: bookDetails.sessionId.length > 0
                        TextField {
                            id: countField
                            width: 120
                            placeholderText: qsTr("Characters read")
                            Accessible.name: placeholderText
                            validator: IntValidator { bottom: 0 }
                        }
                        TextField {
                            id: hoursField
                            width: 62
                            placeholderText: qsTr("Hours")
                            Accessible.name: placeholderText
                            validator: IntValidator { bottom: 0 }
                        }
                        TextField {
                            id: minutesField
                            width: 70
                            placeholderText: qsTr("Minutes")
                            Accessible.name: placeholderText
                            validator: IntValidator { bottom: 0; top: 59 }
                        }
                        Button {
                            text: qsTr("Save")
                            enabled: countField.acceptableInput && hoursField.acceptableInput
                                && minutesField.acceptableInput
                            onClicked: {
                                if (StatisticsViewModel.saveBookSession(bookDetails.sessionId,
                                    Number(countField.text), Number(hoursField.text),
                                    Number(minutesField.text))) {
                                    bookDetails.restoreSelection(bookDetails.dateKey, bookDetails.sessionId)
                                }
                            }
                        }
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm
                        Button {
                            visible: bookDetails.sessionId.length > 0
                            text: qsTr("Delete Session")
                            onClicked: deleteStats.ask(0)
                        }
                        Button {
                            visible: !!bookDetails.day
                            text: qsTr("Delete This Day")
                            onClicked: deleteStats.ask(1)
                        }
                        Button {
                            visible: StatisticsViewModel.bookDays.length > 0
                            text: qsTr("Delete All Statistics")
                            onClicked: deleteStats.ask(2)
                        }
                    }
                }
            }
        }
    }

    StatisticsDeleteDialog {
        id: deleteStats
        sessionId: bookDetails.sessionId
        dateKey: bookDetails.dateKey
        onAccepted: {
            var id = deleteStats.scope === 0 ? deleteStats.sessionId : ""
            var date = deleteStats.scope === 1 ? deleteStats.dateKey : ""
            if ((deleteStats.scope === 0 && !id) || (deleteStats.scope === 1 && !date))
                return
            if (StatisticsViewModel.deleteBookSessions(id, date))
                bookDetails.restoreSelection(deleteStats.scope === 0 ? deleteStats.dateKey : "", "")
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBackground
    }

    PageHeader {
        id: headerBand

        objectName: "statisticsHeader"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 48
        leadingInset: page.toolbarInset
        title: qsTr("Statistics")
        subtitle: page.rangeTitle()
        onBackRequested: page.backRequested()
    }

    ScrollView {
        id: scroll

        objectName: "statisticsScroll"
        anchors.top: headerBand.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        contentWidth: content.width + page.outerMargin * 2
        contentHeight: content.height + page.outerMargin * 2
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            id: content

            x: page.outerMargin
            y: page.outerMargin
            width: Math.max(page.width - page.outerMargin * 2, 240)
            spacing: page.cardSpacing

            StatisticsCard {
                Layout.fillWidth: true
                visible: StatisticsViewModel.hasCorruptStatistics
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Some statistics are temporarily unavailable.")
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
            }

            StatisticsCard {
                objectName: "loadingCard"
                Layout.fillWidth: true
                visible: !StatisticsViewModel.hasLoaded
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Loading Statistics")
                        color: Theme.textSecondary
                    }
                    BusyIndicator { running: !StatisticsViewModel.hasLoaded }
                }
                Repeater {
                    model: 3
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        radius: 6
                        color: Theme.cardStroke
                    }
                }
            }

            StatisticsTrendCard {
                objectName: "trendCard"
                host: page
            }

            GridLayout {
                Layout.fillWidth: true
                columns: page.wideLayout ? 2 : 1
                columnSpacing: page.cardSpacing
                rowSpacing: page.cardSpacing

                StatisticsTodayCard {
                    objectName: "todayCard"
                    host: page
                }
                StatisticsGoalCard {
                    objectName: "goalCard"
                    host: page
                    visible: StatisticsViewModel.hasLoaded
                }
                StatisticsWeekCard {
                    objectName: "weekCard"
                    host: page
                }
            }

            StatisticsCalendarCard {
                objectName: "calendarCard"
                host: page
            }

            StatisticsSelectedRangeCard {
                objectName: "selectedRangeCard"
                host: page
            }

            GridLayout {
                Layout.fillWidth: true
                columns: page.wideLayout ? 2 : 1
                columnSpacing: page.cardSpacing
                rowSpacing: page.cardSpacing

                StatisticsSpeedCard {
                    objectName: "speedCard"
                    host: page
                }
                StatisticsRankingCard {
                    objectName: "rankingCard"
                    host: page
                }
            }

            StatisticsShelfCard {
                objectName: "shelfCard"
                host: page
            }

            StatisticsCard {
                objectName: "emptyStateCard"
                Layout.fillWidth: true
                visible: StatisticsViewModel.hasLoaded && !StatisticsViewModel.hasStatistics

                Label {
                    Layout.fillWidth: true
                    text: qsTr("No Reading Records")
                    color: Theme.textPrimary
                    font.pixelSize: 18
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Open a book and start reading with statistics enabled.")
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Component.onCompleted: StatisticsViewModel.refresh()
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The statistics dashboard, following Features/Bookshelf/StatisticsDashboardView.swift
// (header, Today and goal ring, Goal, This Week, Reading Calendar,
// Range & Trend, Speed Summary, Book Ranking, Shelf Comparison) over the
// aggregation in StatisticsDashboardModels.swift. Values arrive already
// aggregated from StatisticsViewModel; this page only formats and draws them.
//
// The breakpoints follow the source in spirit: the top cards and the
// speed/ranking pair sit in two columns from 840 px and stack below it, and the
// calendar and shelf table scroll horizontally instead of shrinking their
// cells. The trend chart draws bars rather than reproducing the source's line
// chart style switch.
//
// Every colour is a Theme token; the data series is Theme.accent.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: page

    signal backRequested()

    readonly property int outerMargin: 24
    readonly property int cardSpacing: 16
    readonly property int chartHeight: 224
    readonly property int chartLabelHeight: 22
    readonly property bool wideLayout: page.width >= 840
    readonly property var weekdayLabels: [
        qsTr("Mon"), qsTr("Tue"), qsTr("Wed"), qsTr("Thu"),
        qsTr("Fri"), qsTr("Sat"), qsTr("Sun")
    ]

    readonly property string selectedKey: page.dateKey(StatisticsViewModel.selectedDate)
    readonly property string rangeStartKey: page.dateKey(StatisticsViewModel.rangeStart)
    readonly property string rangeEndKey: page.dateKey(StatisticsViewModel.rangeEnd)
    readonly property bool rangeHighlight: StatisticsViewModel.rangeMode !== 0
    readonly property real trendMaxValue: page.trendMaximum()
    readonly property real rankingMaxValue: page.rankingMaximum()

    property int chartStyle: 0

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

    Dialog {
        id: deleteStats
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Cancel | Dialog.Ok
        property int scope: 0
        function ask(value) {
            scope = value
            open()
        }
        title: scope === 0 ? qsTr("Delete This Session") : scope === 1
            ? qsTr("Delete This Day") : qsTr("Delete All Statistics")
        contentItem: Label {
            text: deleteStats.scope === 0 ? qsTr("Delete the selected reading session? This cannot be undone.")
                : deleteStats.scope === 1 ? qsTr("Delete the selected day's statistics? This cannot be undone.")
                : qsTr("Delete all statistics for this book? This cannot be undone.")
            wrapMode: Text.WordWrap
        }
        onAccepted: {
            var id = scope === 0 ? bookDetails.sessionId : ""
            var date = scope === 1 ? bookDetails.dateKey : ""
            if ((scope === 0 && !id) || (scope === 1 && !date))
                return
            if (StatisticsViewModel.deleteBookSessions(id, date))
                bookDetails.restoreSelection(scope === 0 ? bookDetails.dateKey : "", "")
        }
    }

    component Card: Rectangle {
        default property alias content: body.data
        radius: Theme.cardRadius
        color: Theme.cardBackground
        border.width: 1
        border.color: Theme.cardStroke
        implicitWidth: body.implicitWidth + 36
        implicitHeight: body.implicitHeight + 36

        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 18
            spacing: 10
        }
    }

    component SectionHeader: ColumnLayout {
        id: header
        property string title
        property string detail
        spacing: 3

        Label {
            Layout.fillWidth: true
            text: header.title
            color: Theme.textPrimary
            font.pixelSize: 16
            font.bold: true
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            visible: header.detail.length > 0
            text: header.detail
            color: Theme.textSecondary
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    component MetricTile: Rectangle {
        id: tile
        property string label
        property string value
        implicitWidth: 96
        implicitHeight: tileLayout.implicitHeight + 22
        radius: 12
        color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.05)

        ColumnLayout {
            id: tileLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 11
            spacing: 3

            Label {
                Layout.fillWidth: true
                text: tile.label
                color: Theme.textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                text: tile.value
                color: Theme.textPrimary
                font.pixelSize: 17
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    component Segments: RowLayout {
        id: segments
        property var labels: []
        property int currentIndex: 0
        signal activated(int index)
        spacing: 3

        Repeater {
            model: segments.labels
            delegate: Button {
                id: segment
                required property int index
                required property var modelData
                text: String(modelData)
                leftPadding: 10
                rightPadding: 10
                topPadding: 5
                bottomPadding: 5
                onClicked: segments.activated(segment.index)
                background: Rectangle {
                    radius: 8
                    color: segments.currentIndex === segment.index
                        ? Theme.accent
                        : Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                                  Theme.textPrimary.b, 0.06)
                    border.width: 1
                    border.color: segments.currentIndex === segment.index
                        ? Theme.accent
                        : Theme.cardStroke
                }
                contentItem: Text {
                    text: segment.text
                    color: segments.currentIndex === segment.index
                        ? Theme.popupBackground
                        : Theme.textPrimary
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }
    }

    component StepperRow: RowLayout {
        id: stepper
        property string label
        property string valueText
        property bool canDecrease: true
        property bool canIncrease: true
        signal decrease()
        signal increase()
        Layout.fillWidth: true
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: stepper.label
            color: Theme.textPrimary
            font.pixelSize: 13
            elide: Text.ElideRight
        }
        Label {
            text: stepper.valueText
            color: Theme.textSecondary
            font.pixelSize: 13
            font.bold: true
        }
        Button {
            implicitWidth: 30
            text: "\u2212"
            enabled: stepper.canDecrease
            onClicked: stepper.decrease()
        }
        Button {
            implicitWidth: 30
            text: "+"
            enabled: stepper.canIncrease
            onClicked: stepper.increase()
        }
    }

    component GoalRing: Item {
        id: ring
        property int percent: 0
        property color accent: Theme.accent
        property color track: Theme.cardStroke

        onPercentChanged: ringCanvas.requestPaint()
        onWidthChanged: ringCanvas.requestPaint()
        onHeightChanged: ringCanvas.requestPaint()

        Canvas {
            id: ringCanvas
            anchors.fill: parent

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, ringCanvas.width, ringCanvas.height)
                var lineWidth = 13
                var center = Math.min(ringCanvas.width, ringCanvas.height) / 2
                var radius = Math.max(center - lineWidth / 2 - 1, 0)
                ctx.lineWidth = lineWidth
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.arc(center, center, radius, 0, Math.PI * 2)
                ctx.strokeStyle = ring.track
                ctx.stroke()

                var ratio = Math.max(0, Math.min(ring.percent / 100, 1))
                if (ratio > 0) {
                    ctx.beginPath()
                    ctx.arc(center, center, radius, -Math.PI / 2,
                            -Math.PI / 2 + Math.PI * 2 * ratio)
                    ctx.strokeStyle = ring.accent
                    ctx.stroke()
                }
            }
        }

        Connections {
            target: Theme
            function onDarkChanged() {
                ringCanvas.requestPaint()
            }
            function onAccentChanged() {
                ringCanvas.requestPaint()
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: 1

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: ring.percent + "%"
                color: Theme.textPrimary
                font.pixelSize: 20
                font.bold: true
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Goal")
                color: Theme.textSecondary
                font.pixelSize: 10
            }
        }
    }

    component TableCell: Label {
        property int cellWidth: 100
        property bool rightAligned: false
        width: cellWidth
        topPadding: 6
        bottomPadding: 6
        color: Theme.textPrimary
        font.pixelSize: 12
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: rightAligned ? Text.AlignRight : Text.AlignLeft
    }

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

    function formatSignedPercent(value) {
        return value > 0 ? "+" + value + "%" : value + "%"
    }

    function monthDay(dateKey) {
        var parts = dateKey.split("-")
        if (parts.length !== 3)
            return dateKey
        return Number(parts[1]) + "/" + Number(parts[2])
    }

    function formatSpeedDay(dateKey, speed) {
        if (dateKey.length === 0 || speed < 0)
            return "-"
        return qsTr("%1 · %2").arg(page.monthDay(dateKey)).arg(page.formatSpeed(speed))
    }

    function dateKey(date) {
        return date ? Qt.formatDate(date, "yyyy-MM-dd") : ""
    }

    function stepCharacter(direction) {
        StatisticsViewModel.characterTarget = StatisticsViewModel.characterTarget
            + direction * StatisticsViewModel.targetLimits.characterStep
    }

    function stepDuration(direction) {
        StatisticsViewModel.durationTargetMinutes = StatisticsViewModel.durationTargetMinutes
            + direction * StatisticsViewModel.targetLimits.durationStep
    }

    function stepWeeklyDays(direction) {
        StatisticsViewModel.weeklyTargetDays = StatisticsViewModel.weeklyTargetDays
            + direction * StatisticsViewModel.targetLimits.weeklyStep
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

    function trendValue(point) {
        if (StatisticsViewModel.trendMetric === 1)
            return point.time
        if (StatisticsViewModel.trendMetric === 2)
            return point.speed >= 0 ? point.speed : null
        return point.characters
    }

    function trendMaximum() {
        var points = StatisticsViewModel.trend
        var maximum = 0
        for (var index = 0; index < points.length; ++index) {
            var value = page.trendValue(points[index])
            if (value !== null && isFinite(value) && value > maximum)
                maximum = value
        }
        return Math.max(maximum * 1.1, 1)
    }

    function rankingMaximum() {
        var rows = StatisticsViewModel.ranking
        var maximum = 1
        for (var index = 0; index < rows.length; ++index)
            maximum = Math.max(maximum, rows[index].value)
        return maximum
    }

    function rankingValueText(row) {
        if (StatisticsViewModel.rankingMetric === 1)
            return page.formatDuration(row.time)
        if (StatisticsViewModel.rankingMetric === 2)
            return page.formatSpeed(row.speed)
        return page.formatCharacters(row.characters)
    }

    function fullDate(key) {
        return Qt.formatDate(new Date(key + "T12:00:00"), Qt.locale().dateFormat(Locale.LongFormat))
    }

    function trendDescription(point) {
        var value = page.trendValue(point)
        var metric = value === null ? "—" : page.formatAxis(value)
        var description = [point.id, qsTr("Metric: %1").arg(metric),
            qsTr("Characters: %1").arg(page.formatCharacters(point.characters)),
            qsTr("Duration: %1").arg(page.formatDuration(point.time)),
            qsTr("Speed: %1").arg(page.formatSpeed(point.speed))]
        if (point.books.length > 0) {
            description.push(qsTr("Books"))
            for (var i = 0; i < point.books.length; ++i) {
                description.push(qsTr("%1: %2").arg(point.books[i].title)
                    .arg(page.formatCharacters(point.books[i].characters)))
            }
        }
        return description.join("\n")
    }

    function barWidth() {
        if (StatisticsViewModel.trendGrain === 1)
            return 22
        if (StatisticsViewModel.trendGrain === 2)
            return 28
        return 12
    }

    function barSpacing() {
        if (StatisticsViewModel.trendGrain === 1)
            return 8
        if (StatisticsViewModel.trendGrain === 2)
            return 10
        return 5
    }

    function labelWidth() {
        if (StatisticsViewModel.trendGrain === 1)
            return 66
        if (StatisticsViewModel.trendGrain === 2)
            return 58
        return 42
    }

    function shouldShowTrendLabel(index) {
        var count = StatisticsViewModel.trend.length
        if (count <= 8)
            return true
        var stride = Math.max(Math.ceil(count / 8), 1)
        var last = count - 1
        var lastShown = Math.floor(last / stride) * stride
        // The final label replaces a grid label that would sit under it.
        if (index === last)
            return last - lastShown >= stride || lastShown === last
        if (index === lastShown && last - lastShown < stride)
            return false
        return index % stride === 0
    }

    function compactNumber(value) {
        if (value >= 1000000)
            return (value / 1000000).toFixed(1) + "M"
        if (value >= 1000)
            return (value / 1000).toFixed(1) + "k"
        return String(value)
    }

    function formatAxis(value) {
        if (StatisticsViewModel.trendMetric === 1)
            return page.formatDuration(value)
        var compact = page.compactNumber(Math.round(value))
        if (StatisticsViewModel.trendMetric === 2)
            return qsTr("%1/h").arg(compact)
        return compact
    }

    function weekSlice(weekIndex) {
        return StatisticsViewModel.calendarDays.slice(weekIndex * 7, weekIndex * 7 + 7)
    }

    function scrollCalendarToSelection() {
        var index = StatisticsViewModel.calendarDays.findIndex(
            day => day.dateKey === page.selectedKey)
        if (index >= 0) {
            var x = Math.floor(index / 7) * (12 + weeksRow.spacing)
            calendarScroll.contentX = Math.min(
                Math.max(0, calendarScroll.contentWidth - calendarScroll.width),
                Math.max(0, x - calendarScroll.width / 2))
        }
    }

    onSelectedKeyChanged: Qt.callLater(page.scrollCalendarToSelection)
    Component.onCompleted: StatisticsViewModel.refresh()

    Connections {
        target: StatisticsViewModel
        function onDataChanged() {
            Qt.callLater(page.scrollCalendarToSelection)
            trendLine.requestPaint()
        }
        function onViewChanged() {
            trendLine.requestPaint()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBackground
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
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

            PageHeader {
                title: qsTr("Statistics")
                subtitle: page.rangeTitle()
                onBackRequested: page.backRequested()
            }

            Card {
                Layout.fillWidth: true
                visible: StatisticsViewModel.hasCorruptStatistics
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Some statistics are temporarily unavailable.")
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
            }

            Card {
                Layout.fillWidth: true
                visible: StatisticsViewModel.loading
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Loading Statistics")
                        color: Theme.textSecondary
                    }
                    BusyIndicator { running: StatisticsViewModel.loading }
                }
                Repeater {
                    model: StatisticsViewModel.hasLoaded ? 0 : 3
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        radius: 6
                        color: Theme.cardStroke
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: page.cardSpacing
                visible: StatisticsViewModel.hasLoaded

                GridLayout {
                    Layout.fillWidth: true
                    columns: page.wideLayout ? 2 : 1
                    columnSpacing: page.cardSpacing
                    rowSpacing: page.cardSpacing

                    Card {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        visible: StatisticsViewModel.hasStatistics

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            GoalRing {
                                Layout.preferredWidth: 108
                                Layout.preferredHeight: 108
                                percent: StatisticsViewModel.today.targetPercent
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                SectionHeader {
                                    Layout.fillWidth: true
                                    title: qsTr("Today")
                                    detail: qsTr("Goal: %1").arg(page.dailyTargetText())
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 8
                                    rowSpacing: 8

                                    MetricTile {
                                        Layout.fillWidth: true
                                        label: qsTr("Duration")
                                        value: page.formatDuration(StatisticsViewModel.today.duration)
                                    }
                                    MetricTile {
                                        Layout.fillWidth: true
                                        label: qsTr("Characters")
                                        value: page.formatCharacters(StatisticsViewModel.today.characters)
                                    }
                                    MetricTile {
                                        Layout.fillWidth: true
                                        label: qsTr("Speed")
                                        value: page.formatSpeed(StatisticsViewModel.today.speed)
                                    }
                                    MetricTile {
                                        Layout.fillWidth: true
                                        label: qsTr("Streak")
                                        value: qsTr("%1 days").arg(StatisticsViewModel.today.streak)
                                    }
                                }
                            }
                        }
                    }

                    Card {
                        Layout.fillWidth: true

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Goal")
                            detail: page.dailyTargetText()
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Goal Type")
                                color: Theme.textPrimary
                                font.pixelSize: 13
                            }
                            Segments {
                                labels: [qsTr("Characters"), qsTr("Duration")]
                                currentIndex: StatisticsViewModel.targetType
                                onActivated: function (index) {
                                    StatisticsViewModel.targetType = index
                                }
                            }
                        }

                        StepperRow {
                            visible: StatisticsViewModel.targetType === 0
                            label: qsTr("Character Target")
                            valueText: qsTr("%1 chars").arg(
                                page.formatCharacters(StatisticsViewModel.characterTarget))
                            canDecrease: StatisticsViewModel.characterTarget
                                         > StatisticsViewModel.targetLimits.characterMin
                            canIncrease: StatisticsViewModel.characterTarget
                                         < StatisticsViewModel.targetLimits.characterMax
                            onDecrease: page.stepCharacter(-1)
                            onIncrease: page.stepCharacter(1)
                        }

                        StepperRow {
                            visible: StatisticsViewModel.targetType === 1
                            label: qsTr("Duration Target")
                            valueText: page.formatDuration(
                                StatisticsViewModel.durationTargetMinutes * 60)
                            canDecrease: StatisticsViewModel.durationTargetMinutes
                                         > StatisticsViewModel.targetLimits.durationMin
                            canIncrease: StatisticsViewModel.durationTargetMinutes
                                         < StatisticsViewModel.targetLimits.durationMax
                            onDecrease: page.stepDuration(-1)
                            onIncrease: page.stepDuration(1)
                        }

                        StepperRow {
                            label: qsTr("Weekly Target Days")
                            valueText: String(StatisticsViewModel.weeklyTargetDays)
                            canDecrease: StatisticsViewModel.weeklyTargetDays
                                         > StatisticsViewModel.targetLimits.weeklyMin
                            canIncrease: StatisticsViewModel.weeklyTargetDays
                                         < StatisticsViewModel.targetLimits.weeklyMax
                            onDecrease: page.stepWeeklyDays(-1)
                            onIncrease: page.stepWeeklyDays(1)
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Goals recalculate historical progress and streaks.")
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }

                    Card {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        Layout.columnSpan: page.wideLayout ? 2 : 1
                        visible: StatisticsViewModel.hasStatistics

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("This Week")
                            detail: qsTr("%1/7 days").arg(
                                StatisticsViewModel.week.metTargetDays)
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8

                            MetricTile {
                                Layout.fillWidth: true
                                label: qsTr("Duration")
                                value: page.formatDuration(StatisticsViewModel.week.duration)
                            }
                            MetricTile {
                                Layout.fillWidth: true
                                label: qsTr("Characters")
                                value: page.formatCharacters(StatisticsViewModel.week.characters)
                            }
                            MetricTile {
                                Layout.fillWidth: true
                                label: qsTr("Avg Characters")
                                value: page.formatCharacters(
                                    StatisticsViewModel.week.averageCharacters)
                            }
                            MetricTile {
                                Layout.fillWidth: true
                                label: qsTr("Speed")
                                value: page.formatSpeed(StatisticsViewModel.week.speed)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: StatisticsViewModel.week.days
                                delegate: Item {
                                    id: chip
                                    required property int index
                                    required property var modelData
                                    Layout.fillWidth: true
                                    implicitHeight: 50

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 10
                                        color: chip.modelData.metTarget
                                            ? page.tint(Theme.accent, 0.14)
                                            : Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                                                      Theme.textPrimary.b, 0.06)
                                        border.width: chip.modelData.isToday ? 1.5 : 0
                                        border.color: page.tint(Theme.accent, 0.5)
                                    }

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 2

                                        Label {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            text: page.weekdayLabels[chip.index]
                                            color: chip.modelData.metTarget
                                                ? Theme.accent
                                                : Theme.textSecondary
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                        Label {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            text: chip.modelData.percent >= 0
                                                ? chip.modelData.percent + "%"
                                                : "-"
                                            color: chip.modelData.metTarget
                                                ? Theme.accent
                                                : Theme.textSecondary
                                            font.pixelSize: 10
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: !StatisticsViewModel.hasStatistics

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

                Card {
                    Layout.fillWidth: true
                    visible: StatisticsViewModel.hasStatistics

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Reading Calendar")
                        detail: page.rangeTitle()
                    }

                    Flickable {
                        id: calendarScroll
                        Layout.fillWidth: true
                        Layout.preferredHeight: 7 * 12 + 6 * 4 + 18
                        contentWidth: weeksRow.width
                        contentHeight: height
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.horizontal: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        Row {
                            id: weeksRow
                            spacing: 4

                            Repeater {
                                model: Math.ceil(StatisticsViewModel.calendarDays.length / 7)
                                delegate: Column {
                                    id: weekColumn
                                    required property int index
                                    spacing: 4

                                    Repeater {
                                        model: page.weekSlice(weekColumn.index)
                                        delegate: Rectangle {
                                            id: dayCell
                                            required property var modelData
                                            width: 12
                                            height: 12
                                            radius: 3
                                            color: dayCell.modelData.factor > 0
                                                ? page.tint(Theme.accent, dayCell.modelData.factor)
                                                : Theme.cardStroke
                                            border.width: dayCell.selected ? 2
                                                : dayCell.inRange ? 1 : 0
                                            border.color: dayCell.selected
                                                ? Theme.accent
                                                : page.tint(Theme.accent, 0.45)

                                            readonly property bool selected:
                                                dayCell.modelData.dateKey === page.selectedKey
                                            readonly property bool inRange:
                                                page.rangeHighlight
                                                && dayCell.modelData.dateKey >= page.rangeStartKey
                                                && dayCell.modelData.dateKey <= page.rangeEndKey
                                            readonly property string summary: qsTr("%1: %2")
                                                .arg(page.fullDate(dayCell.modelData.dateKey))
                                                .arg(page.formatCharacters(dayCell.modelData.characters))
                                            Accessible.role: Accessible.Button
                                            Accessible.name: dayCell.summary
                                            Accessible.onPressAction: StatisticsViewModel.selectCalendarDay(
                                                dayCell.modelData.dateKey)
                                            ToolTip.visible: dayMouse.containsMouse
                                            ToolTip.delay: 400
                                            ToolTip.text: dayCell.summary

                                            MouseArea {
                                                id: dayMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                onClicked: StatisticsViewModel.selectCalendarDay(
                                                    dayCell.modelData.dateKey)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Selected: %1").arg(page.fullDate(page.selectedKey))
                            color: Theme.accent
                            font.bold: true
                        }
                        Label {
                            visible: !StatisticsViewModel.selectedCalendarDay.hasRecords
                            text: qsTr("No reading records")
                            color: Theme.textSecondary
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        MetricTile {
                            label: qsTr("Characters")
                            value: page.formatCharacters(StatisticsViewModel.selectedCalendarDay.characters)
                            Layout.fillWidth: true
                        }
                        MetricTile {
                            label: qsTr("Duration")
                            value: page.formatDuration(StatisticsViewModel.selectedCalendarDay.duration)
                            Layout.fillWidth: true
                        }
                        MetricTile {
                            label: qsTr("Books")
                            value: page.formatCharacters(StatisticsViewModel.selectedCalendarDay.books)
                            Layout.fillWidth: true
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: StatisticsViewModel.hasStatistics

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Range & Trend")
                        detail: page.rangeTitle()
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 8

                        MetricTile {
                            Layout.fillWidth: true
                            label: qsTr("Duration")
                            value: page.formatDuration(
                                StatisticsViewModel.selectedRange.duration)
                        }
                        MetricTile {
                            Layout.fillWidth: true
                            label: qsTr("Characters")
                            value: page.formatCharacters(
                                StatisticsViewModel.selectedRange.characters)
                        }
                        MetricTile {
                            Layout.fillWidth: true
                            label: qsTr("Speed")
                            value: page.formatSpeed(StatisticsViewModel.selectedRange.speed)
                        }
                        MetricTile {
                            Layout.fillWidth: true
                            label: StatisticsViewModel.rangeMode === 3
                                ? qsTr("Goal Progress") : qsTr("Days Met")
                            value: StatisticsViewModel.rangeMode === 3
                                ? StatisticsViewModel.selectedRange.targetProgressPercent + "%"
                                : qsTr("%1 days").arg(
                                    StatisticsViewModel.selectedRange.targetDays)
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 16

                        Column {
                            spacing: 4

                            Label {
                                text: qsTr("Range")
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                            Segments {
                                labels: [qsTr("Year"), qsTr("Month"), qsTr("Week"), qsTr("Day")]
                                currentIndex: StatisticsViewModel.rangeMode
                                onActivated: function (index) {
                                    StatisticsViewModel.rangeMode = index
                                }
                            }
                        }

                        Column {
                            spacing: 4

                            Label {
                                text: qsTr("Time Grain")
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                            Segments {
                                labels: [qsTr("Day"), qsTr("Week"), qsTr("Month")]
                                currentIndex: StatisticsViewModel.trendGrain
                                onActivated: function (index) {
                                    StatisticsViewModel.trendGrain = index
                                }
                            }
                        }

                        Column {
                            spacing: 4

                            Label {
                                text: qsTr("Metric")
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                            Segments {
                                labels: [qsTr("Characters"), qsTr("Duration"), qsTr("Speed")]
                                currentIndex: StatisticsViewModel.trendMetric
                                onActivated: function (index) {
                                    StatisticsViewModel.trendMetric = index
                                }
                            }
                        }
                        Column {
                            spacing: 4
                            Label {
                                text: qsTr("Style")
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                            Segments {
                                labels: [qsTr("Bar"), qsTr("Line")]
                                currentIndex: page.chartStyle
                                onActivated: function (index) { page.chartStyle = index }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Range filters all dashboard cards. Time Grain changes only the trend chart.")
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 150
                        visible: StatisticsViewModel.trend.length === 0
                        text: qsTr("No trend data")
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: page.chartHeight
                        visible: StatisticsViewModel.trend.length > 0
                        spacing: 8

                        Item {
                            Layout.preferredWidth: 56
                            Layout.fillHeight: true

                            Label {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                text: page.formatAxis(page.trendMaxValue)
                                color: Theme.textSecondary
                                font.pixelSize: 10
                            }
                            Label {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: page.formatAxis(page.trendMaxValue / 2)
                                color: Theme.textSecondary
                                font.pixelSize: 10
                            }
                            Label {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                text: "0"
                                color: Theme.textSecondary
                                font.pixelSize: 10
                            }
                        }

                        Flickable {
                            id: trendScroll
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentWidth: Math.max(trendRow.width + page.labelWidth(), width)
                            contentHeight: height
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.horizontal: ScrollBar {
                                policy: ScrollBar.AsNeeded
                            }

                                Canvas {
                                    id: trendLine
                                    anchors.fill: trendRow
                                    visible: page.chartStyle === 1
                                    readonly property color accent: Theme.accent
                                    onAccentChanged: requestPaint()
                                    onVisibleChanged: requestPaint()
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, width, height)
                                        ctx.strokeStyle = accent
                                        ctx.fillStyle = accent
                                        ctx.lineWidth = 2
                                        var points = StatisticsViewModel.trend
                                        var slot = page.barWidth() + page.barSpacing()
                                        var connected = false
                                        ctx.beginPath()
                                        for (var i = 0; i < points.length; ++i) {
                                            var value = page.trendValue(points[i])
                                            if (value === null) {
                                                connected = false
                                                continue
                                            }
                                            var x = (i + 0.5) * slot
                                            var y = Math.min(height - 2, height * (1 - value / page.trendMaxValue))
                                            if (connected)
                                                ctx.lineTo(x, y)
                                            else
                                                ctx.moveTo(x, y)
                                            ctx.fillRect(x - 2, y - 2, 4, 4)
                                            connected = true
                                        }
                                        ctx.stroke()
                                    }
                                }

                            Row {
                                id: trendRow
                                x: page.labelWidth() / 2
                                height: trendScroll.height - page.chartLabelHeight
                                spacing: 0

                                Repeater {
                                    model: StatisticsViewModel.trend
                                    delegate: Item {
                                        id: barColumn
                                        required property int index
                                        required property var modelData
                                        width: page.barWidth() + page.barSpacing()
                                        height: trendRow.height

                                        readonly property var value: page.trendValue(barColumn.modelData)
                                        readonly property real barHeight:
                                            barColumn.value === null || barColumn.value <= 0
                                            ? 2
                                            : Math.max(trendRow.height * barColumn.value
                                                       / page.trendMaxValue, 6)

                                        Rectangle {
                                            visible: page.chartStyle === 0
                                            width: page.barWidth()
                                            height: barColumn.barHeight
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            anchors.bottom: parent.bottom
                                            radius: 5
                                            color: page.tint(Theme.accent,
                                                             barHover.hovered ? 1 : 0.86)

                                        }
                                        HoverHandler { id: barHover }
                                        ToolTip.visible: barHover.hovered
                                        ToolTip.delay: 350
                                        ToolTip.text: page.trendDescription(barColumn.modelData)
                                    }
                                }
                            }

                            Row {
                                id: labelRow
                                x: page.labelWidth() / 2
                                anchors.top: trendRow.bottom
                                height: page.chartLabelHeight
                                spacing: 0

                                Repeater {
                                    model: StatisticsViewModel.trend
                                    delegate: Item {
                                        id: labelColumn
                                        required property int index
                                        required property var modelData
                                        width: page.barWidth() + page.barSpacing()
                                        height: page.chartLabelHeight

                                        Label {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            width: page.labelWidth()
                                            text: page.shouldShowTrendLabel(labelColumn.index)
                                                ? labelColumn.modelData.label : ""
                                            color: Theme.textSecondary
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignHCenter
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    visible: StatisticsViewModel.hasStatistics
                    columns: page.wideLayout ? 2 : 1
                    columnSpacing: page.cardSpacing
                    rowSpacing: page.cardSpacing

                    Card {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Speed Summary")
                            detail: page.rangeTitle()
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8

                            MetricTile {
                                Layout.fillWidth: true
                                label: qsTr("Weighted Avg")
                                value: page.formatSpeed(StatisticsViewModel.speed.weighted)
                            }
                            MetricTile {
                                Layout.fillWidth: true
                                label: qsTr("Typical Day")
                                value: page.formatSpeed(StatisticsViewModel.speed.typical)
                            }
                            MetricTile {
                                Layout.fillWidth: true
                                label: qsTr("Last 7 Active Days")
                                value: page.formatSpeed(StatisticsViewModel.speed.recent)
                            }
                            MetricTile {
                                Layout.fillWidth: true
                                label: qsTr("Change vs First 14")
                                value: StatisticsViewModel.speed.hasChange
                                    ? page.formatSignedPercent(StatisticsViewModel.speed.change)
                                    : "-"
                            }
                            MetricTile {
                                Layout.fillWidth: true
                                label: qsTr("Fastest Day")
                                value: page.formatSpeedDay(StatisticsViewModel.speed.bestDate,
                                                           StatisticsViewModel.speed.bestSpeed)
                            }
                            MetricTile {
                                Layout.fillWidth: true
                                label: qsTr("Slowest Day")
                                value: page.formatSpeedDay(StatisticsViewModel.speed.worstDate,
                                                           StatisticsViewModel.speed.worstSpeed)
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Speed ignores reading samples under 1 minute.")
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }

                    Card {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop

                        SectionHeader {
                            Layout.fillWidth: true
                            title: qsTr("Book Ranking")
                            detail: page.rangeTitle()
                        }

                        Segments {
                            labels: [qsTr("Characters"), qsTr("Duration"), qsTr("Speed")]
                            currentIndex: StatisticsViewModel.rankingMetric
                            onActivated: function (index) { StatisticsViewModel.rankingMetric = index }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 100
                            visible: StatisticsViewModel.ranking.length === 0
                            text: qsTr("No reading records")
                            color: Theme.textSecondary
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        Repeater {
                            model: StatisticsViewModel.ranking
                            delegate: Item {
                                id: rankRow
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 54
                                Accessible.role: Accessible.Button
                                Accessible.name: rankRow.modelData.title
                                Accessible.onPressAction: bookDetails.showBook(rankRow.modelData)
                                activeFocusOnTab: true
                                Keys.onReturnPressed: bookDetails.showBook(rankRow.modelData)
                                Keys.onSpacePressed: bookDetails.showBook(rankRow.modelData)
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 4
                                    color: page.tint(Theme.accent, 0.12)
                                    visible: rankRow.activeFocus || rankTap.hovered
                                }
                                HoverHandler { id: rankTap }
                                TapHandler { onTapped: bookDetails.showBook(rankRow.modelData) }

                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 12

                                    Item {
                                        Layout.preferredWidth: 36
                                        Layout.preferredHeight: 48

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 4
                                            color: page.tint(Theme.textPrimary, 0.06)
                                            visible: rankRow.modelData.cover.length === 0
                                        }
                                        Image {
                                            anchors.fill: parent
                                            source: rankRow.modelData.cover
                                            fillMode: Image.PreserveAspectFit
                                            asynchronous: true
                                            visible: rankRow.modelData.cover.length > 0
                                        }
                                    }

                                    Label {
                                        Layout.preferredWidth: 120
                                        text: rankRow.modelData.title
                                        color: Theme.textPrimary
                                        font.pixelSize: 13
                                        font.bold: true
                                        maximumLineCount: 2
                                        wrapMode: Text.WordWrap
                                        elide: Text.ElideRight
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 9

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 4.5
                                            color: page.tint(Theme.textPrimary, 0.12)
                                        }
                                        Rectangle {
                                            width: Math.max(parent.width
                                                            * rankRow.modelData.value
                                                            / page.rankingMaxValue,
                                                            rankRow.modelData.value > 0
                                                            ? 2 : 0)
                                            height: parent.height
                                            radius: 4.5
                                            color: page.tint(Theme.accent, 0.78)
                                        }
                                    }

                                    Label {
                                        Layout.preferredWidth: 84
                                        text: page.rankingValueText(rankRow.modelData)
                                        color: Theme.textSecondary
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            visible: StatisticsViewModel.hasMoreBooks
                            text: qsTr("More Books")
                            onClicked: StatisticsViewModel.showMoreBooks()
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: StatisticsViewModel.hasStatistics

                    SectionHeader {
                        Layout.fillWidth: true
                        title: qsTr("Shelf Comparison")
                        detail: page.rangeTitle()
                    }

                    Flickable {
                        id: shelfScroll
                        Layout.fillWidth: true
                        Layout.preferredHeight: shelfColumn.height
                        contentWidth: shelfColumn.width
                        contentHeight: shelfColumn.height
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.horizontal: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        Column {
                            id: shelfColumn
                            width: Math.max(680, shelfScroll.width)
                            spacing: 0

                            Row {
                                width: parent.width
                                spacing: 10

                                TableCell {
                                    cellWidth: 160
                                    text: qsTr("Shelf")
                                    color: Theme.textSecondary
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                                TableCell {
                                    cellWidth: 56
                                    rightAligned: true
                                    text: qsTr("Books")
                                    color: Theme.textSecondary
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                                TableCell {
                                    cellWidth: 112
                                    rightAligned: true
                                    text: qsTr("Total Characters")
                                    color: Theme.textSecondary
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                                TableCell {
                                    cellWidth: 118
                                    rightAligned: true
                                    text: qsTr("Recorded Characters")
                                    color: Theme.textSecondary
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                                TableCell {
                                    cellWidth: 82
                                    rightAligned: true
                                    text: qsTr("Duration")
                                    color: Theme.textSecondary
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                                TableCell {
                                    cellWidth: 92
                                    rightAligned: true
                                    text: qsTr("Avg Speed")
                                    color: Theme.textSecondary
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                            }

                            Rectangle {
                                width: parent.width
                                height: 1
                                color: Theme.cardStroke
                            }

                            Label {
                                width: parent.width
                                height: 96
                                visible: StatisticsViewModel.shelfComparison.length === 0
                                text: qsTr("No reading records")
                                color: Theme.textSecondary
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            Repeater {
                                model: StatisticsViewModel.shelfComparison
                                delegate: Column {
                                    id: shelfRow
                                    required property int index
                                    required property var modelData
                                    width: shelfColumn.width
                                    spacing: 0

                                    Row {
                                        width: parent.width
                                        spacing: 10

                                        TableCell {
                                            cellWidth: 160
                                            text: shelfRow.modelData.name
                                        }
                                        TableCell {
                                            cellWidth: 56
                                            rightAligned: true
                                            text: String(shelfRow.modelData.books)
                                        }
                                        TableCell {
                                            cellWidth: 112
                                            rightAligned: true
                                            text: page.formatCharacters(
                                                shelfRow.modelData.totalCharacters)
                                        }
                                        TableCell {
                                            cellWidth: 118
                                            rightAligned: true
                                            text: page.formatCharacters(
                                                shelfRow.modelData.characters)
                                        }
                                        TableCell {
                                            cellWidth: 82
                                            rightAligned: true
                                            text: page.formatDuration(shelfRow.modelData.time)
                                        }
                                        TableCell {
                                            cellWidth: 92
                                            rightAligned: true
                                            text: page.formatSpeed(shelfRow.modelData.speed)
                                        }
                                    }

                                    Rectangle {
                                        width: parent.width
                                        height: 1
                                        color: Theme.cardStroke
                                        visible: shelfRow.index
                                                 < StatisticsViewModel.shelfComparison.length - 1
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

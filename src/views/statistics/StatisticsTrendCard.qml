// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Range & Trend card of the statistics dashboard, following
// `fullWidthTrendSection`/`trendSection` in
// Features/Bookshelf/StatisticsDashboardView.swift: the range, time grain,
// metric and style pickers, the hint and the chart. It stays first and full
// width above the dashboard columns, and carries no range metric tiles of its
// own. Decision D5 keeps the existing bar/line Canvas drawing.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

StatisticsCard {
    id: card

    property var host
    Layout.fillWidth: true
    visible: StatisticsViewModel.hasStatistics

    readonly property real maximum: card.trendMaximum()

    function trendValue(point) {
        if (StatisticsViewModel.trendMetric === 1)
            return point.time
        if (StatisticsViewModel.trendMetric === 2)
            return point.speed >= 0 ? point.speed : null
        return point.characters
    }

    function trendMaximum() {
        var points = StatisticsViewModel.trend
        var value = 0
        for (var index = 0; index < points.length; ++index) {
            var point = card.trendValue(points[index])
            if (point !== null && isFinite(point) && point > value)
                value = point
        }
        return Math.max(value * 1.1, 1)
    }

    // Bar, gap and axis label widths per time grain (day, week, month).
    function barWidth() {
        return Theme.trendBarWidths[StatisticsViewModel.trendGrain] ?? Theme.trendBarWidths[0]
    }

    function barSpacing() {
        return Theme.trendBarSpacings[StatisticsViewModel.trendGrain] ?? Theme.trendBarSpacings[0]
    }

    function labelWidth() {
        return Theme.trendLabelWidths[StatisticsViewModel.trendGrain] ?? Theme.trendLabelWidths[0]
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
            return card.host.formatDuration(value)
        var compact = card.compactNumber(Math.round(value))
        if (StatisticsViewModel.trendMetric === 2)
            return qsTr("%1/h").arg(compact)
        return compact
    }

    function trendDescription(point) {
        var value = card.trendValue(point)
        var metric = value === null ? "—" : card.formatAxis(value)
        var description = [point.id, qsTr("Metric: %1").arg(metric),
            qsTr("Characters: %1").arg(card.host.formatCharacters(point.characters)),
            qsTr("Duration: %1").arg(card.host.formatDuration(point.time)),
            qsTr("Speed: %1").arg(card.host.formatSpeed(point.speed))]
        if (point.books.length > 0) {
            description.push(qsTr("Books"))
            for (var i = 0; i < point.books.length; ++i) {
                description.push(qsTr("%1: %2").arg(point.books[i].title)
                    .arg(card.host.formatCharacters(point.books[i].characters)))
            }
        }
        return description.join("\n")
    }

    StatisticsSectionHeader {
        Layout.fillWidth: true
        title: qsTr("Range & Trend")
        detail: card.host.rangeTitle()
    }

    Flow {
        Layout.fillWidth: true
        spacing: Theme.spacingLg

        PickerGroup {
            title: qsTr("Range")

            StatisticsSegments {
                objectName: "rangeSegments"
                labels: [qsTr("Year"), qsTr("Month"), qsTr("Week"), qsTr("Day")]
                currentIndex: StatisticsViewModel.rangeMode
                onActivated: function (index) {
                    StatisticsViewModel.rangeMode = index
                }
            }
        }

        PickerGroup {
            title: qsTr("Time Grain")

            StatisticsSegments {
                labels: [qsTr("Day"), qsTr("Week"), qsTr("Month")]
                currentIndex: StatisticsViewModel.trendGrain
                onActivated: function (index) {
                    StatisticsViewModel.trendGrain = index
                }
            }
        }

        PickerGroup {
            title: qsTr("Metric")

            StatisticsSegments {
                labels: [qsTr("Characters"), qsTr("Duration"), qsTr("Speed")]
                currentIndex: StatisticsViewModel.trendMetric
                onActivated: function (index) {
                    StatisticsViewModel.trendMetric = index
                }
            }
        }
        PickerGroup {
            title: qsTr("Style")

            StatisticsSegments {
                labels: [qsTr("Bar"), qsTr("Line")]
                currentIndex: card.host.chartStyle
                onActivated: function (index) { card.host.chartStyle = index }
            }
        }
    }

    StyledLabel {
        Layout.fillWidth: true
        text: qsTr("Range filters all dashboard cards. Time Grain changes only the trend chart.")
        textStyle: Theme.footnoteFont
        secondary: true
        wrapMode: Text.WordWrap
    }

    StyledLabel {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.trendEmptyHeight
        visible: StatisticsViewModel.trend.length === 0
        text: qsTr("No trend data")
        textStyle: Theme.calloutFont
        secondary: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    RowLayout {
        objectName: "trendChart"
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.trendChartHeight
        visible: StatisticsViewModel.trend.length > 0
        spacing: Theme.spacingSm

        Item {
            Layout.preferredWidth: Theme.trendAxisWidth
            Layout.fillHeight: true

            AxisLabel {
                anchors.right: parent.right
                anchors.top: parent.top
                text: card.formatAxis(card.maximum)
            }
            AxisLabel {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: card.formatAxis(card.maximum / 2)
            }
            AxisLabel {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: "0"
            }
        }

        Flickable {
            id: trendScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: Math.max(trendRow.width + card.labelWidth(), width)
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            Canvas {
                id: trendLine
                anchors.fill: trendRow
                visible: card.host.chartStyle === 1
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
                    ctx.lineWidth = Theme.trendLineWidth
                    var points = StatisticsViewModel.trend
                    var slot = card.barWidth() + card.barSpacing()
                    var connected = false
                    ctx.beginPath()
                    for (var i = 0; i < points.length; ++i) {
                        var value = card.trendValue(points[i])
                        if (value === null) {
                            connected = false
                            continue
                        }
                        var x = (i + 0.5) * slot
                        var y = Math.min(height - Theme.trendPointSize / 2,
                                         height * (1 - value / card.maximum))
                        if (connected)
                            ctx.lineTo(x, y)
                        else
                            ctx.moveTo(x, y)
                        ctx.fillRect(x - Theme.trendPointSize / 2, y - Theme.trendPointSize / 2,
                                     Theme.trendPointSize, Theme.trendPointSize)
                        connected = true
                    }
                    ctx.stroke()
                }
            }

            Row {
                id: trendRow
                x: card.labelWidth() / 2
                height: trendScroll.height - Theme.trendLabelHeight
                spacing: 0

                Repeater {
                    model: StatisticsViewModel.trend
                    delegate: Item {
                        id: barColumn
                        required property int index
                        required property var modelData
                        width: card.barWidth() + card.barSpacing()
                        height: trendRow.height

                        readonly property var value: card.trendValue(barColumn.modelData)
                        readonly property real barHeight:
                            barColumn.value === null || barColumn.value <= 0
                            ? Theme.trendEmptyBarHeight
                            : Math.max(trendRow.height * barColumn.value
                                       / card.maximum, Theme.trendMinimumBarHeight)

                        Rectangle {
                            visible: card.host.chartStyle === 0
                            width: card.barWidth()
                            height: barColumn.barHeight
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            radius: Theme.trendBarRadius
                            color: card.host.tint(Theme.accent,
                                                  barHover.hovered ? 1 : Theme.trendBarOpacity)
                        }
                        HoverHandler { id: barHover }
                        ToolTip.visible: barHover.hovered
                        ToolTip.delay: 350
                        ToolTip.text: card.trendDescription(barColumn.modelData)
                    }
                }
            }

            Row {
                id: labelRow
                x: card.labelWidth() / 2
                anchors.top: trendRow.bottom
                height: Theme.trendLabelHeight
                spacing: 0

                Repeater {
                    model: StatisticsViewModel.trend
                    delegate: Item {
                        id: labelColumn
                        required property int index
                        required property var modelData
                        width: card.barWidth() + card.barSpacing()
                        height: Theme.trendLabelHeight

                        AxisLabel {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: card.labelWidth()
                            text: card.shouldShowTrendLabel(labelColumn.index)
                                ? labelColumn.modelData.label : ""
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    // A titled picker of the trend controls: caption label over the picker.
    component PickerGroup: Column {
        id: group

        property string title

        spacing: Theme.dashboardHeaderSpacing

        StyledLabel {
            text: group.title
            textStyle: Theme.captionFont
            weight: Theme.weightSemibold
            secondary: true
        }
    }

    // The chart's axis and period labels.
    component AxisLabel: StyledLabel {
        textStyle: Theme.caption2Font
        secondary: true
        monospacedDigits: true
    }

    Connections {
        target: StatisticsViewModel
        function onDataChanged() { trendLine.requestPaint() }
        function onViewChanged() { trendLine.requestPaint() }
    }
}

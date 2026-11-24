// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Range & Trend card of the statistics dashboard, following
// `fullWidthTrendSection`/`trendSection` in
// Features/Bookshelf/StatisticsDashboardView.swift: the range, time grain,
// metric and style pickers, the hint and the chart. It stays first and full
// width above the dashboard columns, and carries no range metric tiles of its
// own. Decision D5 keeps the existing bar/line Canvas drawing. The plot fits
// the card's width with gridlines at the y ticks, as Swift Charts lays out
// StatisticsTrendChartView, rather than scrolling sideways.

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

    // The plot fits the card: every period gets an equal slot of the plot
    // width, a bar fills part of its slot up to the grain's bar width, and a
    // date label is shown on every `labelStride`-th slot so whole labels never
    // overlap. Labels near the ends are clamped inside the plot.
    readonly property int pointCount: StatisticsViewModel.trend.length
    readonly property real slotWidth: card.pointCount > 0 ? plot.width / card.pointCount : 0
    readonly property real barWidth: Math.max(Theme.hairlineWidth,
        Math.min(Theme.trendBarWidths[StatisticsViewModel.trendGrain] ?? Theme.trendBarWidths[0],
                 card.slotWidth * Theme.trendBarFill))
    readonly property real labelWidth: Theme.trendLabelWidths[StatisticsViewModel.trendGrain]
                                       ?? Theme.trendLabelWidths[0]
    readonly property int labelStride: card.slotWidth > 0
        ? Math.max(1, Math.ceil((card.labelWidth + Theme.spacingXs) / card.slotWidth)) : 1
    // The y ticks, as fractions of the maximum: gridlines and axis labels.
    readonly property var ticks: [1, 0.5, 0]

    function shouldShowTrendLabel(index) {
        const last = card.pointCount - 1
        if (index === last)
            return true
        // The final label replaces a stride label that would collide with it.
        if (last - index < card.labelStride)
            return false
        return index % card.labelStride === 0
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

    // The selected range is named under the page title, so the card heading
    // carries only its title.
    StatisticsSectionHeader {
        Layout.fillWidth: true
        title: qsTr("Range & Trend")
    }

    // trendSection's filters: each picker under its caption, the four in one
    // row while they fit and otherwise in a two-column grid, so both pickers
    // of a column start at the same x.
    GridLayout {
        id: controls

        readonly property real oneRowWidth: rangeGroup.implicitWidth + grainGroup.implicitWidth
            + metricGroup.implicitWidth + styleGroup.implicitWidth + 3 * columnSpacing

        Layout.fillWidth: true
        // The card's column width, not the grid's own, which follows its columns.
        columns: controls.parent && controls.oneRowWidth <= controls.parent.width ? 4 : 2
        columnSpacing: Theme.trendControlSpacing
        rowSpacing: Theme.dashboardCardContentSpacing

        PickerGroup {
            id: rangeGroup
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
            id: grainGroup
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
            id: metricGroup
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
            id: styleGroup
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

        // The y axis: one label per tick, centred on its gridline.
        Item {
            Layout.preferredWidth: Theme.trendAxisWidth
            Layout.fillHeight: true

            Repeater {
                model: card.ticks

                delegate: AxisLabel {
                    required property real modelData

                    anchors.right: parent.right
                    y: Math.max(0, Math.min(plot.height - height,
                                            plot.height * (1 - modelData) - height / 2))
                    text: modelData === 0 ? "0" : card.formatAxis(card.maximum * modelData)
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                id: plot

                width: parent.width
                height: parent.height - Theme.trendLabelHeight

                // Swift Charts' default horizontal gridlines at the y ticks.
                Repeater {
                    model: card.ticks

                    delegate: Rectangle {
                        required property real modelData

                        width: plot.width
                        height: Theme.hairlineWidth
                        y: Math.min(plot.height - height, plot.height * (1 - modelData))
                        color: Theme.separator
                    }
                }

                Canvas {
                    id: trendLine

                    anchors.fill: parent
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
                        var connected = false
                        ctx.beginPath()
                        for (var i = 0; i < points.length; ++i) {
                            var value = card.trendValue(points[i])
                            if (value === null) {
                                connected = false
                                continue
                            }
                            var x = (i + 0.5) * card.slotWidth
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

                Repeater {
                    model: StatisticsViewModel.trend
                    delegate: Item {
                        id: barColumn
                        required property int index
                        required property var modelData
                        x: barColumn.index * card.slotWidth
                        width: card.slotWidth
                        height: plot.height

                        readonly property var value: card.trendValue(barColumn.modelData)
                        readonly property real barHeight:
                            barColumn.value === null || barColumn.value <= 0
                            ? Theme.trendEmptyBarHeight
                            : Math.max(plot.height * barColumn.value
                                       / card.maximum, Theme.trendMinimumBarHeight)

                        Rectangle {
                            visible: card.host.chartStyle === 0
                            width: card.barWidth
                            height: barColumn.barHeight
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            // Only the top is rounded, so every bar stands flat on
                            // the baseline; Rectangle clamps the radius to half a
                            // narrow bar's width.
                            topLeftRadius: Theme.trendBarRadius
                            topRightRadius: Theme.trendBarRadius
                            color: card.host.tint(Theme.accent,
                                                  barHover.hovered ? 1 : Theme.trendBarOpacity)
                        }
                        HoverHandler { id: barHover }
                        ToolTip.visible: barHover.hovered
                        ToolTip.delay: Theme.tooltipDelay
                        ToolTip.text: card.trendDescription(barColumn.modelData)
                    }
                }
            }

            // The baseline rule at 0, drawn over the bars' feet in the gridline
            // colour.
            Rectangle {
                y: plot.height - height
                width: plot.width
                height: Theme.hairlineWidth
                color: Theme.separator
            }

            // Period labels under the plot, clamped so the first and last
            // stay whole inside the card.
            Repeater {
                model: StatisticsViewModel.trend
                delegate: AxisLabel {
                    id: periodLabel
                    required property int index
                    required property var modelData
                    visible: card.shouldShowTrendLabel(periodLabel.index)
                    y: plot.height + (Theme.trendLabelHeight - height) / 2
                    x: Math.max(0, Math.min(plot.width - width,
                                            (periodLabel.index + 0.5) * card.slotWidth - width / 2))
                    text: periodLabel.modelData.label
                }
            }
        }
    }

    // trendControl: a caption2 semibold secondary caption over its picker.
    component PickerGroup: Column {
        id: group

        property string title

        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
        spacing: Theme.trendControlLabelSpacing

        StyledLabel {
            text: group.title
            textStyle: Theme.caption2Font
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

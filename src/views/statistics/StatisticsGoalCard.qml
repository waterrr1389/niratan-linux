// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Goal card of the statistics dashboard, following `targetSettingsSection`
// in Features/Bookshelf/StatisticsDashboardView.swift: the target kind picker
// and the character, duration and weekly-day steppers. Each control writes the
// matching StatisticsViewModel setting, which clamps and persists it.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

StatisticsCard {
    id: card

    property var host
    Layout.fillWidth: true

    StatisticsSectionHeader {
        Layout.fillWidth: true
        title: qsTr("Goal")
        detail: card.host.dailyTargetText()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.rowSpacing

        StyledLabel {
            Layout.fillWidth: true
            text: qsTr("Goal Type")
            elide: Text.ElideRight
        }
        StatisticsSegments {
            labels: [qsTr("Characters"), qsTr("Duration")]
            currentIndex: StatisticsViewModel.targetType
            onActivated: function (index) {
                StatisticsViewModel.targetType = index
            }
        }
    }

    StepperRow {
        objectName: "characterTargetStepper"
        visible: StatisticsViewModel.targetType === 0
        label: qsTr("Character Target")
        valueText: qsTr("%1 chars").arg(
            card.host.formatCharacters(StatisticsViewModel.characterTarget))
        value: StatisticsViewModel.characterTarget
        from: StatisticsViewModel.targetLimits.characterMin
        to: StatisticsViewModel.targetLimits.characterMax
        stepSize: StatisticsViewModel.targetLimits.characterStep
        onStepped: value => StatisticsViewModel.characterTarget = value
    }

    StepperRow {
        visible: StatisticsViewModel.targetType === 1
        label: qsTr("Duration Target")
        valueText: card.host.formatDuration(
            StatisticsViewModel.durationTargetMinutes * 60)
        value: StatisticsViewModel.durationTargetMinutes
        from: StatisticsViewModel.targetLimits.durationMin
        to: StatisticsViewModel.targetLimits.durationMax
        stepSize: StatisticsViewModel.targetLimits.durationStep
        onStepped: value => StatisticsViewModel.durationTargetMinutes = value
    }

    StepperRow {
        label: qsTr("Weekly Target Days")
        valueText: String(StatisticsViewModel.weeklyTargetDays)
        value: StatisticsViewModel.weeklyTargetDays
        from: StatisticsViewModel.targetLimits.weeklyMin
        to: StatisticsViewModel.targetLimits.weeklyMax
        stepSize: StatisticsViewModel.targetLimits.weeklyStep
        onStepped: value => StatisticsViewModel.weeklyTargetDays = value
    }

    StyledLabel {
        Layout.fillWidth: true
        text: qsTr("Goals recalculate historical progress and streaks.")
        textStyle: Theme.footnoteFont
        secondary: true
        wrapMode: Text.WordWrap
    }

    // A labelled kit stepper; the stepper clamps to the view model's limits
    // and the row writes the stepped value back.
    component StepperRow: RowLayout {
        id: stepper

        property string label
        property alias valueText: control.valueText
        property alias value: control.value
        property alias from: control.from
        property alias to: control.to
        property alias stepSize: control.stepSize
        signal stepped(real value)

        Layout.fillWidth: true
        spacing: Theme.rowSpacing

        StyledLabel {
            Layout.fillWidth: true
            text: stepper.label
            elide: Text.ElideRight
        }
        StepperControl {
            id: control

            Accessible.name: stepper.label
            onStepped: value => stepper.stepped(value)
        }
    }
}

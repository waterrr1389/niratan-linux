// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Goal card of the statistics dashboard, following `targetSettingsSection`
// in Features/Bookshelf/StatisticsDashboardView.swift: the target kind picker
// and the character, duration and weekly-day steppers. Each control writes the
// matching StatisticsViewModel setting, which clamps and persists it.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
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
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: qsTr("Goal Type")
            color: Theme.textPrimary
            font.pixelSize: 13
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
        canDecrease: StatisticsViewModel.characterTarget
                     > StatisticsViewModel.targetLimits.characterMin
        canIncrease: StatisticsViewModel.characterTarget
                     < StatisticsViewModel.targetLimits.characterMax
        onDecrease: StatisticsViewModel.characterTarget = StatisticsViewModel.characterTarget
            - StatisticsViewModel.targetLimits.characterStep
        onIncrease: StatisticsViewModel.characterTarget = StatisticsViewModel.characterTarget
            + StatisticsViewModel.targetLimits.characterStep
    }

    StepperRow {
        visible: StatisticsViewModel.targetType === 1
        label: qsTr("Duration Target")
        valueText: card.host.formatDuration(
            StatisticsViewModel.durationTargetMinutes * 60)
        canDecrease: StatisticsViewModel.durationTargetMinutes
                     > StatisticsViewModel.targetLimits.durationMin
        canIncrease: StatisticsViewModel.durationTargetMinutes
                     < StatisticsViewModel.targetLimits.durationMax
        onDecrease: StatisticsViewModel.durationTargetMinutes
            = StatisticsViewModel.durationTargetMinutes
            - StatisticsViewModel.targetLimits.durationStep
        onIncrease: StatisticsViewModel.durationTargetMinutes
            = StatisticsViewModel.durationTargetMinutes
            + StatisticsViewModel.targetLimits.durationStep
    }

    StepperRow {
        label: qsTr("Weekly Target Days")
        valueText: String(StatisticsViewModel.weeklyTargetDays)
        canDecrease: StatisticsViewModel.weeklyTargetDays
                     > StatisticsViewModel.targetLimits.weeklyMin
        canIncrease: StatisticsViewModel.weeklyTargetDays
                     < StatisticsViewModel.targetLimits.weeklyMax
        onDecrease: StatisticsViewModel.weeklyTargetDays = StatisticsViewModel.weeklyTargetDays
            - StatisticsViewModel.targetLimits.weeklyStep
        onIncrease: StatisticsViewModel.weeklyTargetDays = StatisticsViewModel.weeklyTargetDays
            + StatisticsViewModel.targetLimits.weeklyStep
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("Goals recalculate historical progress and streaks.")
        color: Theme.textSecondary
        font.pixelSize: 11
        wrapMode: Text.WordWrap
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
}

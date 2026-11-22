// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/StatisticsSettingsView.swift: body and resetTimeBinding.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

SettingsForm {
    id: form

    objectName: "settings-statistics"
    readonly property list<string> modes: ["Off", "Page Turn", "On"]

    SettingsViewModel {
        id: settings
    }

    component ResetTimePart: SpinBox {
        editable: true
        textFromValue: (value, locale) => value.toLocaleString(locale, "f", 0).padStart(2, "0")
    }

    SettingsNote {
        visible: settings.error.length > 0
        text: settings.error
        color: Theme.popupDanger
    }

    SettingsCard {
        title: qsTr("Statistics")
        footer: qsTr("Statistics appears in Bookshelf when enabled.")

        SettingsToggle {
            label: qsTr("Enable")
            checked: settings.enableStatistics
            onEdited: value => settings.enableStatistics = value
        }
    }

    SettingsCard {
        visible: settings.enableStatistics
        title: qsTr("Autostart")

        SettingRow {
            id: autostartRow
            label: qsTr("Autostart")

            SettingsSegments {
                Accessible.name: autostartRow.label
                labels: [qsTr("Off"), qsTr("Page Turn"), qsTr("On")]
                minimumSegmentWidth: 72
                selectedIndex: form.modes.indexOf(settings.statisticsAutostartMode)
                onEdited: index => settings.statisticsAutostartMode = form.modes[index]
            }
        }
    }

    SettingsCard {
        visible: settings.enableStatistics
        title: qsTr("Daily Reset")
        footer: qsTr("Reading statistics recorded before this time count toward the previous day.")

        SettingRow {
            label: qsTr("Reset Time")

            ResetTimePart {
                Accessible.name: qsTr("Hour")
                to: 23
                value: Math.floor(settings.statisticsResetTime / 60)
                onValueModified: settings.statisticsResetTime = value * 60 + settings.statisticsResetTime % 60
            }

            Label { text: ":" }

            ResetTimePart {
                Accessible.name: qsTr("Minute")
                to: 59
                value: settings.statisticsResetTime % 60
                onValueModified: settings.statisticsResetTime = Math.floor(settings.statisticsResetTime / 60) * 60 + value
            }
        }
    }
}

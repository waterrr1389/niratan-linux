// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/StatisticsSettingsView.swift: body and resetTimeBinding.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

PanelScaffold {
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

    StyledLabel {
        visible: settings.error.length > 0
        text: settings.error
        textStyle: Theme.footnoteFont
        wrapMode: Text.Wrap
        color: Theme.popupDanger
    }

    FormSection {
        title: qsTr("Statistics")
        footer: qsTr("Statistics appears in Bookshelf when enabled.")

        FormToggleRow {
            label: qsTr("Enable")
            checked: settings.enableStatistics
            onToggled: value => settings.enableStatistics = value
        }
    }

    FormSection {
        visible: settings.enableStatistics
        title: qsTr("Autostart")

        FormSegmentedRow {
            label: qsTr("Autostart")
            model: [qsTr("Off"), qsTr("Page Turn"), qsTr("On")]
            currentIndex: form.modes.indexOf(settings.statisticsAutostartMode)
            onActivated: index => settings.statisticsAutostartMode = form.modes[index]
        }
    }

    FormSection {
        visible: settings.enableStatistics
        title: qsTr("Daily Reset")
        footer: qsTr("Reading statistics recorded before this time count toward the previous day.")

        FormRow {
            label: qsTr("Reset Time")

            Row {
                spacing: Theme.spacingXs

                ResetTimePart {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: qsTr("Hour")
                    to: 23
                    value: Math.floor(settings.statisticsResetTime / 60)
                    onValueModified: settings.statisticsResetTime = value * 60 + settings.statisticsResetTime % 60
                }

                StyledLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    text: ":"
                }

                ResetTimePart {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: qsTr("Minute")
                    to: 59
                    value: settings.statisticsResetTime % 60
                    onValueModified: settings.statisticsResetTime = Math.floor(settings.statisticsResetTime / 60) * 60 + value
                }
            }
        }
    }
}

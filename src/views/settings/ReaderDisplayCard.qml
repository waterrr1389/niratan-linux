// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/AppearanceView.swift: the Display card of
// AppearanceView.nativeAppearanceContent. The three statistics rows follow
// Progress Position while the global statistics switch is on, before Show
// Sasayaki Toggle, as in Swift's body.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

SettingsCard {
    id: card

    required property SettingsViewModel settings
    required property SasayakiSettingsViewModel sasayaki

    readonly property list<string> counts: ["Off", "Characters", "Pages"]
    readonly property bool showsProgress: settings.showProgress || settings.showChapterProgress

    title: qsTr("Display")
    footer: qsTr("Reader changes take effect the next time you open a book.")

    SettingsToggle {
        label: qsTr("Show Title")
        checked: card.settings.showTitle
        onEdited: value => card.settings.showTitle = value
    }

    SettingsSeparator {}
    SettingsToggle {
        label: qsTr("Show Progress")
        checked: card.settings.showProgress
        onEdited: value => card.settings.showProgress = value
    }

    SettingsSeparator {}
    SettingsToggle {
        label: qsTr("Show Chapter Progress")
        checked: card.settings.showChapterProgress
        onEdited: value => card.settings.showChapterProgress = value
    }

    SettingsSeparator { visible: card.showsProgress }
    SettingRow {
        id: countRow

        visible: card.showsProgress
        label: qsTr("Count")
        subtitle: card.settings.progressCount === "Pages"
                  ? qsTr("Pages are counted for the current layout and window size, and are unavailable in continuous mode.") : ""

        SettingsSegments {
            Accessible.name: countRow.label
            labels: [qsTr("Off"), qsTr("Characters"), qsTr("Pages")]
            minimumSegmentWidth: 64
            selectedIndex: card.counts.indexOf(card.settings.progressCount)
            onEdited: index => card.settings.progressCount = card.counts[index]
        }
    }

    SettingsSeparator { visible: card.showsProgress }
    SettingsToggle {
        visible: card.showsProgress
        label: qsTr("Show Percentage")
        checked: card.settings.showPercentage
        onEdited: value => card.settings.showPercentage = value
    }

    SettingsSeparator { visible: card.showsProgress }
    SettingRow {
        id: positionRow

        visible: card.showsProgress
        label: qsTr("Progress Position")

        SettingsSegments {
            Accessible.name: positionRow.label
            labels: [qsTr("Top"), qsTr("Bottom")]
            minimumSegmentWidth: 54
            selectedIndex: card.settings.showProgressTop ? 0 : 1
            onEdited: index => card.settings.showProgressTop = index === 0
        }
    }

    SettingsSeparator {
        objectName: "showStatisticsToggleSeparator"
        visible: card.settings.enableStatistics
    }
    SettingsToggle {
        objectName: "showStatisticsToggleRow"
        visible: card.settings.enableStatistics
        label: qsTr("Show Statistics Toggle")
        checked: card.settings.showStatisticsToggle
        onEdited: value => card.settings.showStatisticsToggle = value
    }

    SettingsSeparator {
        objectName: "showReadingSpeedSeparator"
        visible: card.settings.enableStatistics
    }
    SettingsToggle {
        objectName: "showReadingSpeedRow"
        visible: card.settings.enableStatistics
        label: qsTr("Show Reading Speed")
        checked: card.settings.showReadingSpeed
        onEdited: value => card.settings.showReadingSpeed = value
    }

    SettingsSeparator {
        objectName: "showReadingTimeSeparator"
        visible: card.settings.enableStatistics
    }
    SettingsToggle {
        objectName: "showReadingTimeRow"
        visible: card.settings.enableStatistics
        label: qsTr("Show Reading Time")
        checked: card.settings.showReadingTime
        onEdited: value => card.settings.showReadingTime = value
    }

    SettingsSeparator { visible: card.sasayaki.enable }
    SettingsToggle {
        visible: card.sasayaki.enable
        label: qsTr("Show Sasayaki Toggle")
        checked: card.sasayaki.showSasayakiToggle
        onEdited: value => card.sasayaki.showSasayakiToggle = value
    }
}

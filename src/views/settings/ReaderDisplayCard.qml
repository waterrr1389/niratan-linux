// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/AppearanceView.swift: the Display card of
// AppearanceView.nativeAppearanceContent. The three statistics rows follow
// Progress Position while the global statistics switch is on, before Show
// Sasayaki Toggle, as in Swift's body.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

FormSection {
    id: card

    required property SettingsViewModel settings
    required property SasayakiSettingsViewModel sasayaki

    readonly property list<string> counts: ["Off", "Characters", "Pages"]
    readonly property bool showsProgress: settings.showProgress || settings.showChapterProgress

    title: qsTr("Display")
    footer: qsTr("Reader changes take effect the next time you open a book.")

    FormToggleRow {
        label: qsTr("Show Title")
        checked: card.settings.showTitle
        onToggled: value => card.settings.showTitle = value
    }

    FormDivider {}
    FormToggleRow {
        label: qsTr("Show Progress")
        checked: card.settings.showProgress
        onToggled: value => card.settings.showProgress = value
    }

    FormDivider {}
    FormToggleRow {
        label: qsTr("Show Chapter Progress")
        checked: card.settings.showChapterProgress
        onToggled: value => card.settings.showChapterProgress = value
    }

    FormDivider { visible: card.showsProgress }
    FormSegmentedRow {
        visible: card.showsProgress
        label: qsTr("Count")
        subtitle: card.settings.progressCount === "Pages"
                  ? qsTr("Pages are counted for the current layout and window size, and are unavailable in continuous mode.") : ""
        model: [qsTr("Off"), qsTr("Characters"), qsTr("Pages")]
        currentIndex: card.counts.indexOf(card.settings.progressCount)
        onActivated: index => card.settings.progressCount = card.counts[index]
    }

    FormDivider { visible: card.showsProgress }
    FormToggleRow {
        visible: card.showsProgress
        label: qsTr("Show Percentage")
        checked: card.settings.showPercentage
        onToggled: value => card.settings.showPercentage = value
    }

    FormDivider { visible: card.showsProgress }
    FormSegmentedRow {
        visible: card.showsProgress
        label: qsTr("Progress Position")
        model: [qsTr("Top"), qsTr("Bottom")]
        currentIndex: card.settings.showProgressTop ? 0 : 1
        onActivated: index => card.settings.showProgressTop = index === 0
    }

    FormDivider {
        objectName: "showStatisticsToggleSeparator"
        visible: card.settings.enableStatistics
    }
    FormToggleRow {
        objectName: "showStatisticsToggleRow"
        visible: card.settings.enableStatistics
        label: qsTr("Show Statistics Toggle")
        checked: card.settings.showStatisticsToggle
        onToggled: value => card.settings.showStatisticsToggle = value
    }

    FormDivider {
        objectName: "showReadingSpeedSeparator"
        visible: card.settings.enableStatistics
    }
    FormToggleRow {
        objectName: "showReadingSpeedRow"
        visible: card.settings.enableStatistics
        label: qsTr("Show Reading Speed")
        checked: card.settings.showReadingSpeed
        onToggled: value => card.settings.showReadingSpeed = value
    }

    FormDivider {
        objectName: "showReadingTimeSeparator"
        visible: card.settings.enableStatistics
    }
    FormToggleRow {
        objectName: "showReadingTimeRow"
        visible: card.settings.enableStatistics
        label: qsTr("Show Reading Time")
        checked: card.settings.showReadingTime
        onToggled: value => card.settings.showReadingTime = value
    }

    FormDivider { visible: card.sasayaki.enable }
    FormToggleRow {
        visible: card.sasayaki.enable
        label: qsTr("Show Sasayaki Toggle")
        checked: card.sasayaki.showSasayakiToggle
        onToggled: value => card.sasayaki.showSasayakiToggle = value
    }
}

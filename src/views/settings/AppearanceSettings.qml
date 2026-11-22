// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/AppearanceView.swift: AppearanceView.nativeAppearanceContent.
// Deliberate Linux difference: keep the existing Invert Sepia in Dark Mode
// setting; the pinned Swift Theme card does not show this row.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

SettingsForm {
    id: form

    objectName: "settings-appearance"
    readonly property list<string> themes: ["System", "Light", "Dark", "Sepia", "Custom"]
    readonly property list<string> themeLabels: [qsTr("System"), qsTr("Light"), qsTr("Dark"), qsTr("Sepia"), qsTr("Custom")]
    readonly property list<string> furiganaModes: ["Off", "Dimmed", "Toggle", "Hidden"]
    readonly property list<string> counts: ["Off", "Characters", "Pages"]
    readonly property list<string> fonts: Qt.fontFamilies()
    readonly property bool paginated: !settings.continuousMode
    readonly property bool paragraph: paginated && settings.paragraphMode
    readonly property bool spreadsAvailable: paginated && !settings.verticalWriting && !settings.paragraphMode
    readonly property bool showsProgress: settings.showProgress || settings.showChapterProgress

    SettingsViewModel {
        id: settings
    }

    SasayakiSettingsViewModel {
        id: sasayaki
    }

    SettingsNote {
        visible: settings.error.length > 0
        text: settings.error
        color: Theme.popupDanger
    }

    SettingsCard {
        title: qsTr("Theme")

        SettingsSegments {
            Layout.fillWidth: true
            Layout.margins: 12
            Accessible.name: qsTr("Reader Theme")
            labels: form.themeLabels
            minimumSegmentWidth: 44
            selectedIndex: form.themes.indexOf(settings.theme)
            onEdited: index => settings.theme = form.themes[index]
        }

        SettingsSeparator { visible: lightSepia.visible }
        SettingsToggle {
            id: lightSepia
            visible: settings.theme === "System"
            label: qsTr("Use Sepia as Light Theme")
            checked: settings.systemLightSepia
            onEdited: value => settings.systemLightSepia = value
        }

        SettingsSeparator { visible: sepiaInvert.visible }
        SettingsToggle {
            id: sepiaInvert
            visible: settings.theme === "Sepia"
            label: qsTr("Invert Sepia in Dark Mode")
            checked: settings.sepiaInvertInDark
            onEdited: value => settings.sepiaInvertInDark = value
        }

        SettingsSeparator { visible: interfaceRow.visible }
        SettingRow {
            id: interfaceRow
            visible: settings.theme === "Custom"
            label: qsTr("Interface")

            SettingsSegments {
                Accessible.name: interfaceRow.label
                labels: form.themeLabels.slice(0, 3)
                minimumSegmentWidth: 58
                selectedIndex: form.themes.indexOf(settings.uiTheme)
                onEdited: index => settings.uiTheme = form.themes[index]
            }
        }
    }

    SettingsCard {
        title: qsTr("Text")

        SettingRow {
            id: orientationRow
            label: qsTr("Text Orientation")

            SettingsSegments {
                Accessible.name: orientationRow.label
                labels: [qsTr("Vertical"), qsTr("Horizontal")]
                minimumSegmentWidth: 74
                selectedIndex: settings.verticalWriting ? 0 : 1
                onEdited: index => settings.verticalWriting = index === 0
            }
        }

        SettingsSeparator {}
        SettingRow {
            id: fontRow
            label: qsTr("Font")

            ComboBox {
                Accessible.name: fontRow.label
                Layout.preferredWidth: 200
                model: form.fonts.includes(settings.selectedFont) ? form.fonts : [settings.selectedFont].concat(form.fonts)
                displayText: settings.selectedFont
                currentIndex: Math.max(0, form.fonts.indexOf(settings.selectedFont))
                onActivated: index => settings.selectedFont = textAt(index)
            }
        }

        SettingsSeparator {}
        SettingsInteger {
            label: qsTr("Font Size")
            from: 16
            to: 40
            value: settings.fontSize
            onEdited: value => settings.fontSize = value
        }

        SettingsSeparator {}
        SettingRow {
            id: furiganaRow
            label: qsTr("Furigana")
            stacked: form.cardWidth < labelWidth + furiganaPicker.implicitWidth + 32 + Theme.spacingMd

            SettingsSegments {
                id: furiganaPicker

                Accessible.name: furiganaRow.label
                Layout.fillWidth: furiganaRow.stacked
                labels: [qsTr("Show"), qsTr("Dimmed"), qsTr("Tap to Reveal"), qsTr("Hidden")]
                minimumSegmentWidth: 64
                selectedIndex: form.furiganaModes.indexOf(settings.furiganaMode)
                onEdited: index => settings.furiganaMode = form.furiganaModes[index]
            }
        }
    }

    SettingsCard {
        title: qsTr("Layout")

        SettingRow {
            id: modeRow
            label: qsTr("Mode")

            SettingsSegments {
                Accessible.name: modeRow.label
                labels: [qsTr("Paginated"), qsTr("Continuous")]
                minimumSegmentWidth: 78
                selectedIndex: settings.continuousMode ? 1 : 0
                onEdited: index => settings.continuousMode = index === 1
            }
        }

        SettingsSeparator { visible: form.paginated }
        SettingsToggle {
            visible: form.paginated
            label: qsTr("Paragraph Mode")
            checked: settings.paragraphMode
            onEdited: value => settings.paragraphMode = value
        }

        SettingsSeparator { visible: form.paragraph }
        SettingsToggle {
            visible: form.paragraph
            label: qsTr("Text Animation")
            checked: settings.textAnimation
            onEdited: value => settings.textAnimation = value
        }

        SettingsSeparator { visible: speed.visible }
        SettingsSliderRow {
            id: speed
            visible: form.paragraph && settings.textAnimation
            label: qsTr("Text Speed")
            from: 25
            to: 100
            stepSize: 5
            decimals: 0
            suffix: qsTr("/s")
            value: settings.textSpeed
            onEdited: value => settings.textSpeed = value
        }

        SettingsSeparator { visible: form.spreadsAvailable }
        SettingsToggle {
            visible: form.spreadsAvailable
            label: qsTr("Two-Column Horizontal Pages")
            checked: settings.twoColumnHorizontalPages
            onEdited: value => settings.twoColumnHorizontalPages = value
        }

        SettingsSeparator {}
        SettingsInteger {
            label: qsTr("Horizontal Padding")
            from: 0
            to: 80
            suffix: "%"
            value: settings.horizontalPadding
            onEdited: value => settings.horizontalPadding = value
        }

        SettingsSeparator {}
        SettingsInteger {
            label: qsTr("Vertical Padding")
            from: 0
            to: 50
            suffix: "%"
            value: settings.verticalPadding
            onEdited: value => settings.verticalPadding = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Avoid Page Break")
            checked: settings.avoidPageBreak
            onEdited: value => settings.avoidPageBreak = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Justify Text")
            checked: settings.justifyText
            onEdited: value => settings.justifyText = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Blur Images")
            checked: settings.blurImages
            onEdited: value => settings.blurImages = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Advanced")
            checked: settings.layoutAdvanced
            onEdited: value => settings.layoutAdvanced = value
        }

        SettingsSeparator { visible: settings.layoutAdvanced }
        SettingsSliderRow {
            visible: settings.layoutAdvanced
            label: qsTr("Line Height")
            from: 1
            to: 2.5
            stepSize: 0.05
            decimals: 2
            value: settings.lineHeight
            onEdited: value => settings.lineHeight = value
        }

        SettingsSeparator { visible: settings.layoutAdvanced }
        SettingsSliderRow {
            visible: settings.layoutAdvanced
            label: qsTr("Character Spacing")
            from: -10
            to: 10
            stepSize: 1
            decimals: 0
            suffix: "%"
            value: settings.characterSpacing
            onEdited: value => settings.characterSpacing = value
        }

        SettingsSeparator { visible: settings.layoutAdvanced }
        SettingsSliderRow {
            visible: settings.layoutAdvanced
            label: qsTr("Paragraph Spacing")
            from: 0
            to: 3
            stepSize: 0.1
            decimals: 1
            suffix: qsTr("em")
            value: settings.paragraphSpacing
            onEdited: value => settings.paragraphSpacing = value
        }
    }

    SettingsCard {
        title: qsTr("Display")
        footer: qsTr("Reader changes take effect the next time you open a book.")

        SettingsToggle {
            label: qsTr("Show Title")
            checked: settings.showTitle
            onEdited: value => settings.showTitle = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Show Progress")
            checked: settings.showProgress
            onEdited: value => settings.showProgress = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Show Chapter Progress")
            checked: settings.showChapterProgress
            onEdited: value => settings.showChapterProgress = value
        }

        SettingsSeparator { visible: form.showsProgress }
        SettingRow {
            id: countRow
            visible: form.showsProgress
            label: qsTr("Count")
            subtitle: settings.progressCount === "Pages"
                      ? qsTr("Pages are counted for the current layout and window size, and are unavailable in continuous mode.") : ""

            SettingsSegments {
                Accessible.name: countRow.label
                labels: [qsTr("Off"), qsTr("Characters"), qsTr("Pages")]
                minimumSegmentWidth: 64
                selectedIndex: form.counts.indexOf(settings.progressCount)
                onEdited: index => settings.progressCount = form.counts[index]
            }
        }

        SettingsSeparator { visible: form.showsProgress }
        SettingsToggle {
            visible: form.showsProgress
            label: qsTr("Show Percentage")
            checked: settings.showPercentage
            onEdited: value => settings.showPercentage = value
        }

        SettingsSeparator { visible: form.showsProgress }
        SettingRow {
            id: positionRow
            visible: form.showsProgress
            label: qsTr("Progress Position")

            SettingsSegments {
                Accessible.name: positionRow.label
                labels: [qsTr("Top"), qsTr("Bottom")]
                minimumSegmentWidth: 54
                selectedIndex: settings.showProgressTop ? 0 : 1
                onEdited: index => settings.showProgressTop = index === 0
            }
        }

        SettingsSeparator { visible: sasayaki.enable }
        SettingsToggle {
            visible: sasayaki.enable
            label: qsTr("Show Sasayaki Toggle")
            checked: sasayaki.showSasayakiToggle
            onEdited: value => sasayaki.showSasayakiToggle = value
        }
    }
}

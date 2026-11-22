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
    readonly property list<string> fonts: Qt.fontFamilies()
    readonly property bool paginated: !settingsModel.continuousMode
    readonly property bool paragraph: paginated && settingsModel.paragraphMode
    readonly property bool spreadsAvailable: paginated && !settingsModel.verticalWriting && !settingsModel.paragraphMode

    SettingsViewModel {
        id: settingsModel
    }

    SasayakiSettingsViewModel {
        id: sasayakiModel
    }

    SettingsNote {
        visible: settingsModel.error.length > 0
        text: settingsModel.error
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
            selectedIndex: form.themes.indexOf(settingsModel.theme)
            onEdited: index => settingsModel.theme = form.themes[index]
        }

        SettingsSeparator { visible: lightSepia.visible }
        SettingsToggle {
            id: lightSepia
            visible: settingsModel.theme === "System"
            label: qsTr("Use Sepia as Light Theme")
            checked: settingsModel.systemLightSepia
            onEdited: value => settingsModel.systemLightSepia = value
        }

        SettingsSeparator { visible: sepiaInvert.visible }
        SettingsToggle {
            id: sepiaInvert
            visible: settingsModel.theme === "Sepia"
            label: qsTr("Invert Sepia in Dark Mode")
            checked: settingsModel.sepiaInvertInDark
            onEdited: value => settingsModel.sepiaInvertInDark = value
        }

        SettingsSeparator { visible: interfaceRow.visible }
        SettingRow {
            id: interfaceRow
            visible: settingsModel.theme === "Custom"
            label: qsTr("Interface")

            SettingsSegments {
                Accessible.name: interfaceRow.label
                labels: form.themeLabels.slice(0, 3)
                minimumSegmentWidth: 58
                selectedIndex: form.themes.indexOf(settingsModel.uiTheme)
                onEdited: index => settingsModel.uiTheme = form.themes[index]
            }
        }

        SettingsSeparator {
            objectName: "customBackgroundColorSeparator"
            visible: settingsModel.theme === "Custom"
        }
        SettingsColorRow {
            objectName: "customBackgroundColorRow"
            visible: settingsModel.theme === "Custom"
            label: qsTr("Background Color")
            value: settingsModel.customBackgroundColor
            onEdited: value => settingsModel.customBackgroundColor = value
        }

        SettingsSeparator {
            objectName: "customTextColorSeparator"
            visible: settingsModel.theme === "Custom"
        }
        SettingsColorRow {
            objectName: "customTextColorRow"
            visible: settingsModel.theme === "Custom"
            label: qsTr("Text Color")
            value: settingsModel.customTextColor
            onEdited: value => settingsModel.customTextColor = value
        }

        SettingsSeparator {
            objectName: "customInfoColorSeparator"
            visible: settingsModel.theme === "Custom"
        }
        SettingsColorRow {
            objectName: "customInfoColorRow"
            visible: settingsModel.theme === "Custom"
            label: qsTr("Info Color")
            value: settingsModel.customInfoColor
            onEdited: value => settingsModel.customInfoColor = value
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
                selectedIndex: settingsModel.verticalWriting ? 0 : 1
                onEdited: index => settingsModel.verticalWriting = index === 0
            }
        }

        SettingsSeparator {}
        SettingRow {
            id: fontRow
            label: qsTr("Font")

            ComboBox {
                Accessible.name: fontRow.label
                Layout.preferredWidth: 200
                model: form.fonts.includes(settingsModel.selectedFont) ? form.fonts : [settingsModel.selectedFont].concat(form.fonts)
                displayText: settingsModel.selectedFont
                currentIndex: Math.max(0, form.fonts.indexOf(settingsModel.selectedFont))
                onActivated: index => settingsModel.selectedFont = textAt(index)
            }
        }

        SettingsSeparator {}
        SettingsInteger {
            label: qsTr("Font Size")
            from: 16
            to: 40
            value: settingsModel.fontSize
            onEdited: value => settingsModel.fontSize = value
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
                selectedIndex: form.furiganaModes.indexOf(settingsModel.furiganaMode)
                onEdited: index => settingsModel.furiganaMode = form.furiganaModes[index]
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
                selectedIndex: settingsModel.continuousMode ? 1 : 0
                onEdited: index => settingsModel.continuousMode = index === 1
            }
        }

        SettingsSeparator { visible: form.paginated }
        SettingsToggle {
            visible: form.paginated
            label: qsTr("Paragraph Mode")
            checked: settingsModel.paragraphMode
            onEdited: value => settingsModel.paragraphMode = value
        }

        SettingsSeparator { visible: form.paragraph }
        SettingsToggle {
            visible: form.paragraph
            label: qsTr("Text Animation")
            checked: settingsModel.textAnimation
            onEdited: value => settingsModel.textAnimation = value
        }

        SettingsSeparator { visible: speed.visible }
        SettingsSliderRow {
            id: speed
            visible: form.paragraph && settingsModel.textAnimation
            label: qsTr("Text Speed")
            from: 25
            to: 100
            stepSize: 5
            decimals: 0
            suffix: qsTr("/s")
            value: settingsModel.textSpeed
            onEdited: value => settingsModel.textSpeed = value
        }

        SettingsSeparator { visible: form.spreadsAvailable }
        SettingsToggle {
            visible: form.spreadsAvailable
            label: qsTr("Two-Column Horizontal Pages")
            checked: settingsModel.twoColumnHorizontalPages
            onEdited: value => settingsModel.twoColumnHorizontalPages = value
        }

        SettingsSeparator {}
        SettingsToggle {
            objectName: "wheelPageTurnRow"
            label: qsTr("Mouse Wheel Page Turn")
            checked: settingsModel.wheelPageTurnEnabled
            onEdited: value => settingsModel.wheelPageTurnEnabled = value
        }

        SettingsSeparator {}
        SettingsInteger {
            label: qsTr("Horizontal Padding")
            from: 0
            to: 80
            suffix: "%"
            value: settingsModel.horizontalPadding
            onEdited: value => settingsModel.horizontalPadding = value
        }

        SettingsSeparator {}
        SettingsInteger {
            label: qsTr("Vertical Padding")
            from: 0
            to: 50
            suffix: "%"
            value: settingsModel.verticalPadding
            onEdited: value => settingsModel.verticalPadding = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Avoid Page Break")
            checked: settingsModel.avoidPageBreak
            onEdited: value => settingsModel.avoidPageBreak = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Justify Text")
            checked: settingsModel.justifyText
            onEdited: value => settingsModel.justifyText = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Blur Images")
            checked: settingsModel.blurImages
            onEdited: value => settingsModel.blurImages = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Advanced")
            checked: settingsModel.layoutAdvanced
            onEdited: value => settingsModel.layoutAdvanced = value
        }

        SettingsSeparator { visible: settingsModel.layoutAdvanced }
        SettingsSliderRow {
            visible: settingsModel.layoutAdvanced
            label: qsTr("Line Height")
            from: 1
            to: 2.5
            stepSize: 0.05
            decimals: 2
            value: settingsModel.lineHeight
            onEdited: value => settingsModel.lineHeight = value
        }

        SettingsSeparator { visible: settingsModel.layoutAdvanced }
        SettingsSliderRow {
            visible: settingsModel.layoutAdvanced
            label: qsTr("Character Spacing")
            from: -10
            to: 10
            stepSize: 1
            decimals: 0
            suffix: "%"
            value: settingsModel.characterSpacing
            onEdited: value => settingsModel.characterSpacing = value
        }

        SettingsSeparator { visible: settingsModel.layoutAdvanced }
        SettingsSliderRow {
            visible: settingsModel.layoutAdvanced
            label: qsTr("Paragraph Spacing")
            from: 0
            to: 3
            stepSize: 0.1
            decimals: 1
            suffix: qsTr("em")
            value: settingsModel.paragraphSpacing
            onEdited: value => settingsModel.paragraphSpacing = value
        }
    }

    ReaderDisplayCard {
        settings: settingsModel
        sasayaki: sasayakiModel
    }

    SettingsCard {
        objectName: "settings-popupCard"
        title: qsTr("Popup")

        SettingsSliderRow {
            objectName: "popupWidthRow"
            label: qsTr("Width")
            from: 100
            to: 1400
            stepSize: 10
            decimals: 0
            value: settingsModel.popupWidth
            onEdited: value => settingsModel.popupWidth = value
        }

        SettingsSeparator { objectName: "popupSeparator" }
        SettingsSliderRow {
            objectName: "popupHeightRow"
            label: qsTr("Height")
            from: 100
            to: 800
            stepSize: 10
            decimals: 0
            value: settingsModel.popupHeight
            onEdited: value => settingsModel.popupHeight = value
        }

        SettingsSeparator { objectName: "popupSeparator" }
        SettingsSliderRow {
            objectName: "popupScaleRow"
            label: qsTr("Scale")
            from: 0.8
            to: 1.5
            stepSize: 0.05
            decimals: 2
            value: settingsModel.popupScale
            onEdited: value => settingsModel.popupScale = value
        }

        SettingsSeparator { objectName: "popupSeparator" }
        SettingsToggle {
            objectName: "popupActionBarRow"
            label: qsTr("Show Action Bar")
            checked: settingsModel.popupActionBar
            onEdited: value => settingsModel.popupActionBar = value
        }

        SettingsSeparator { objectName: "popupSeparator" }
        SettingsToggle {
            objectName: "popupDisableTransparencyRow"
            label: qsTr("Disable Transparency")
            checked: settingsModel.popupDisableTransparency
            onEdited: value => settingsModel.popupDisableTransparency = value
        }

        SettingsSeparator { objectName: "popupSeparator" }
        SettingsToggle {
            objectName: "popupFullWidthRow"
            label: qsTr("Full-width")
            checked: settingsModel.popupFullWidth
            onEdited: value => settingsModel.popupFullWidth = value
        }

        SettingsSeparator { objectName: "popupSeparator" }
        SettingsToggle {
            objectName: "popupSwipeToDismissRow"
            label: qsTr("Swipe to Dismiss")
            checked: settingsModel.popupSwipeToDismiss
            onEdited: value => settingsModel.popupSwipeToDismiss = value
        }

        SettingsSeparator {
            objectName: "popupSwipeThresholdSeparator"
            visible: settingsModel.popupSwipeToDismiss
        }
        SettingsSliderRow {
            objectName: "popupSwipeThresholdRow"
            visible: settingsModel.popupSwipeToDismiss
            label: qsTr("Swipe Threshold")
            from: 20
            to: 80
            stepSize: 5
            decimals: 0
            value: settingsModel.popupSwipeThreshold
            onEdited: value => settingsModel.popupSwipeThreshold = value
        }
    }
}

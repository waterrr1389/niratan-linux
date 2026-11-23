// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/AppearanceView.swift: AppearanceView.nativeAppearanceContent.
// Deliberate Linux difference: keep the existing Invert Sepia in Dark Mode
// setting; the pinned Swift Theme card does not show this row.
//
// Built from the control kit. Sliders show their value with the decimals and
// suffix Swift's NativeSettingsSliderRow formats; integer settings are
// steppers. Furigana's four segments move below their label when the form
// is too narrow to hold them on one line (the reader's side panel).

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

PanelScaffold {
    id: form

    objectName: "settings-appearance"
    readonly property list<string> themes: ["System", "Light", "Dark", "Sepia", "Custom"]
    readonly property list<string> themeLabels: [qsTr("System"), qsTr("Light"), qsTr("Dark"), qsTr("Sepia"), qsTr("Custom")]
    readonly property list<string> furiganaModes: ["Off", "Dimmed", "Toggle", "Hidden"]
    readonly property list<string> furiganaLabels: [qsTr("Show"), qsTr("Dimmed"), qsTr("Tap to Reveal"), qsTr("Hidden")]
    readonly property list<string> fonts: Qt.fontFamilies()
    readonly property bool paginated: !settingsModel.continuousMode
    readonly property bool paragraph: paginated && settingsModel.paragraphMode
    readonly property bool spreadsAvailable: paginated && !settingsModel.verticalWriting && !settingsModel.paragraphMode
    readonly property bool narrow: column.width < Theme.appearanceWideFormWidth

    function formatted(value, decimals, suffix) {
        return Number(value).toLocaleString(Qt.locale(), "f", decimals) + suffix
    }

    component ValueSlider: FormSliderRow {
        property int decimals: 0
        property string suffix

        valueText: form.formatted(value, decimals, suffix)
    }

    component ValueStepper: FormStepperRow {
        property string suffix

        stepSize: 1
        valueText: form.formatted(value, 0, suffix)
    }

    SettingsViewModel {
        id: settingsModel
    }

    SasayakiSettingsViewModel {
        id: sasayakiModel
    }

    StyledLabel {
        visible: settingsModel.error.length > 0
        text: settingsModel.error
        color: Theme.popupDanger
        wrapMode: Text.Wrap
    }

    FormSection {
        title: qsTr("Theme")

        FormSegmentedRow {
            fillsWidth: true
            model: form.themeLabels
            currentIndex: form.themes.indexOf(settingsModel.theme)
            onActivated: index => settingsModel.theme = form.themes[index]
        }

        FormDivider { visible: lightSepia.visible }
        FormToggleRow {
            id: lightSepia
            visible: settingsModel.theme === "System"
            label: qsTr("Use Sepia as Light Theme")
            checked: settingsModel.systemLightSepia
            onToggled: value => settingsModel.systemLightSepia = value
        }

        FormDivider { visible: sepiaInvert.visible }
        FormToggleRow {
            id: sepiaInvert
            visible: settingsModel.theme === "Sepia"
            label: qsTr("Invert Sepia in Dark Mode")
            checked: settingsModel.sepiaInvertInDark
            onToggled: value => settingsModel.sepiaInvertInDark = value
        }

        FormDivider { visible: interfaceRow.visible }
        FormSegmentedRow {
            id: interfaceRow
            visible: settingsModel.theme === "Custom"
            label: qsTr("Interface")
            model: form.themeLabels.slice(0, 3)
            currentIndex: form.themes.indexOf(settingsModel.uiTheme)
            onActivated: index => settingsModel.uiTheme = form.themes[index]
        }

        FormDivider {
            objectName: "customBackgroundColorSeparator"
            visible: settingsModel.theme === "Custom"
        }
        FormColorRow {
            objectName: "customBackgroundColorRow"
            controlName: "customBackgroundColorRowSwatch"
            visible: settingsModel.theme === "Custom"
            label: qsTr("Background Color")
            color: settingsModel.customBackgroundColor
            onPicked: value => settingsModel.customBackgroundColor = value
        }

        FormDivider {
            objectName: "customTextColorSeparator"
            visible: settingsModel.theme === "Custom"
        }
        FormColorRow {
            objectName: "customTextColorRow"
            controlName: "customTextColorRowSwatch"
            visible: settingsModel.theme === "Custom"
            label: qsTr("Text Color")
            color: settingsModel.customTextColor
            onPicked: value => settingsModel.customTextColor = value
        }

        FormDivider {
            objectName: "customInfoColorSeparator"
            visible: settingsModel.theme === "Custom"
        }
        FormColorRow {
            objectName: "customInfoColorRow"
            controlName: "customInfoColorRowSwatch"
            visible: settingsModel.theme === "Custom"
            label: qsTr("Info Color")
            color: settingsModel.customInfoColor
            onPicked: value => settingsModel.customInfoColor = value
        }
    }

    FormSection {
        title: qsTr("Text")

        FormSegmentedRow {
            label: qsTr("Text Orientation")
            model: [qsTr("Vertical"), qsTr("Horizontal")]
            currentIndex: settingsModel.verticalWriting ? 0 : 1
            onActivated: index => settingsModel.verticalWriting = index === 0
        }

        FormDivider {}
        FormPickerRow {
            label: qsTr("Font")
            model: form.fonts.includes(settingsModel.selectedFont)
                   ? form.fonts : [settingsModel.selectedFont].concat(form.fonts)
            currentIndex: Math.max(0, model.indexOf(settingsModel.selectedFont))
            onActivated: index => settingsModel.selectedFont = model[index]
        }

        FormDivider {}
        ValueStepper {
            label: qsTr("Font Size")
            from: 16
            to: 40
            value: settingsModel.fontSize
            onStepped: value => settingsModel.fontSize = value
        }

        FormDivider {}
        FormSegmentedRow {
            visible: !form.narrow
            label: qsTr("Furigana")
            model: form.furiganaLabels
            currentIndex: form.furiganaModes.indexOf(settingsModel.furiganaMode)
            onActivated: index => settingsModel.furiganaMode = form.furiganaModes[index]
        }
        FormRow {
            visible: form.narrow
            label: qsTr("Furigana")
        }
        FormSegmentedRow {
            visible: form.narrow
            fillsWidth: true
            model: form.furiganaLabels
            currentIndex: form.furiganaModes.indexOf(settingsModel.furiganaMode)
            onActivated: index => settingsModel.furiganaMode = form.furiganaModes[index]
        }
    }

    FormSection {
        title: qsTr("Layout")

        FormSegmentedRow {
            label: qsTr("Mode")
            model: [qsTr("Paginated"), qsTr("Continuous")]
            currentIndex: settingsModel.continuousMode ? 1 : 0
            onActivated: index => settingsModel.continuousMode = index === 1
        }

        FormDivider { visible: form.paginated }
        FormToggleRow {
            visible: form.paginated
            label: qsTr("Paragraph Mode")
            checked: settingsModel.paragraphMode
            onToggled: value => settingsModel.paragraphMode = value
        }

        FormDivider { visible: form.paragraph }
        FormToggleRow {
            visible: form.paragraph
            label: qsTr("Text Animation")
            checked: settingsModel.textAnimation
            onToggled: value => settingsModel.textAnimation = value
        }

        FormDivider { visible: speed.visible }
        ValueSlider {
            id: speed
            visible: form.paragraph && settingsModel.textAnimation
            label: qsTr("Text Speed")
            from: 25
            to: 100
            stepSize: 5
            suffix: qsTr("/s")
            value: settingsModel.textSpeed
            onMoved: value => settingsModel.textSpeed = value
        }

        FormDivider { visible: form.spreadsAvailable }
        FormToggleRow {
            visible: form.spreadsAvailable
            label: qsTr("Two-Column Horizontal Pages")
            checked: settingsModel.twoColumnHorizontalPages
            onToggled: value => settingsModel.twoColumnHorizontalPages = value
        }

        FormDivider {}
        FormToggleRow {
            objectName: "wheelPageTurnRow"
            label: qsTr("Mouse Wheel Page Turn")
            checked: settingsModel.wheelPageTurnEnabled
            onToggled: value => settingsModel.wheelPageTurnEnabled = value
        }

        FormDivider {}
        ValueStepper {
            label: qsTr("Horizontal Padding")
            from: 0
            to: 80
            suffix: "%"
            value: settingsModel.horizontalPadding
            onStepped: value => settingsModel.horizontalPadding = value
        }

        FormDivider {}
        ValueStepper {
            label: qsTr("Vertical Padding")
            from: 0
            to: 50
            suffix: "%"
            value: settingsModel.verticalPadding
            onStepped: value => settingsModel.verticalPadding = value
        }

        FormDivider {}
        FormToggleRow {
            label: qsTr("Avoid Page Break")
            checked: settingsModel.avoidPageBreak
            onToggled: value => settingsModel.avoidPageBreak = value
        }

        FormDivider {}
        FormToggleRow {
            label: qsTr("Justify Text")
            checked: settingsModel.justifyText
            onToggled: value => settingsModel.justifyText = value
        }

        FormDivider {}
        FormToggleRow {
            label: qsTr("Blur Images")
            checked: settingsModel.blurImages
            onToggled: value => settingsModel.blurImages = value
        }

        FormDivider {}
        FormToggleRow {
            label: qsTr("Advanced")
            checked: settingsModel.layoutAdvanced
            onToggled: value => settingsModel.layoutAdvanced = value
        }

        FormDivider { visible: settingsModel.layoutAdvanced }
        ValueSlider {
            visible: settingsModel.layoutAdvanced
            label: qsTr("Line Height")
            from: 1
            to: 2.5
            stepSize: 0.05
            decimals: 2
            value: settingsModel.lineHeight
            onMoved: value => settingsModel.lineHeight = value
        }

        FormDivider { visible: settingsModel.layoutAdvanced }
        ValueSlider {
            visible: settingsModel.layoutAdvanced
            label: qsTr("Character Spacing")
            from: -10
            to: 10
            stepSize: 1
            suffix: "%"
            value: settingsModel.characterSpacing
            onMoved: value => settingsModel.characterSpacing = value
        }

        FormDivider { visible: settingsModel.layoutAdvanced }
        ValueSlider {
            visible: settingsModel.layoutAdvanced
            label: qsTr("Paragraph Spacing")
            from: 0
            to: 3
            stepSize: 0.1
            decimals: 1
            suffix: qsTr("em")
            value: settingsModel.paragraphSpacing
            onMoved: value => settingsModel.paragraphSpacing = value
        }
    }

    ReaderDisplayCard {
        settings: settingsModel
        sasayaki: sasayakiModel
    }

    FormSection {
        objectName: "settings-popupCard"
        title: qsTr("Popup")

        ValueSlider {
            objectName: "popupWidthRow"
            label: qsTr("Width")
            from: 100
            to: 1400
            stepSize: 10
            value: settingsModel.popupWidth
            onMoved: value => settingsModel.popupWidth = value
        }

        FormDivider { objectName: "popupSeparator" }
        ValueSlider {
            objectName: "popupHeightRow"
            label: qsTr("Height")
            from: 100
            to: 800
            stepSize: 10
            value: settingsModel.popupHeight
            onMoved: value => settingsModel.popupHeight = value
        }

        FormDivider { objectName: "popupSeparator" }
        ValueSlider {
            objectName: "popupScaleRow"
            label: qsTr("Scale")
            from: 0.8
            to: 1.5
            stepSize: 0.05
            decimals: 2
            value: settingsModel.popupScale
            onMoved: value => settingsModel.popupScale = value
        }

        FormDivider { objectName: "popupSeparator" }
        FormToggleRow {
            objectName: "popupActionBarRow"
            label: qsTr("Show Action Bar")
            checked: settingsModel.popupActionBar
            onToggled: value => settingsModel.popupActionBar = value
        }

        FormDivider { objectName: "popupSeparator" }
        FormToggleRow {
            objectName: "popupDisableTransparencyRow"
            label: qsTr("Disable Transparency")
            checked: settingsModel.popupDisableTransparency
            onToggled: value => settingsModel.popupDisableTransparency = value
        }

        FormDivider { objectName: "popupSeparator" }
        FormToggleRow {
            objectName: "popupFullWidthRow"
            label: qsTr("Full-width")
            checked: settingsModel.popupFullWidth
            onToggled: value => settingsModel.popupFullWidth = value
        }

        FormDivider { objectName: "popupSeparator" }
        FormToggleRow {
            objectName: "popupSwipeToDismissRow"
            label: qsTr("Swipe to Dismiss")
            checked: settingsModel.popupSwipeToDismiss
            onToggled: value => settingsModel.popupSwipeToDismiss = value
        }

        FormDivider {
            objectName: "popupSwipeThresholdSeparator"
            visible: settingsModel.popupSwipeToDismiss
        }
        ValueSlider {
            objectName: "popupSwipeThresholdRow"
            visible: settingsModel.popupSwipeToDismiss
            label: qsTr("Swipe Threshold")
            from: 20
            to: 80
            stepSize: 5
            value: settingsModel.popupSwipeThreshold
            onMoved: value => settingsModel.popupSwipeThreshold = value
        }
    }
}

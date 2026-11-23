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
    property SettingsViewModel settingsModel: SettingsViewModel {}
    readonly property list<string> themes: ["System", "Light", "Dark", "Sepia", "Custom"]
    readonly property list<string> themeLabels: [qsTr("System"), qsTr("Light"), qsTr("Dark"), qsTr("Sepia"), qsTr("Custom")]
    readonly property list<string> furiganaModes: ["Off", "Dimmed", "Toggle", "Hidden"]
    readonly property list<string> furiganaLabels: [qsTr("Show"), qsTr("Dimmed"), qsTr("Tap to Reveal"), qsTr("Hidden")]
    readonly property list<string> fonts: Qt.fontFamilies()
    readonly property bool paginated: !form.settingsModel.continuousMode
    readonly property bool paragraph: paginated && form.settingsModel.paragraphMode
    readonly property bool spreadsAvailable: paginated && !form.settingsModel.verticalWriting && !form.settingsModel.paragraphMode
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

    SasayakiSettingsViewModel {
        id: sasayakiModel
    }

    StyledLabel {
        visible: form.settingsModel.error.length > 0
        text: form.settingsModel.error
        color: Theme.popupDanger
        wrapMode: Text.Wrap
    }

    FormSection {
        title: qsTr("Theme")

        FormSegmentedRow {
            fillsWidth: true
            model: form.themeLabels
            currentIndex: form.themes.indexOf(form.settingsModel.theme)
            onActivated: index => form.settingsModel.theme = form.themes[index]
        }

        FormDivider { visible: lightSepia.visible }
        FormToggleRow {
            id: lightSepia
            visible: form.settingsModel.theme === "System"
            label: qsTr("Use Sepia as Light Theme")
            checked: form.settingsModel.systemLightSepia
            onToggled: value => form.settingsModel.systemLightSepia = value
        }

        FormDivider { visible: sepiaInvert.visible }
        FormToggleRow {
            id: sepiaInvert
            visible: form.settingsModel.theme === "Sepia"
            label: qsTr("Invert Sepia in Dark Mode")
            checked: form.settingsModel.sepiaInvertInDark
            onToggled: value => form.settingsModel.sepiaInvertInDark = value
        }

        FormDivider { visible: interfaceRow.visible }
        FormSegmentedRow {
            id: interfaceRow
            visible: form.settingsModel.theme === "Custom"
            label: qsTr("Interface")
            model: form.themeLabels.slice(0, 3)
            currentIndex: form.themes.indexOf(form.settingsModel.uiTheme)
            onActivated: index => form.settingsModel.uiTheme = form.themes[index]
        }

        FormDivider {
            objectName: "customBackgroundColorSeparator"
            visible: form.settingsModel.theme === "Custom"
        }
        FormColorRow {
            objectName: "customBackgroundColorRow"
            controlName: "customBackgroundColorRowSwatch"
            visible: form.settingsModel.theme === "Custom"
            label: qsTr("Background Color")
            color: form.settingsModel.customBackgroundColor
            onPicked: value => form.settingsModel.customBackgroundColor = value
        }

        FormDivider {
            objectName: "customTextColorSeparator"
            visible: form.settingsModel.theme === "Custom"
        }
        FormColorRow {
            objectName: "customTextColorRow"
            controlName: "customTextColorRowSwatch"
            visible: form.settingsModel.theme === "Custom"
            label: qsTr("Text Color")
            color: form.settingsModel.customTextColor
            onPicked: value => form.settingsModel.customTextColor = value
        }

        FormDivider {
            objectName: "customInfoColorSeparator"
            visible: form.settingsModel.theme === "Custom"
        }
        FormColorRow {
            objectName: "customInfoColorRow"
            controlName: "customInfoColorRowSwatch"
            visible: form.settingsModel.theme === "Custom"
            label: qsTr("Info Color")
            color: form.settingsModel.customInfoColor
            onPicked: value => form.settingsModel.customInfoColor = value
        }
    }

    FormSection {
        title: qsTr("Text")

        FormSegmentedRow {
            label: qsTr("Text Orientation")
            model: [qsTr("Vertical"), qsTr("Horizontal")]
            currentIndex: form.settingsModel.verticalWriting ? 0 : 1
            onActivated: index => form.settingsModel.verticalWriting = index === 0
        }

        FormDivider {}
        FormPickerRow {
            label: qsTr("Font")
            model: form.fonts.includes(form.settingsModel.selectedFont)
                   ? form.fonts : [form.settingsModel.selectedFont].concat(form.fonts)
            currentIndex: Math.max(0, model.indexOf(form.settingsModel.selectedFont))
            onActivated: index => form.settingsModel.selectedFont = model[index]
        }

        FormDivider {}
        ValueStepper {
            label: qsTr("Font Size")
            from: 16
            to: 40
            value: form.settingsModel.fontSize
            onStepped: value => form.settingsModel.fontSize = value
        }

        FormDivider {}
        FormSegmentedRow {
            visible: !form.narrow
            label: qsTr("Furigana")
            model: form.furiganaLabels
            currentIndex: form.furiganaModes.indexOf(form.settingsModel.furiganaMode)
            onActivated: index => form.settingsModel.furiganaMode = form.furiganaModes[index]
        }
        FormRow {
            visible: form.narrow
            label: qsTr("Furigana")
        }
        FormSegmentedRow {
            visible: form.narrow
            fillsWidth: true
            model: form.furiganaLabels
            currentIndex: form.furiganaModes.indexOf(form.settingsModel.furiganaMode)
            onActivated: index => form.settingsModel.furiganaMode = form.furiganaModes[index]
        }
    }

    FormSection {
        title: qsTr("Layout")

        FormSegmentedRow {
            label: qsTr("Mode")
            model: [qsTr("Paginated"), qsTr("Continuous")]
            currentIndex: form.settingsModel.continuousMode ? 1 : 0
            onActivated: index => form.settingsModel.continuousMode = index === 1
        }

        FormDivider { visible: form.paginated }
        FormToggleRow {
            visible: form.paginated
            label: qsTr("Paragraph Mode")
            checked: form.settingsModel.paragraphMode
            onToggled: value => form.settingsModel.paragraphMode = value
        }

        FormDivider { visible: form.paragraph }
        FormToggleRow {
            visible: form.paragraph
            label: qsTr("Text Animation")
            checked: form.settingsModel.textAnimation
            onToggled: value => form.settingsModel.textAnimation = value
        }

        FormDivider { visible: speed.visible }
        ValueSlider {
            id: speed
            visible: form.paragraph && form.settingsModel.textAnimation
            label: qsTr("Text Speed")
            from: 25
            to: 100
            stepSize: 5
            suffix: qsTr("/s")
            value: form.settingsModel.textSpeed
            onMoved: value => form.settingsModel.textSpeed = value
        }

        FormDivider { visible: form.spreadsAvailable }
        FormToggleRow {
            visible: form.spreadsAvailable
            label: qsTr("Two-Column Horizontal Pages")
            checked: form.settingsModel.twoColumnHorizontalPages
            onToggled: value => form.settingsModel.twoColumnHorizontalPages = value
        }

        FormDivider {}
        FormToggleRow {
            objectName: "wheelPageTurnRow"
            label: qsTr("Mouse Wheel Page Turn")
            checked: form.settingsModel.wheelPageTurnEnabled
            onToggled: value => form.settingsModel.wheelPageTurnEnabled = value
        }

        FormDivider {}
        ValueStepper {
            label: qsTr("Horizontal Padding")
            from: 0
            to: 80
            suffix: "%"
            value: form.settingsModel.horizontalPadding
            onStepped: value => form.settingsModel.horizontalPadding = value
        }

        FormDivider {}
        ValueStepper {
            label: qsTr("Vertical Padding")
            from: 0
            to: 50
            suffix: "%"
            value: form.settingsModel.verticalPadding
            onStepped: value => form.settingsModel.verticalPadding = value
        }

        FormDivider {}
        FormToggleRow {
            label: qsTr("Avoid Page Break")
            checked: form.settingsModel.avoidPageBreak
            onToggled: value => form.settingsModel.avoidPageBreak = value
        }

        FormDivider {}
        FormToggleRow {
            label: qsTr("Justify Text")
            checked: form.settingsModel.justifyText
            onToggled: value => form.settingsModel.justifyText = value
        }

        FormDivider {}
        FormToggleRow {
            label: qsTr("Blur Images")
            checked: form.settingsModel.blurImages
            onToggled: value => form.settingsModel.blurImages = value
        }

        FormDivider {}
        FormToggleRow {
            label: qsTr("Advanced")
            checked: form.settingsModel.layoutAdvanced
            onToggled: value => form.settingsModel.layoutAdvanced = value
        }

        FormDivider { visible: form.settingsModel.layoutAdvanced }
        ValueSlider {
            visible: form.settingsModel.layoutAdvanced
            label: qsTr("Line Height")
            from: 1
            to: 2.5
            stepSize: 0.05
            decimals: 2
            value: form.settingsModel.lineHeight
            onMoved: value => form.settingsModel.lineHeight = value
        }

        FormDivider { visible: form.settingsModel.layoutAdvanced }
        ValueSlider {
            visible: form.settingsModel.layoutAdvanced
            label: qsTr("Character Spacing")
            from: -10
            to: 10
            stepSize: 1
            suffix: "%"
            value: form.settingsModel.characterSpacing
            onMoved: value => form.settingsModel.characterSpacing = value
        }

        FormDivider { visible: form.settingsModel.layoutAdvanced }
        ValueSlider {
            visible: form.settingsModel.layoutAdvanced
            label: qsTr("Paragraph Spacing")
            from: 0
            to: 3
            stepSize: 0.1
            decimals: 1
            suffix: qsTr("em")
            value: form.settingsModel.paragraphSpacing
            onMoved: value => form.settingsModel.paragraphSpacing = value
        }
    }

    ReaderDisplayCard {
        settings: form.settingsModel
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
            value: form.settingsModel.popupWidth
            onMoved: value => form.settingsModel.popupWidth = value
        }

        FormDivider { objectName: "popupSeparator" }
        ValueSlider {
            objectName: "popupHeightRow"
            label: qsTr("Height")
            from: 100
            to: 800
            stepSize: 10
            value: form.settingsModel.popupHeight
            onMoved: value => form.settingsModel.popupHeight = value
        }

        FormDivider { objectName: "popupSeparator" }
        ValueSlider {
            objectName: "popupScaleRow"
            label: qsTr("Scale")
            from: 0.8
            to: 1.5
            stepSize: 0.05
            decimals: 2
            value: form.settingsModel.popupScale
            onMoved: value => form.settingsModel.popupScale = value
        }

        FormDivider { objectName: "popupSeparator" }
        FormToggleRow {
            objectName: "popupActionBarRow"
            label: qsTr("Show Action Bar")
            checked: form.settingsModel.popupActionBar
            onToggled: value => form.settingsModel.popupActionBar = value
        }

        FormDivider { objectName: "popupSeparator" }
        FormToggleRow {
            objectName: "popupDisableTransparencyRow"
            label: qsTr("Disable Transparency")
            checked: form.settingsModel.popupDisableTransparency
            onToggled: value => form.settingsModel.popupDisableTransparency = value
        }

        FormDivider { objectName: "popupSeparator" }
        FormToggleRow {
            objectName: "popupFullWidthRow"
            label: qsTr("Full-width")
            checked: form.settingsModel.popupFullWidth
            onToggled: value => form.settingsModel.popupFullWidth = value
        }

        FormDivider { objectName: "popupSeparator" }
        FormToggleRow {
            objectName: "popupSwipeToDismissRow"
            label: qsTr("Swipe to Dismiss")
            checked: form.settingsModel.popupSwipeToDismiss
            onToggled: value => form.settingsModel.popupSwipeToDismiss = value
        }

        FormDivider {
            objectName: "popupSwipeThresholdSeparator"
            visible: form.settingsModel.popupSwipeToDismiss
        }
        ValueSlider {
            objectName: "popupSwipeThresholdRow"
            visible: form.settingsModel.popupSwipeToDismiss
            label: qsTr("Swipe Threshold")
            from: 20
            to: 80
            stepSize: 5
            value: form.settingsModel.popupSwipeThreshold
            onMoved: value => form.settingsModel.popupSwipeThreshold = value
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Subtitle Appearance, following VideoInspectorView.swift
// `subtitleAppearanceSection` and UserConfig's video appearance keys: ASS
// styling, font family/size/weight, edge style and strength, background
// disabled and opacity, vertical position and the three colours. Each control
// writes its stored key through the engine; Restore Defaults writes
// UserConfig.resetVideoSubtitleAppearance's defaults. The mask rows are not
// drawn (Decision).
//
// Object names: inspectorAssToggle, inspectorFontFamily, inspectorFontSize,
// inspectorFontWeight, inspectorEdgeStyle, inspectorEdgeStrength,
// inspectorNoBackground, inspectorBackgroundOpacity, inspectorVerticalPosition,
// inspectorSubtitleColor, inspectorLookupHighlightColor,
// inspectorLookupHighlightTextColor, inspectorRestoreDefaults.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

VideoInspectorSection {
    id: section

    required property var engine
    readonly property var settings: engine.videoSettings
    readonly property var families: Qt.fontFamilies().sort(function (a, b) {
        return a.localeCompare(b, undefined, { numeric: true, sensitivity: "base" })
    })
    readonly property var edgeKeys: ["off", "softShadow", "clearOutline", "highContrast"]

    // A stored "#RRGGBB" or "#RRGGBBAA" as a colour, and a colour back as "#RRGGBBAA".
    function storedColor(key) {
        const hex = String(settings[key])
        if (hex.length === 9)
            return "#" + hex.slice(7) + hex.slice(1, 7)
        return hex.length === 7 ? hex : Theme.textPrimary
    }
    function hexColor(value) {
        return "#" + [value.r, value.g, value.b, value.a].map(function (channel) {
            return Math.round(channel * 255).toString(16).padStart(2, "0").toUpperCase()
        }).join("")
    }
    function write(key, value) {
        engine.changeVideoSetting(key, value)
    }

    component SettingToggle: FormToggleRow {
        id: toggle

        required property string settingKey

        checked: Boolean(section.settings[settingKey])
        onToggled: value => section.write(settingKey, value)
    }

    component SettingSlider: FormSliderRow {
        id: slider

        readonly property real current: Number(section.settings[settingKey])

        value: current
        onMoved: value => section.write(settingKey, value)
    }

    component SettingColor: FormColorRow {
        id: colorRow

        required property string settingKey

        color: section.storedColor(settingKey)
        onPicked: value => section.write(settingKey, section.hexColor(value))
    }

    sectionTitle: qsTr("Subtitle Appearance")
    headerAccessory: LinkButton {
        objectName: "inspectorRestoreDefaults"
        small: true
        text: qsTr("Restore Defaults")
        onClicked: section.engine.resetSubtitleAppearance()
    }

    SettingToggle {
        controlName: "inspectorAssToggle"
        settingKey: "videoRespectASSStyle"
        glyph: "textformat.alt"
        label: qsTr("Respect ASS subtitle styles")
    }

    FormDivider {}

    FormPickerRow {
        controlName: "inspectorFontFamily"
        glyph: "textformat"
        label: qsTr("Subtitle Font")
        model: [qsTr("System Default")].concat(section.families)
        currentIndex: Math.max(0, section.families.indexOf(
            String(section.settings.videoSubtitleFontFamily)) + 1)
        onActivated: index => section.write("videoSubtitleFontFamily",
                                            index === 0 ? "" : section.families[index - 1])
    }

    FormDivider {}

    SettingSlider {
        id: sizeRow

        controlName: "inspectorFontSize"
        settingKey: "videoSubtitleFontSize"
        glyph: "textformat.size"
        label: qsTr("Subtitle Size")
        valueText: Math.trunc(sizeRow.current) + " px"
        from: 12
        to: 72
        stepSize: 1
    }

    FormDivider {}

    FormRow {
        glyph: "bold"
        label: qsTr("Subtitle Weight")

        SpinBox {
            objectName: "inspectorFontWeight"
            from: 100
            to: 900
            stepSize: 100
            editable: false
            font.pixelSize: Theme.calloutFont.pixelSize
            font.features: { "tnum": 1 }
            value: Number(section.settings.videoSubtitleFontWeight)
            Accessible.name: qsTr("Subtitle Weight")
            onValueModified: {
                section.write("videoSubtitleFontWeight", value)
                value = Qt.binding(() => Number(section.settings.videoSubtitleFontWeight))
            }
        }
    }

    FormDivider {}

    FormPickerRow {
        controlName: "inspectorEdgeStyle"
        glyph: "character.textbox"
        label: qsTr("Edge Style")
        model: [qsTr("Off"), qsTr("Soft Shadow"), qsTr("Clear Outline"), qsTr("High Contrast")]
        currentIndex: Math.max(0, section.edgeKeys.indexOf(String(section.settings.videoSubtitleEdgeStyle)))
        onActivated: index => section.write("videoSubtitleEdgeStyle", section.edgeKeys[index])
    }

    FormDivider {}

    SettingSlider {
        id: strengthRow

        controlName: "inspectorEdgeStrength"
        settingKey: "videoSubtitleEdgeStrength"
        glyph: "circle.dashed"
        label: qsTr("Edge Strength")
        valueText: Math.round(strengthRow.current * 100) + "%"
        from: 0
        to: 1
        stepSize: 0.05
        enabled: String(section.settings.videoSubtitleEdgeStyle) !== "off"
    }

    FormDivider {}

    SettingToggle {
        controlName: "inspectorNoBackground"
        settingKey: "videoSubtitleBackgroundDisabled"
        glyph: "rectangle.dashed"
        label: qsTr("No Background")
    }

    FormDivider {}

    SettingSlider {
        id: opacityRow

        controlName: "inspectorBackgroundOpacity"
        settingKey: "videoSubtitleBackgroundOpacity"
        glyph: "rectangle.fill"
        label: qsTr("Background Opacity")
        valueText: Math.trunc(opacityRow.current * 100) + "%"
        from: 0
        to: 1
        stepSize: 0.05
        enabled: !Boolean(section.settings.videoSubtitleBackgroundDisabled)
    }

    FormDivider {}

    SettingSlider {
        controlName: "inspectorVerticalPosition"
        settingKey: "videoSubtitleVerticalPositionFraction"
        glyph: "arrow.up.and.down"
        label: qsTr("Vertical Position")
        leadingGlyph: "rectangle.topthird.inset.filled"
        trailingGlyph: "rectangle.bottomthird.inset.filled"
        from: 0
        to: 1
    }

    FormDivider {}

    SettingColor {
        controlName: "inspectorSubtitleColor"
        settingKey: "videoSubtitleColor"
        label: qsTr("Subtitle Color")
    }

    FormDivider {}

    SettingColor {
        controlName: "inspectorLookupHighlightColor"
        settingKey: "videoSubtitleLookupHighlightColor"
        label: qsTr("Lookup Highlight Color")
    }

    FormDivider {}

    SettingColor {
        controlName: "inspectorLookupHighlightTextColor"
        settingKey: "videoSubtitleLookupHighlightTextColor"
        label: qsTr("Lookup Highlight Text Color")
    }
}

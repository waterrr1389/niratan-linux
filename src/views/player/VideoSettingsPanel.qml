// SPDX-License-Identifier: GPL-3.0-or-later
// Local settings sections follow VideoSettingsView.swift.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Niratan

Dialog {
    id: panel
    required property MpvItem engine
    readonly property var values: engine.videoSettings
    property string errorText: ""
    title: qsTr("Video Settings")
    modal: true
    standardButtons: Dialog.Close
    onOpened: errorText = ""

    function edit(key, value) {
        const saved = engine.changeVideoSetting(key, value)
        errorText = saved ? "" : qsTr("Could not save video settings.")
        return saved
    }

    contentItem: ScrollView {
        id: scroller
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroller.availableWidth
            spacing: Theme.spacingSm

            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: panel.errorText
                wrapMode: Text.WordWrap
                color: Theme.textPrimary
            }

            Heading { text: qsTr("Playback") }
            Label {
                Layout.fillWidth: true
                text: qsTr("Unavailable features are shown disabled.")
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            Toggle { key: "videoAutoPlayNext"; text: qsTr("Auto-Play Next Episode"); enabled: false }
            Toggle { key: "videoRememberPlaybackPosition"; text: qsTr("Remember Playback State") }
            Toggle { key: "videoAutoPauseOnLookup"; text: qsTr("Auto-pause on Lookup") }
            Toggle { key: "videoSubtitleGapFastForwardEnabled"; text: qsTr("Fast-forward Subtitle Gaps"); enabled: false }
            NumberRow {
                key: "videoSubtitleGapFastForwardSpeed"; title: qsTr("Fast-forward Speed")
                from: 1.1; to: 5; increment: 0.1; decimals: 1; suffix: "×"
                enabled: false
            }
            Choice {
                key: "videoControlBarLayout"; title: qsTr("Control Bar Layout")
                names: [qsTr("Floating"), qsTr("Compact Bottom")]
                keys: ["floating", "compactBottom"]
            }
            NumberRow {
                key: "videoSeekInterval"; title: qsTr("Default Seek Interval")
                from: 1; to: 60; suffix: qsTr(" s")
            }

            Heading { text: qsTr("Video Enhancement") }
            Toggle { key: "videoHardwareDecodingEnabled"; text: qsTr("Hardware Decoding") }
            Toggle { key: "videoDeinterlacingEnabled"; text: qsTr("Deinterlacing") }
            Toggle { key: "videoHDREnhancementEnabled"; text: qsTr("HDR Enhancement") }
            Choice {
                key: "videoShaderPreset"; title: qsTr("Anime4K"); enabled: false
                names: [qsTr("Off"), qsTr("Anime4K Fast"), qsTr("Anime4K High Quality")]
                keys: ["off", "anime4KFast", "anime4KHighQuality"]
            }
            Equalizer { key: "videoBrightness"; title: qsTr("Brightness") }
            Equalizer { key: "videoContrast"; title: qsTr("Contrast") }
            Equalizer { key: "videoSaturation"; title: qsTr("Saturation") }
            Equalizer { key: "videoGamma"; title: qsTr("Gamma") }
            Equalizer { key: "videoHue"; title: qsTr("Hue") }

            Heading { text: qsTr("Mining") }
            NumberRow { key: "videoMiningHistoryLimit"; title: qsTr("Mining History Storage Limit"); from: 0; to: 1000 }
            Label {
                Layout.fillWidth: true
                text: qsTr("Set to 0 to disable Mining History.")
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            Heading { text: qsTr("Subtitle Appearance") }
            Choice {
                key: "videoSubtitleFontFamily"; title: qsTr("Font Family")
                readonly property var families: Qt.fontFamilies()
                names: [qsTr("System Default")].concat(families)
                keys: [""].concat(families)
            }
            Toggle { key: "videoRespectASSStyle"; text: qsTr("Respect ASS Styling") }
            NumberRow { key: "videoSubtitleFontSize"; title: qsTr("Font Size"); from: 12; to: 72 }
            NumberRow { key: "videoSubtitleFontWeight"; title: qsTr("Font Weight"); from: 100; to: 900; increment: 100 }
            Choice {
                key: "videoSubtitleEdgeStyle"; title: qsTr("Edge Style")
                names: [qsTr("Off"), qsTr("Soft Shadow"), qsTr("Clear Outline"), qsTr("High Contrast")]
                keys: ["off", "softShadow", "clearOutline", "highContrast"]
            }
            NumberRow {
                key: "videoSubtitleEdgeStrength"; title: qsTr("Edge Strength")
                from: 0; to: 1; increment: 0.05; decimals: 2
                enabled: panel.values.videoSubtitleEdgeStyle !== "off"
            }
            Toggle { key: "videoSubtitleBackgroundDisabled"; text: qsTr("No Background") }
            NumberRow {
                key: "videoSubtitleBackgroundOpacity"; title: qsTr("Background Opacity")
                from: 0; to: 1; increment: 0.05; decimals: 2
                enabled: !panel.values.videoSubtitleBackgroundDisabled
            }
            NumberRow {
                key: "videoSubtitleVerticalPositionFraction"; title: qsTr("Vertical Position")
                from: 0; to: 1; increment: 0.01; decimals: 2
            }
            ColorRow { key: "videoSubtitleColor"; title: qsTr("Subtitle Color") }
            ColorRow { key: "videoSubtitleLookupHighlightColor"; title: qsTr("Lookup Highlight Color") }
            ColorRow { key: "videoSubtitleLookupHighlightTextColor"; title: qsTr("Lookup Highlight Text Color") }
            Button {
                text: qsTr("Restore Subtitle Appearance Defaults")
                onClicked: panel.errorText = panel.engine.resetSubtitleAppearance()
                    ? "" : qsTr("Could not save video settings.")
            }

            Heading { text: qsTr("Subtitle Mask") }
            Toggle { key: "videoSubtitleMaskEnabled"; text: qsTr("Mask subtitles until hover"); enabled: false }
            Choice {
                key: "videoSubtitleMaskMode"; title: qsTr("Mask Mode")
                names: [qsTr("Blur"), qsTr("Transparent")]
                keys: ["Blur", "Transparent"]; enabled: false
            }
            NumberRow {
                key: "videoSubtitleMaskBlurRadius"; title: qsTr("Blur Radius")
                from: 0; to: 20; suffix: qsTr(" px"); enabled: false
                visible: panel.values.videoSubtitleMaskMode === "Blur"
            }
            NumberRow {
                key: "videoSubtitleMaskHiddenOpacity"; title: qsTr("Hidden Opacity")
                from: 0; to: 1; increment: 0.05; decimals: 2; enabled: false
                visible: panel.values.videoSubtitleMaskMode === "Transparent"
            }
        }
    }

    component Heading: Label {
        Layout.fillWidth: true
        Layout.topMargin: Theme.spacingMd
        color: Theme.textPrimary
        font.bold: true
        font.pixelSize: Theme.fontBody
    }

    component Row: RowLayout {
        id: setting
        required property string key
        required property string title
        Layout.fillWidth: true
        Label {
            Layout.fillWidth: true
            text: setting.title
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
        }
    }

    component Toggle: Switch {
        id: toggle
        required property string key
        checked: Boolean(panel.values[key])
        onToggled: {
            panel.edit(key, checked)
            checked = Qt.binding(function() { return Boolean(panel.values[toggle.key]) })
        }
    }

    component NumberRow: Row {
        id: numberRow
        required property real from
        required property real to
        property real increment: 1
        property int decimals: 0
        property string suffix: ""
        property bool resettable: false
        readonly property real value: Number(panel.values[key])
        Slider {
            id: slider
            Layout.preferredWidth: 130
            from: numberRow.from
            to: numberRow.to
            stepSize: numberRow.increment
            snapMode: Slider.SnapAlways
            value: numberRow.value
            Accessible.name: numberRow.title
            onMoved: if (!pressed) commit()
            onPressedChanged: if (!pressed && value !== numberRow.value) commit()
            function commit() {
                panel.edit(numberRow.key, value)
                value = Qt.binding(function() { return numberRow.value })
            }
        }
        Label {
            Layout.minimumWidth: 45
            text: slider.value.toFixed(numberRow.decimals) + numberRow.suffix
            color: Theme.textSecondary
            horizontalAlignment: Text.AlignRight
        }
        Button {
            visible: numberRow.resettable
            text: qsTr("Reset")
            onClicked: panel.edit(numberRow.key, 0)
        }
    }

    component Equalizer: NumberRow { from: -100; to: 100; resettable: true }

    component Choice: Row {
        id: choice
        required property var names
        required property var keys
        ComboBox {
            Layout.preferredWidth: 180
            model: choice.names
            currentIndex: choice.keys.indexOf(panel.values[choice.key])
            Accessible.name: choice.title
            onActivated: function(index) {
                panel.edit(choice.key, choice.keys[index])
                currentIndex = Qt.binding(function() { return choice.keys.indexOf(panel.values[choice.key]) })
            }
        }
    }

    component ColorRow: Row {
        id: colorRow
        readonly property string hex: String(panel.values[key])
        readonly property color swatch: hex.length === 9 ? "#" + hex.slice(7) + hex.slice(1, 7) : hex
        Button {
            text: colorRow.hex
            Accessible.name: colorRow.title
            onClicked: {
                picker.selectedColor = colorRow.swatch
                picker.open()
            }
        }
        ColorDialog {
            id: picker
            title: colorRow.title
            options: ColorDialog.ShowAlphaChannel
            onAccepted: {
                const rgba = [selectedColor.r, selectedColor.g, selectedColor.b, selectedColor.a]
                panel.edit(colorRow.key, "#" + rgba.map(function(channel) {
                    return Math.round(channel * 255).toString(16).padStart(2, "0")
                }).join(""))
            }
        }
    }
}

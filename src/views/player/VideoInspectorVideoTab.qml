// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Inspector's Video tab, following VideoInspectorView.swift `videoTab`:
// Playback Speed, Picture, Video Track, Loop, Video Enhancement and Picture
// Adjustments (Anime4K is not drawn, Decision).
//
// Speed presets, slider and custom field all go through the Inspector model;
// its `speedRequested` reaches the page through the panel. Picture writes
// `engine.aspectRatio` and rotates. The track card selects mpv video tracks.
// Loop writes `engine.looping` and drives the A-B range. Enhancement toggles
// and the equalizer sliders write stored keys through `engine`; Reset writes 0
// to all five adjustments (VideoEqualizerAdjustment.neutral).
//
// Object names: inspectorSpeedPreset (repeated; `speedValue`, `selected`),
// inspectorSpeedSlider, inspectorSpeedField, inspectorAspectMenu,
// inspectorRotateButton, inspectorLoopToggle, inspectorLoopStartButton,
// inspectorLoopEndButton, inspectorLoopClearButton, inspectorABRange,
// inspectorHardwareToggle, inspectorDeinterlaceToggle, inspectorHdrToggle,
// inspectorEqualizer (repeated; `settingKey`), inspectorEqualizerReset.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Column {
    id: tab

    required property var model
    required property var engine

    signal selectTrackRequested(string type, int id)
    signal speedRequested(real value)

    readonly property var settings: engine.videoSettings
    readonly property var equalizerKeys: [
        "videoBrightness", "videoContrast", "videoSaturation", "videoGamma", "videoHue"
    ]
    readonly property bool adjustmentsChanged: equalizerKeys.some(
        function (key) { return Number(tab.settings[key]) !== 0 })
    readonly property bool hasABRange: engine.loopStart >= 0 && engine.loopEnd > engine.loopStart
    readonly property var aspectValues: ["-1", "16:9", "4:3", "1:1", "21:9"]

    function formatTime(seconds) {
        if (!isFinite(seconds) || seconds <= 0)
            return "0:00"
        const total = Math.floor(seconds)
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor((total % 3600) / 60)
        const remainder = total % 60
        const secondsText = remainder < 10 ? "0" + remainder : String(remainder)
        if (hours > 0) {
            const minutesText = minutes < 10 ? "0" + minutes : String(minutes)
            return hours + ":" + minutesText + ":" + secondsText
        }
        return minutes + ":" + secondsText
    }

    component SettingToggle: FormToggleRow {
        required property string settingKey

        checked: Boolean(tab.settings[settingKey])
        onToggled: value => tab.engine.changeVideoSetting(settingKey, value)
    }

    component EqualizerRow: FormSliderRow {
        id: eqRow

        readonly property real current: Number(tab.settings[settingKey])

        controlName: "inspectorEqualizer"
        from: -100
        to: 100
        stepSize: 1
        value: current
        valueText: String(Math.round(current))
        valueHighlighted: current !== 0
        onMoved: value => tab.engine.changeVideoSetting(settingKey, value)

        GlyphButton {
            visible: eqRow.current !== 0
            glyph: "arrow.counterclockwise"
            diameter: Theme.tinyGlyphButtonDiameter
            glyphSize: Theme.tinyGlyphSize
            filled: false
            Accessible.name: qsTr("Reset")
            onClicked: tab.engine.changeVideoSetting(eqRow.settingKey, 0)
        }
    }

    spacing: Theme.panelSpacing

    VideoInspectorSpeedSection {
        model: tab.model
        onSpeedRequested: value => tab.speedRequested(value)
    }

    VideoInspectorSection {
        sectionTitle: qsTr("Picture")

        FormPickerRow {
            controlName: "inspectorAspectMenu"
            glyph: "rectangle.inset.filled"
            label: qsTr("Aspect Ratio")
            model: [qsTr("Automatic"), "16:9", "4:3", "1:1", "21:9"]
            currentIndex: Math.max(0, tab.aspectValues.indexOf(tab.engine.aspectRatio))
            onActivated: index => tab.engine.aspectRatio = tab.aspectValues[index]
        }

        FormDivider {}

        FormRow {
            glyph: "rotate.right"
            label: qsTr("Rotate Clockwise")

            GlyphButton {
                objectName: "inspectorRotateButton"
                glyph: "rotate.right"
                diameter: Theme.smallGlyphButtonDiameter
                filled: false
                Accessible.name: qsTr("Rotate Clockwise")
                onClicked: tab.engine.rotateClockwise()
            }
        }
    }

    VideoInspectorTrackSection {
        title: qsTr("Video Track")
        type: "video"
        tracks: tab.model.videoTracks
        onSelectTrackRequested: (type, id) => tab.selectTrackRequested(type, id)
    }

    VideoInspectorSection {
        sectionTitle: qsTr("Loop")

        FormToggleRow {
            controlName: "inspectorLoopToggle"
            glyph: "repeat"
            label: qsTr("Loop File")
            checked: tab.engine.looping
            onToggled: value => tab.engine.looping = value
        }

        FormDivider {}

        FormRow {
            glyph: "point.forward.to.point.capsulepath"
            label: qsTr("A-B Loop")

            StyledLabel {
                objectName: "inspectorABRange"
                visible: tab.hasABRange
                text: tab.formatTime(tab.engine.loopStart) + " – " + tab.formatTime(tab.engine.loopEnd)
                textStyle: Theme.captionFont
                weight: Theme.weightMedium
                monospacedDigits: true
                color: Theme.accent
            }
        }

        Row {
            x: Theme.compactRowPadding
            bottomPadding: Theme.compactRowPadding
            spacing: Theme.chipSpacing

            ChipButton {
                objectName: "inspectorLoopStartButton"
                text: qsTr("Set A Point")
                onClicked: tab.engine.setABLoopStart()
            }

            ChipButton {
                objectName: "inspectorLoopEndButton"
                text: qsTr("Set B Point")
                onClicked: tab.engine.setABLoopEnd()
            }

            ChipButton {
                objectName: "inspectorLoopClearButton"
                text: qsTr("Clear")
                enabled: tab.hasABRange
                onClicked: tab.engine.clearABLoop()
            }
        }
    }

    VideoInspectorSection {
        sectionTitle: qsTr("Video Enhancement")

        SettingToggle {
            controlName: "inspectorHardwareToggle"
            settingKey: "videoHardwareDecodingEnabled"
            glyph: "cpu"
            label: qsTr("Hardware Decoding")
        }

        FormDivider {}

        SettingToggle {
            controlName: "inspectorDeinterlaceToggle"
            settingKey: "videoDeinterlacingEnabled"
            glyph: "line.3.horizontal"
            label: qsTr("Deinterlace")
        }

        FormDivider {}

        SettingToggle {
            controlName: "inspectorHdrToggle"
            settingKey: "videoHDREnhancementEnabled"
            glyph: "sun.max.trianglebadge.exclamationmark"
            label: qsTr("HDR")
        }
    }

    VideoInspectorSection {
        sectionTitle: qsTr("Picture Adjustments")
        headerAccessory: LinkButton {
            objectName: "inspectorEqualizerReset"
            small: true
            visible: tab.adjustmentsChanged
            text: qsTr("Reset")
            onClicked: {
                for (const key of tab.equalizerKeys)
                    tab.engine.changeVideoSetting(key, 0)
            }
        }

        EqualizerRow {
            settingKey: "videoBrightness"
            glyph: "sun.max"
            label: qsTr("Brightness")
        }

        FormDivider {}

        EqualizerRow {
            settingKey: "videoContrast"
            glyph: "circle.lefthalf.filled"
            label: qsTr("Contrast")
        }

        FormDivider {}

        EqualizerRow {
            settingKey: "videoSaturation"
            glyph: "drop"
            label: qsTr("Saturation")
        }

        FormDivider {}

        EqualizerRow {
            settingKey: "videoGamma"
            glyph: "camera.filters"
            label: qsTr("Gamma")
        }

        FormDivider {}

        EqualizerRow {
            settingKey: "videoHue"
            glyph: "paintpalette"
            label: qsTr("Hue")
        }
    }
}

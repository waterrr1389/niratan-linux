// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Settings/VideoSettingsView.swift body: the Playback, Video
// Enhancement, Mining and Subtitle Mask cards. Subtitle Appearance stays in
// the player Inspector and Anime4K is not ported (V0), following the module
// Decisions. Every row reads and writes a VideoSettingsViewModel property,
// which stores only its own key through services/player/video_settings; the
// page forwards the model's videoSettingsChanged so an open player reloads.
//
// Object names for the tests: settings-video, videoPlaybackSection,
// videoEnhancementSection, videoMiningSection, videoMaskSection,
// videoAutoPlayNextToggle, videoRememberPlaybackToggle, videoAutoPauseToggle,
// videoGapFastForwardToggle, videoGapFastForwardSpeed, videoControlBarLayout,
// videoSeekInterval, videoHardwareDecodingToggle, videoDeinterlaceToggle,
// videoHdrToggle, videoEqualizer (repeated; settingKey),
// videoEqualizerReset (repeated; settingKey), videoMiningLimit,
// videoMaskToggle, videoMaskMode, videoMaskBlurRadius,
// videoMaskHiddenOpacity.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

PanelScaffold {
    id: page

    objectName: "settings-video"

    signal videoSettingsChanged()

    VideoSettingsViewModel {
        id: settings

        onVideoSettingsChanged: page.videoSettingsChanged()
    }

    FormSection {
        objectName: "videoPlaybackSection"
        title: qsTr("Playback")
        footer: qsTr("Restores playback position, subtitles, speed, timing, and audio track for each video.")

        FormToggleRow {
            objectName: "videoAutoPlayNextToggle"
            label: qsTr("Auto-Play Next Episode")
            checked: settings.videoAutoPlayNext
            onToggled: value => settings.videoAutoPlayNext = value
        }

        FormDivider {}

        FormToggleRow {
            objectName: "videoRememberPlaybackToggle"
            label: qsTr("Remember Playback State")
            checked: settings.videoRememberPlaybackPosition
            onToggled: value => settings.videoRememberPlaybackPosition = value
        }

        FormDivider {}

        FormToggleRow {
            objectName: "videoAutoPauseToggle"
            label: qsTr("Auto-pause on Lookup")
            checked: settings.videoAutoPauseOnLookup
            onToggled: value => settings.videoAutoPauseOnLookup = value
        }

        FormDivider {}

        FormToggleRow {
            objectName: "videoGapFastForwardToggle"
            label: qsTr("Fast-forward Subtitle Gaps")
            checked: settings.videoSubtitleGapFastForwardEnabled
            onToggled: value => settings.videoSubtitleGapFastForwardEnabled = value
        }

        FormDivider {}

        FormStepperRow {
            objectName: "videoGapFastForwardSpeed"
            label: qsTr("Fast-forward Speed")
            subtitle: qsTr("Temporarily speeds through gaps between subtitle lines.")
            value: settings.videoSubtitleGapFastForwardSpeed
            from: 1.1
            to: 5
            stepSize: 0.1
            valueText: settings.videoSubtitleGapFastForwardSpeed.toFixed(1) + "x"
            enabled: settings.videoSubtitleGapFastForwardEnabled
            onStepped: value => settings.videoSubtitleGapFastForwardSpeed = value
        }

        FormDivider {}

        FormSegmentedRow {
            objectName: "videoControlBarLayout"
            label: qsTr("Control Bar Layout")
            model: [qsTr("Floating"), qsTr("Compact Bottom")]
            currentIndex: settings.videoControlBarLayout === "compactBottom" ? 1 : 0
            onActivated: index => settings.videoControlBarLayout =
                index === 0 ? "floating" : "compactBottom"
        }

        FormDivider {}

        FormStepperRow {
            objectName: "videoSeekInterval"
            label: qsTr("Default Seek Interval")
            value: settings.videoSeekInterval
            from: 1
            to: 60
            stepSize: 1
            valueText: settings.videoSeekInterval + " s"
            onStepped: value => settings.videoSeekInterval = value
        }
    }

    FormSection {
        objectName: "videoEnhancementSection"
        title: qsTr("Video Enhancement")
        footer: qsTr("Hardware decoding uses mpv's automatic safe decoder path. HDR changes mpv tone-mapping peak handling.")

        FormToggleRow {
            objectName: "videoHardwareDecodingToggle"
            label: qsTr("Hardware Decoding")
            checked: settings.videoHardwareDecodingEnabled
            onToggled: value => settings.videoHardwareDecodingEnabled = value
        }

        FormDivider {}

        FormToggleRow {
            objectName: "videoDeinterlaceToggle"
            label: qsTr("Deinterlace")
            checked: settings.videoDeinterlacingEnabled
            onToggled: value => settings.videoDeinterlacingEnabled = value
        }

        FormDivider {}

        FormToggleRow {
            objectName: "videoHdrToggle"
            label: qsTr("HDR")
            checked: settings.videoHDREnhancementEnabled
            onToggled: value => settings.videoHDREnhancementEnabled = value
        }

        FormDivider {}

        FormSliderRow {
            objectName: "videoEqualizer"
            settingKey: "videoBrightness"
            label: qsTr("Brightness")
            value: settings.videoBrightness
            from: -100
            to: 100
            stepSize: 1
            valueText: String(Math.round(settings.videoBrightness))
            onMoved: value => settings.videoBrightness = value

            GlyphButton {
                objectName: "videoEqualizerReset"
                property string settingKey: "videoBrightness"
                glyph: "arrow.counterclockwise"
                diameter: Theme.tinyGlyphButtonDiameter
                glyphSize: Theme.tinyGlyphSize
                filled: false
                Accessible.name: qsTr("Reset")
                onClicked: settings.videoBrightness = 0
            }
        }

        FormDivider {}

        FormSliderRow {
            objectName: "videoEqualizer"
            settingKey: "videoContrast"
            label: qsTr("Contrast")
            value: settings.videoContrast
            from: -100
            to: 100
            stepSize: 1
            valueText: String(Math.round(settings.videoContrast))
            onMoved: value => settings.videoContrast = value

            GlyphButton {
                objectName: "videoEqualizerReset"
                property string settingKey: "videoContrast"
                glyph: "arrow.counterclockwise"
                diameter: Theme.tinyGlyphButtonDiameter
                glyphSize: Theme.tinyGlyphSize
                filled: false
                Accessible.name: qsTr("Reset")
                onClicked: settings.videoContrast = 0
            }
        }

        FormDivider {}

        FormSliderRow {
            objectName: "videoEqualizer"
            settingKey: "videoSaturation"
            label: qsTr("Saturation")
            value: settings.videoSaturation
            from: -100
            to: 100
            stepSize: 1
            valueText: String(Math.round(settings.videoSaturation))
            onMoved: value => settings.videoSaturation = value

            GlyphButton {
                objectName: "videoEqualizerReset"
                property string settingKey: "videoSaturation"
                glyph: "arrow.counterclockwise"
                diameter: Theme.tinyGlyphButtonDiameter
                glyphSize: Theme.tinyGlyphSize
                filled: false
                Accessible.name: qsTr("Reset")
                onClicked: settings.videoSaturation = 0
            }
        }

        FormDivider {}

        FormSliderRow {
            objectName: "videoEqualizer"
            settingKey: "videoGamma"
            label: qsTr("Gamma")
            value: settings.videoGamma
            from: -100
            to: 100
            stepSize: 1
            valueText: String(Math.round(settings.videoGamma))
            onMoved: value => settings.videoGamma = value

            GlyphButton {
                objectName: "videoEqualizerReset"
                property string settingKey: "videoGamma"
                glyph: "arrow.counterclockwise"
                diameter: Theme.tinyGlyphButtonDiameter
                glyphSize: Theme.tinyGlyphSize
                filled: false
                Accessible.name: qsTr("Reset")
                onClicked: settings.videoGamma = 0
            }
        }

        FormDivider {}

        FormSliderRow {
            objectName: "videoEqualizer"
            settingKey: "videoHue"
            label: qsTr("Hue")
            value: settings.videoHue
            from: -100
            to: 100
            stepSize: 1
            valueText: String(Math.round(settings.videoHue))
            onMoved: value => settings.videoHue = value

            GlyphButton {
                objectName: "videoEqualizerReset"
                property string settingKey: "videoHue"
                glyph: "arrow.counterclockwise"
                diameter: Theme.tinyGlyphButtonDiameter
                glyphSize: Theme.tinyGlyphSize
                filled: false
                Accessible.name: qsTr("Reset")
                onClicked: settings.videoHue = 0
            }
        }
    }

    FormSection {
        objectName: "videoMiningSection"
        title: qsTr("Mining")
        footer: qsTr("Set to 0 to disable and clear Mining History.")

        FormStepperRow {
            objectName: "videoMiningLimit"
            label: qsTr("Mining History Storage Limit")
            value: settings.videoMiningHistoryLimit
            from: 0
            to: 1000
            stepSize: 1
            valueText: String(settings.videoMiningHistoryLimit)
            onStepped: value => settings.videoMiningHistoryLimit = value
        }
    }

    FormSection {
        objectName: "videoMaskSection"
        title: qsTr("Subtitle Mask")
        footer: qsTr("Masked subtitles are shown normally while the pointer is over the subtitle row.")

        FormToggleRow {
            objectName: "videoMaskToggle"
            label: qsTr("Mask subtitles until hover")
            checked: settings.videoSubtitleMaskEnabled
            onToggled: value => settings.videoSubtitleMaskEnabled = value
        }

        FormDivider {}

        FormSegmentedRow {
            objectName: "videoMaskMode"
            label: qsTr("Mask Mode")
            model: [qsTr("Blur"), qsTr("Transparent")]
            currentIndex: settings.videoSubtitleMaskMode === "Transparent" ? 1 : 0
            enabled: settings.videoSubtitleMaskEnabled
            onActivated: index => settings.videoSubtitleMaskMode =
                index === 0 ? "Blur" : "Transparent"
        }

        FormDivider {}

        FormSliderRow {
            objectName: "videoMaskBlurRadius"
            label: qsTr("Blur Radius")
            visible: settings.videoSubtitleMaskMode === "Blur"
            value: settings.videoSubtitleMaskBlurRadius
            from: 0
            to: 20
            stepSize: 1
            valueText: Math.round(settings.videoSubtitleMaskBlurRadius) + " px"
            onMoved: value => settings.videoSubtitleMaskBlurRadius = value
        }

        FormSliderRow {
            objectName: "videoMaskHiddenOpacity"
            label: qsTr("Hidden Opacity")
            visible: settings.videoSubtitleMaskMode === "Transparent"
            value: settings.videoSubtitleMaskHiddenOpacity
            from: 0
            to: 1
            stepSize: 0.05
            valueText: Math.round(settings.videoSubtitleMaskHiddenOpacity * 100) + "%"
            onMoved: value => settings.videoSubtitleMaskHiddenOpacity = value
        }
    }
}

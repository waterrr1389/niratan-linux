// SPDX-License-Identifier: GPL-3.0-or-later
//
// Local verification sheet for the control kit in src/views/components:
// every kit component in its typical states, a cards form on the left and a
// compact inspector panel on the right, after Pictures/setting.png and
// Pictures/video1.png of Mac Niratan. Shown with NIRATAN_THEME_PREVIEW=kit,
// or kit-end to show both panels scrolled to their end; it is not part of any
// user flow and keeps its demo state locally.
import QtQuick
import Niratan

Item {
    id: sheet

    // Both panels scrolled to their end, to check the bottom padding.
    property bool scrolledToEnd: false
    property int themeIndex: 0
    property bool furigana: true
    property color backgroundColor: "#F2E8D5"
    property int fontIndex: 0
    property int directionIndex: 0
    property real seekInterval: 10
    property real fontSize: 22
    property bool subtitles: true
    property real subtitleSize: 38
    property int subtitleFontIndex: 0
    property real subtitleWeight: 500
    property color subtitleColor: "#CCFFFFFF"
    property bool pauseAtLineEnd: false
    property real volume: 80
    property int speedIndex: 1
    property bool autoplay: true
    property bool transcript: false
    property int secondaryIndex: 0

    width: 960
    height: 640

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBackground
    }

    PanelScaffold {
        id: form

        width: parent.width - inspector.width
        height: parent.height

        FormSection {
            title: qsTr("Theme")
            footer: qsTr("Custom uses the colours below for the reader and the interface picker for windows.")

            FormSegmentedRow {
                fillsWidth: true
                model: [qsTr("System"), qsTr("Light"), qsTr("Dark"), qsTr("Sepia"), qsTr("Custom")]
                currentIndex: sheet.themeIndex
                onActivated: index => sheet.themeIndex = index
            }
            FormDivider {}
            FormToggleRow {
                label: qsTr("Show Furigana")
                checked: sheet.furigana
                onToggled: checked => sheet.furigana = checked
            }
            FormDivider {}
            FormColorRow {
                label: qsTr("Background Color")
                color: sheet.backgroundColor
                onPicked: color => sheet.backgroundColor = color
            }
        }

        FormSection {
            title: qsTr("Text")

            FormPickerRow {
                label: qsTr("Font")
                model: ["Noto Serif CJK JP SemiBold Extended", "Inter", "Noto Sans CJK JP"]
                currentIndex: sheet.fontIndex
                minimumWidth: 140
                onActivated: index => sheet.fontIndex = index
            }
            FormDivider {}
            FormSegmentedRow {
                label: qsTr("Text Direction")
                model: [qsTr("Vertical"), qsTr("Horizontal")]
                currentIndex: sheet.directionIndex
                onActivated: index => sheet.directionIndex = index
            }
            FormDivider {}
            FormStepperRow {
                label: qsTr("Default Seek Interval")
                value: sheet.seekInterval
                from: 1
                to: 60
                valueText: qsTr("%1 s").arg(sheet.seekInterval)
                onStepped: value => sheet.seekInterval = value
            }
            FormDivider {}
            FormSliderRow {
                label: qsTr("Font Size")
                value: sheet.fontSize
                from: 12
                to: 48
                stepSize: 1
                valueText: qsTr("%1 px").arg(sheet.fontSize)
                onMoved: value => sheet.fontSize = value
            }
            FormDivider {}
            FormToggleRow {
                enabled: false
                label: qsTr("Hide Furigana for Known Words")
                subtitle: qsTr("Needs a frequency dictionary with readings.")
                checked: false
            }
        }

        FormSection {
            title: qsTr("Links")

            FormLinkRow {
                label: qsTr("Niratan on GitHub")
                url: "https://github.com/W1ght/Niratan"
            }
            FormDivider {}
            FormButtonRow {
                FormButton {
                    text: qsTr("Export Settings")
                }
                FormButton {
                    text: qsTr("Reset All")
                    destructive: true
                }
            }
            FormDivider {}
            FormRow {
                label: qsTr("Jimaku API Key")

                LinkButton {
                    text: qsTr("Create API Key")
                    url: "https://jimaku.cc/account"
                }
            }
        }

        Column {
            width: parent.width
            spacing: Theme.spacingXs

            StyledLabel {
                leftPadding: Theme.sectionHeaderInset
                bottomPadding: Theme.spacingXs
                text: qsTr("Text Styles")
                textStyle: Theme.subheadlineFont
                weight: Theme.weightSemibold
                secondary: true
            }

            Repeater {
                model: [
                    { name: "Large Title", style: Theme.largeTitleFont },
                    { name: "Title 1", style: Theme.title1Font },
                    { name: "Title 2", style: Theme.title2Font },
                    { name: "Title 3", style: Theme.title3Font },
                    { name: "Headline", style: Theme.headlineFont },
                    { name: "Body", style: Theme.bodyFont },
                    { name: "Callout", style: Theme.calloutFont },
                    { name: "Subheadline", style: Theme.subheadlineFont },
                    { name: "Footnote", style: Theme.footnoteFont },
                    { name: "Caption", style: Theme.captionFont },
                    { name: "Caption 2", style: Theme.caption2Font }
                ]

                delegate: StyledLabel {
                    required property var modelData

                    leftPadding: Theme.sectionHeaderInset
                    text: modelData.name + " " + modelData.style.pixelSize + " — 日本語"
                    textStyle: modelData.style
                }
            }
        }
    }

    Rectangle {
        x: inspector.x - width
        width: Theme.hairlineWidth
        height: parent.height
        color: Theme.separator
    }

    Rectangle {
        id: inspector

        x: parent.width - width
        width: 340
        height: parent.height
        color: Theme.sidebarBackground

        PanelScaffold {
            id: panel

            anchors.fill: parent
            compact: true

            header: PanelHeader {
                title: qsTr("Video")
                subtitle: qsTr("Episode 2 — The Long Road")
            }

            FormSection {
                title: qsTr("Subtitles")
                compact: true
                footer: qsTr("Size and weight apply to the primary subtitle track.")
                headerAccessory: [
                    IconButton {
                        icon: "arrow.clockwise"
                        diameter: 22
                        iconSize: 11
                        accessibleName: qsTr("Reset")
                    }
                ]

                FormToggleRow {
                    glyph: "textformat"
                    label: qsTr("日本語の字幕を表示")
                    checked: sheet.subtitles
                    onToggled: checked => sheet.subtitles = checked
                }
                FormDivider {}
                FormSliderRow {
                    glyph: "textformat.size.larger.ja"
                    label: qsTr("Size")
                    value: sheet.subtitleSize
                    from: 16
                    to: 72
                    stepSize: 1
                    valueText: qsTr("%1 px").arg(sheet.subtitleSize)
                    onMoved: value => sheet.subtitleSize = value
                }
                FormDivider {}
                FormPickerRow {
                    glyph: "character.cursor.ibeam.ja"
                    label: qsTr("Font")
                    model: ["Noto Sans CJK JP", "Inter"]
                    currentIndex: sheet.subtitleFontIndex
                    onActivated: index => sheet.subtitleFontIndex = index
                }
                FormDivider {}
                FormStepperRow {
                    glyph: "highlighter"
                    label: qsTr("Weight")
                    value: sheet.subtitleWeight
                    from: 100
                    to: 900
                    stepSize: 100
                    valueText: String(sheet.subtitleWeight)
                    onStepped: value => sheet.subtitleWeight = value
                }
                FormDivider {}
                FormColorRow {
                    label: qsTr("Text Color")
                    color: sheet.subtitleColor
                    onPicked: color => sheet.subtitleColor = color
                }
                FormDivider {}
                FormToggleRow {
                    glyph: "pause.fill"
                    label: qsTr("Pause automatically at the end of every subtitle line")
                    checked: sheet.pauseAtLineEnd
                    onToggled: checked => sheet.pauseAtLineEnd = checked
                }
            }

            FormSection {
                title: qsTr("Playback")
                compact: true

                FormSegmentedRow {
                    glyph: "timer"
                    label: qsTr("Speed")
                    model: ["0.75×", "1×", "1.5×"]
                    minimumSegmentWidth: 0
                    currentIndex: sheet.speedIndex
                    onActivated: index => sheet.speedIndex = index
                }
                FormDivider {}
                FormSliderRow {
                    glyph: "speaker.wave.2.fill"
                    label: qsTr("Volume")
                    value: sheet.volume
                    from: 0
                    to: 100
                    stepSize: 1
                    valueText: qsTr("%1%").arg(sheet.volume)
                    onMoved: value => sheet.volume = value
                }
                FormDivider {}
                FormToggleRow {
                    glyph: "play.circle"
                    label: qsTr("Play Next Episode")
                    checked: sheet.autoplay
                    onToggled: checked => sheet.autoplay = checked
                }
            }

            FormSection {
                title: qsTr("Transcript")
                compact: true
                footer: qsTr("Lines come from the selected subtitle track.")

                FormToggleRow {
                    glyph: "list.bullet"
                    label: qsTr("Show Transcript")
                    checked: sheet.transcript
                    onToggled: checked => sheet.transcript = checked
                }
                FormDivider {}
                FormPickerRow {
                    glyph: "film.stack"
                    label: qsTr("Secondary Track")
                    model: [qsTr("None"), "English", "日本語"]
                    currentIndex: sheet.secondaryIndex
                    onActivated: index => sheet.secondaryIndex = index
                }
                FormDivider {}
                FormLinkRow {
                    glyph: "magnifyingglass"
                    label: qsTr("Search Jimaku")
                    url: "https://jimaku.cc"
                }
            }
        }
    }

    Binding {
        when: sheet.scrolledToEnd
        target: form.flickable
        property: "contentY"
        value: Math.max(0, form.flickable.contentHeight - form.flickable.height)
    }

    Binding {
        when: sheet.scrolledToEnd
        target: panel.flickable
        property: "contentY"
        value: Math.max(0, panel.flickable.contentHeight - panel.flickable.height)
    }
}

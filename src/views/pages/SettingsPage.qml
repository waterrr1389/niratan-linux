// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reader, dictionary, statistics and Sasayaki controls follow AppearanceView,
// DictionaryBehaviorSettingsSections, StatisticsSettingsView and
// SasayakiSettingsView; the Sasayaki form itself is SasayakiSettingsForm.qml.
// The section rail itself is SettingsSectionRail.qml; its Anki entry reports
// ankiRequested(), which MainWindow turns into the Anki settings page.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Page {
    id: page

    signal backRequested
    signal ankiRequested

    padding: 0
    palette.window: Theme.windowBackground
    palette.windowText: Theme.textPrimary
    palette.base: Theme.popupBackground
    palette.alternateBase: Theme.cardBackground
    palette.text: Theme.textPrimary
    palette.button: Theme.cardBackground
    palette.buttonText: Theme.textPrimary
    palette.mid: Theme.cardStroke
    palette.midlight: Theme.cardStroke
    palette.dark: Theme.textSecondary
    palette.light: Theme.cardBackground
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.popupBackground
    palette.placeholderText: Theme.textSecondary

    background: Rectangle {
        color: Theme.windowBackground
    }

    SettingsViewModel {
        id: settings
    }

    readonly property var interfaceThemes: [
        {
            label: qsTr("System"),
            value: "System"
        },
        {
            label: qsTr("Light"),
            value: "Light"
        },
        {
            label: qsTr("Dark"),
            value: "Dark"
        }
    ]

    component Section: ColumnLayout {
        id: section

        required property string title
        default property alias rows: body.data

        Layout.fillWidth: true
        spacing: Theme.spacingSm

        Label {
            Layout.fillWidth: true
            text: section.title
            color: Theme.textPrimary
            font.pixelSize: Theme.fontTitle
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
        }

        Pane {
            Layout.fillWidth: true
            padding: Theme.spacingMd
            contentItem: ColumnLayout {
                id: body
                spacing: Theme.spacingSm
            }
            background: Rectangle {
                color: Theme.cardBackground
                radius: Theme.cardRadius
                border.color: Theme.cardStroke
                border.width: Theme.cardBorderWidth
            }
        }
    }

    component ToggleRow: SettingRow {
        id: toggleRow

        required property bool checked
        signal edited(bool value)

        Switch {
            Accessible.name: toggleRow.label
            palette.dark: Theme.accent
            checked: toggleRow.checked
            onToggled: toggleRow.edited(checked)
        }
    }

    component ChoiceRow: SettingRow {
        id: choiceRow

        required property var choices
        required property string selected
        signal edited(string value)

        ComboBox {
            id: choice

            Accessible.name: choiceRow.label
            model: choiceRow.choices
            textRole: "label"
            valueRole: "value"
            currentValue: choiceRow.selected
            implicitContentWidthPolicy: ComboBox.WidestText
            onActivated: choiceRow.edited(String(choice.currentValue))
        }
    }

    component IntegerRow: SettingRow {
        id: integerRow

        required property int from
        required property int to
        required property int value
        signal edited(int value)

        SpinBox {
            id: integerInput

            Accessible.name: integerRow.label
            from: integerRow.from
            to: integerRow.to
            stepSize: 1
            editable: true
            value: integerRow.value
            onValueModified: integerRow.edited(integerInput.value)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PageHeader {
            title: qsTr("Settings")
            onBackRequested: page.backRequested()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            SettingsSectionRail {
                id: rail

                selected: settings.section
                onSectionSelected: index => {
                    settings.section = index;
                }
                onAnkiRequested: page.ankiRequested()
            }

            ScrollView {
                id: scroll

                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: Theme.spacingLg
                rightPadding: Theme.spacingXl
                clip: true
                contentWidth: scroll.availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: scroll.availableWidth
                    spacing: Theme.spacingLg

                    SettingsNote {
                        visible: settings.error !== ""
                        color: Theme.popupDanger
                        text: settings.error
                    }

                    Loader {
                        Layout.fillWidth: true
                        sourceComponent: settings.section === 1 ? dictionaryForm : settings.section === 2 ? statisticsForm : settings.section === 3 ? sasayakiForm : readerForm
                        onLoaded: {
                            const flickable = scroll.contentItem as Flickable;
                            if (flickable)
                                flickable.contentY = flickable.originY;
                        }
                    }
                }
            }
        }
    }

    Component {
        id: readerForm

        ColumnLayout {
            spacing: Theme.spacingLg

            SettingsNote {
                text: qsTr("Reader changes take effect the next time you open a book.")
            }

            Section {
                title: qsTr("Appearance")

                ChoiceRow {
                    label: qsTr("Reader theme")
                    choices: page.interfaceThemes.concat([
                        {
                            label: qsTr("Sepia"),
                            value: "Sepia"
                        },
                        {
                            label: qsTr("Custom"),
                            value: "Custom"
                        }
                    ])
                    selected: settings.theme
                    onEdited: value => {
                        settings.theme = value;
                    }
                }

                ChoiceRow {
                    label: qsTr("Interface theme")
                    choices: page.interfaceThemes
                    selected: settings.uiTheme
                    onEdited: value => {
                        settings.uiTheme = value;
                    }
                }

                ToggleRow {
                    visible: settings.theme === "Sepia"
                    label: qsTr("Invert Sepia in Dark Mode")
                    checked: settings.sepiaInvertInDark
                    onEdited: value => {
                        settings.sepiaInvertInDark = value;
                    }
                }

                ToggleRow {
                    visible: settings.theme === "System"
                    label: qsTr("Use Sepia as Light Theme")
                    checked: settings.systemLightSepia
                    onEdited: value => {
                        settings.systemLightSepia = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Vertical writing")
                    checked: settings.verticalWriting
                    onEdited: value => {
                        settings.verticalWriting = value;
                    }
                }

                SettingRow {
                    id: fontRow

                    label: qsTr("Font family")

                    TextField {
                        id: fontInput

                        Layout.preferredWidth: 11 * Theme.spacingLg
                        Accessible.name: fontRow.label
                        text: settings.selectedFont
                        onEditingFinished: settings.selectedFont = fontInput.text
                    }
                }

                IntegerRow {
                    label: qsTr("Font size")
                    from: 16
                    to: 40
                    value: settings.fontSize
                    onEdited: value => {
                        settings.fontSize = value;
                    }
                }

                ChoiceRow {
                    label: qsTr("Furigana")
                    choices: [
                        {
                            label: qsTr("Off"),
                            value: "Off"
                        },
                        {
                            label: qsTr("Dimmed"),
                            value: "Dimmed"
                        },
                        {
                            label: qsTr("Toggle"),
                            value: "Toggle"
                        },
                        {
                            label: qsTr("Hidden"),
                            value: "Hidden"
                        }
                    ]
                    selected: settings.furiganaMode
                    onEdited: value => {
                        settings.furiganaMode = value;
                    }
                }
            }

            Section {
                title: qsTr("Layout")

                ToggleRow {
                    label: qsTr("Continuous scrolling")
                    checked: settings.continuousMode
                    onEdited: value => {
                        settings.continuousMode = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Paragraph mode")
                    enabled: !settings.continuousMode
                    checked: settings.paragraphMode
                    onEdited: value => {
                        settings.paragraphMode = value;
                    }
                }

                ToggleRow {
                    visible: !settings.continuousMode && settings.paragraphMode
                    enabled: visible
                    label: qsTr("Animate Text Reveal")
                    checked: settings.textAnimation
                    onEdited: value => {
                        settings.textAnimation = value;
                    }
                }

                SettingsSliderRow {
                    visible: !settings.continuousMode && settings.paragraphMode && settings.textAnimation
                    label: qsTr("Text Speed")
                    from: 25
                    to: 100
                    stepSize: 5
                    decimals: 0
                    value: settings.textSpeed
                    onEdited: value => {
                        settings.textSpeed = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Two-column horizontal pages")
                    enabled: !settings.continuousMode && !settings.verticalWriting && !settings.paragraphMode
                    checked: settings.twoColumnHorizontalPages
                    onEdited: value => {
                        settings.twoColumnHorizontalPages = value;
                    }
                }

                IntegerRow {
                    label: qsTr("Horizontal padding")
                    from: 0
                    to: 80
                    value: settings.horizontalPadding
                    onEdited: value => {
                        settings.horizontalPadding = value;
                    }
                }

                IntegerRow {
                    label: qsTr("Vertical padding")
                    from: 0
                    to: 50
                    value: settings.verticalPadding
                    onEdited: value => {
                        settings.verticalPadding = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Avoid page break")
                    checked: settings.avoidPageBreak
                    onEdited: value => {
                        settings.avoidPageBreak = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Justify text")
                    checked: settings.justifyText
                    onEdited: value => {
                        settings.justifyText = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Blur images")
                    checked: settings.blurImages
                    onEdited: value => {
                        settings.blurImages = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Advanced layout")
                    checked: settings.layoutAdvanced
                    onEdited: value => {
                        settings.layoutAdvanced = value;
                    }
                }

                SettingsSliderRow {
                    visible: settings.layoutAdvanced
                    label: qsTr("Line height")
                    from: 1.0
                    to: 2.5
                    stepSize: 0.05
                    decimals: 2
                    value: settings.lineHeight
                    onEdited: value => {
                        settings.lineHeight = value;
                    }
                }

                SettingsSliderRow {
                    visible: settings.layoutAdvanced
                    label: qsTr("Character spacing")
                    from: -10
                    to: 10
                    stepSize: 1
                    decimals: 0
                    value: settings.characterSpacing
                    onEdited: value => {
                        settings.characterSpacing = value;
                    }
                }

                SettingsSliderRow {
                    visible: settings.layoutAdvanced
                    label: qsTr("Paragraph spacing")
                    from: 0
                    to: 3
                    stepSize: 0.1
                    decimals: 1
                    value: settings.paragraphSpacing
                    onEdited: value => {
                        settings.paragraphSpacing = value;
                    }
                }
            }

            Section {
                title: qsTr("Progress")

                ToggleRow {
                    label: qsTr("Show title")
                    checked: settings.showTitle
                    onEdited: value => {
                        settings.showTitle = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Show progress")
                    checked: settings.showProgress
                    onEdited: value => {
                        settings.showProgress = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Show chapter progress")
                    checked: settings.showChapterProgress
                    onEdited: value => {
                        settings.showChapterProgress = value;
                    }
                }

                ChoiceRow {
                    label: qsTr("Count")
                    enabled: settings.showProgress || settings.showChapterProgress
                    choices: [
                        {
                            label: qsTr("Off"),
                            value: "Off"
                        },
                        {
                            label: qsTr("Characters"),
                            value: "Characters"
                        },
                        {
                            label: qsTr("Pages"),
                            value: "Pages"
                        }
                    ]
                    selected: settings.progressCount
                    onEdited: value => {
                        settings.progressCount = value;
                    }
                }

                SettingsNote {
                    visible: settings.progressCount === "Pages" && (settings.showProgress || settings.showChapterProgress)
                    text: qsTr("Page counts depend on the layout and window size and are unavailable in continuous mode.")
                }

                ToggleRow {
                    label: qsTr("Show percentage")
                    enabled: settings.showProgress || settings.showChapterProgress
                    checked: settings.showPercentage
                    onEdited: value => {
                        settings.showPercentage = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Show progress at the top")
                    enabled: settings.showProgress || settings.showChapterProgress
                    checked: settings.showProgressTop
                    onEdited: value => {
                        settings.showProgressTop = value;
                    }
                }
            }
        }
    }

    Component {
        id: dictionaryForm

        ColumnLayout {
            spacing: Theme.spacingLg

            Section {
                title: qsTr("Lookup")

                ToggleRow {
                    label: qsTr("Open the dictionary tab by default")
                    checked: settings.dictionaryTabDefault
                    onEdited: value => {
                        settings.dictionaryTabDefault = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Scan non-Japanese text")
                    checked: settings.scanNonJapaneseText
                    onEdited: value => {
                        settings.scanNonJapaneseText = value;
                    }
                }

                IntegerRow {
                    label: qsTr("Maximum results")
                    from: 1
                    to: 50
                    value: settings.maxResults
                    onEdited: value => {
                        settings.maxResults = value;
                    }
                }

                IntegerRow {
                    label: qsTr("Scan length")
                    from: 1
                    to: 64
                    value: settings.scanLength
                    onEdited: value => {
                        settings.scanLength = value;
                    }
                }
            }

            Section {
                title: qsTr("Display")

                ChoiceRow {
                    label: qsTr("Collapse dictionaries")
                    choices: [
                        {
                            label: qsTr("Expand All"),
                            value: "Expand All"
                        },
                        {
                            label: qsTr("Collapse All"),
                            value: "Collapse All"
                        },
                        {
                            label: qsTr("Custom"),
                            value: "Custom"
                        }
                    ]
                    selected: settings.collapseMode
                    onEdited: value => {
                        settings.collapseMode = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Two-column layout")
                    checked: settings.twoColumnLayout
                    onEdited: value => {
                        settings.twoColumnLayout = value;
                    }
                }

                ToggleRow {
                    label: qsTr("Compact glossaries")
                    checked: settings.compactGlossaries
                    onEdited: value => {
                        settings.compactGlossaries = value;
                    }
                }
            }

            Section {
                id: cssSection

                title: qsTr("Custom CSS")

                TextArea {
                    id: cssInput

                    Layout.fillWidth: true
                    Layout.minimumHeight: 6 * Theme.spacingLg
                    Accessible.name: cssSection.title
                    text: settings.customCSS
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    onTextEdited: settings.customCSS = cssInput.text

                    background: Rectangle {
                        color: Theme.popupBackground
                        radius: Theme.controlRadius
                        border.width: Theme.cardBorderWidth
                        border.color: cssInput.activeFocus ? Theme.accent : Theme.cardStroke
                    }
                }
            }
        }
    }

    Component {
        id: sasayakiForm

        SasayakiSettingsForm {}
    }

    Component {
        id: statisticsForm

        ColumnLayout {
            spacing: Theme.spacingLg

            Section {
                title: qsTr("Tracking")

                ToggleRow {
                    label: qsTr("Enable statistics")
                    checked: settings.enableStatistics
                    onEdited: value => {
                        settings.enableStatistics = value;
                    }
                }

                ChoiceRow {
                    label: qsTr("Autostart")
                    enabled: settings.enableStatistics
                    choices: [
                        {
                            label: qsTr("Off"),
                            value: "Off"
                        },
                        {
                            label: qsTr("Page Turn"),
                            value: "Page Turn"
                        },
                        {
                            label: qsTr("On"),
                            value: "On"
                        }
                    ]
                    selected: settings.statisticsAutostartMode
                    onEdited: value => {
                        settings.statisticsAutostartMode = value;
                    }
                }
            }

            Section {
                title: qsTr("Daily reset")

                IntegerRow {
                    label: qsTr("Hour (24-hour clock)")
                    from: 0
                    to: 23
                    value: Math.floor(settings.statisticsResetTime / 60)
                    onEdited: value => {
                        settings.statisticsResetTime = value * 60 + settings.statisticsResetTime % 60;
                    }
                }

                IntegerRow {
                    label: qsTr("Minute")
                    from: 0
                    to: 59
                    value: settings.statisticsResetTime % 60
                    onEdited: value => {
                        settings.statisticsResetTime = Math.floor(settings.statisticsResetTime / 60) * 60 + value;
                    }
                }

                SettingsNote {
                    text: qsTr("Reading before this local time counts toward the previous day. This does not change your reading goals.")
                }
            }
        }
    }
}

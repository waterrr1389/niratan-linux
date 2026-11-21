// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Anki settings page, following Features/Settings/AnkiView.swift (body,
// connectionStatus, ankiConfigToggle, duplicateScopeText, the onAppear and
// onDisappear lifecycle and the alerts in AnkiSettingsAlerts.qml),
// AnkiConnectView.swift (the masked API Key row) and
// NativeMac/NativeReuseViews.swift (NativeSettingsForm, NativeSettingsSectionCard
// including its footer, NativeSettingsRow, NativeSettingsToggle,
// NativeGlassSegmentedPicker with a minimum segment width of 82,
// NativeGlassMenuPicker with a minimum width of 160, and the field row in
// AnkiFieldRow.qml).
//
// The AnkiConnect card always shows Address, the masked API key, the
// "Status: Connected"/"Status: Not connected" line and Connect. Refresh,
// AnkiConnect Settings, Config and Settings exist only while connected; Fields
// also needs a selected note type among the available ones, so both conditions
// create the cards instead of hiding them, as Swift's `if` does. Every edit
// saves at once through the view model, and leaving the page saves once more.
//
// Out of scope: audio and image compression rows, the AVIF rows, Force Sync on
// adding card and Embed Media. The AnkiMobile footer sentence is dropped and
// the card-creation footer loses "Mac" (Decisions accepted).

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Page {
    id: page

    signal backRequested

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

    AnkiSettingsViewModel {
        id: settings
    }

    // AnkiView.swift onAppear loads the profile and pings; onDisappear saves.
    Component.onCompleted: settings.open()
    Component.onDestruction: settings.close()

    // AnkiView.swift duplicateScopeText.
    function scopeLabel(value) {
        switch (value) {
        case "deck":
            return qsTr("Deck");
        case "deckroot":
            return qsTr("Deck Root");
        default:
            return qsTr("Collection");
        }
    }

    component Section: ColumnLayout {
        id: section

        required property string title
        property string footer: ""
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

        Label {
            Layout.fillWidth: true
            visible: section.footer.length > 0
            text: section.footer
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            wrapMode: Text.WordWrap
        }
    }

    component Separator: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Theme.cardStroke
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

    // NativeGlassSegmentedPicker segment: the selected value carries the glass
    // fill, the others read as secondary text in the shared capsule.
    component Segment: Button {
        id: segment

        Layout.minimumWidth: 82
        leftPadding: 10
        rightPadding: 10
        topPadding: 4
        bottomPadding: 4
        Accessible.selected: segment.checked

        contentItem: Label {
            text: segment.text
            color: segment.checked ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: Theme.controlRadius
            color: segment.checked ? Theme.cardBackground : "transparent"
            border.width: segment.checked ? Theme.cardBorderWidth : 0
            border.color: Theme.cardStroke
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PageHeader {
            title: qsTr("Anki")
            onBackRequested: page.backRequested()
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

                Section {
                    title: qsTr("AnkiConnect")
                    footer: qsTr("Card creation uses AnkiConnect.")

                    SettingRow {
                        label: qsTr("Address")

                        TextField {
                            id: addressInput

                            Layout.preferredWidth: 360
                            placeholderText: qsTr("Address")
                            selectByMouse: true
                            text: settings.address
                            onTextEdited: settings.address = addressInput.text

                            Connections {
                                target: settings

                                function onSettingsChanged() {
                                    if (addressInput.text !== settings.address)
                                        addressInput.text = settings.address;
                                }
                            }
                        }
                    }

                    Separator {
                    }

                    SettingRow {
                        label: qsTr("API Key (optional)")

                        TextField {
                            id: apiKeyInput

                            Layout.preferredWidth: 360
                            placeholderText: qsTr("API Key (optional)")
                            selectByMouse: true
                            echoMode: TextInput.Password
                            text: settings.apiKey
                            onTextEdited: settings.apiKey = apiKeyInput.text
                        }
                    }

                    Separator {
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd

                        Label {
                            Layout.fillWidth: true
                            text: settings.connected ? qsTr("Status: Connected")
                                                     : qsTr("Status: Not connected")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.Wrap
                        }

                        Button {
                            text: qsTr("Connect")
                            onClicked: settings.connectToAnki()
                        }
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    active: settings.connected
                    sourceComponent: connectedSections
                }
            }
        }
    }

    AnkiSettingsAlerts {
        id: alerts

        anchors.fill: parent
        errorText: settings.errorMessage
        onFetchRequested: settings.fetchMetadata()
        onDefaultsRequested: settings.applyDefaults()
        onErrorDismissed: settings.dismissError()
    }

    Component {
        id: connectedSections

        ColumnLayout {
            spacing: Theme.spacingLg

            Section {
                title: qsTr("Refresh")
                footer: qsTr("Fetching refreshes decks and models while preserving mappings for fields that still exist.")

                Button {
                    text: qsTr("Fetch decks and models from AnkiConnect")
                    onClicked: alerts.openFetch()
                }
            }

            Section {
                title: qsTr("AnkiConnect Settings")

                SettingRow {
                    label: qsTr("Duplicate Scope")

                    Pane {
                        padding: 2

                        contentItem: RowLayout {
                            spacing: 0

                            Repeater {
                                model: ["collection", "deck", "deckroot"]

                                delegate: Segment {
                                    id: scopeSegment

                                    required property string modelData
                                    readonly property bool segmentSelected: settings.duplicateScope === scopeSegment.modelData

                                    text: page.scopeLabel(scopeSegment.modelData)
                                    checked: scopeSegment.segmentSelected
                                    onClicked: settings.duplicateScope = scopeSegment.modelData
                                }
                            }
                        }

                        background: Rectangle {
                            color: Theme.windowBackground
                            radius: Theme.controlRadius
                            border.width: Theme.cardBorderWidth
                            border.color: Theme.cardStroke
                        }
                    }
                }

                Separator {
                }

                ToggleRow {
                    label: qsTr("Check All Models")
                    checked: settings.checkAllModels
                    onEdited: value => {
                        settings.checkAllModels = value;
                    }
                }
            }

            Section {
                title: qsTr("Config")

                SettingRow {
                    label: qsTr("Deck")

                    ComboBox {
                        id: deckBox

                        Layout.preferredWidth: 160
                        Accessible.name: qsTr("Deck")
                        model: settings.availableDecks
                        currentIndex: settings.availableDecks.indexOf(settings.selectedDeck)
                        displayText: settings.selectedDeck.length > 0 ? settings.selectedDeck
                                                                      : qsTr("Deck")
                        onActivated: settings.selectedDeck = settings.availableDecks[deckBox.currentIndex]
                    }
                }

                Separator {
                }

                SettingRow {
                    label: qsTr("Model")

                    ComboBox {
                        id: modelBox

                        Layout.preferredWidth: 160
                        Accessible.name: qsTr("Model")
                        model: settings.availableNoteTypes
                        currentIndex: settings.availableNoteTypes.indexOf(settings.selectedNoteType)
                        displayText: settings.selectedNoteType.length > 0 ? settings.selectedNoteType
                                                                          : qsTr("Model")
                        onActivated: settings.selectedNoteType = settings.availableNoteTypes[modelBox.currentIndex]
                    }
                }
            }

            Section {
                title: qsTr("Settings")
                footer: qsTr("Higher quality produces larger cards. Image compression applies to book covers and video screenshots; repeated cards reuse matching media files.")

                ToggleRow {
                    label: qsTr("Allow Duplicates")
                    checked: settings.allowDuplicates
                    onEdited: value => {
                        settings.allowDuplicates = value;
                    }
                }

                Separator {
                }

                ToggleRow {
                    label: qsTr("Compact Glossaries")
                    checked: settings.compactGlossaries
                    onEdited: value => {
                        settings.compactGlossaries = value;
                    }
                }

                Separator {
                }

                ToggleRow {
                    label: qsTr("Compress Images")
                    checked: settings.compressImages
                    onEdited: value => {
                        settings.compressImages = value;
                    }
                }
            }

            Loader {
                Layout.fillWidth: true
                active: settings.hasSelectedNoteType
                sourceComponent: fieldsSection
            }
        }
    }

    Component {
        id: fieldsSection

        Section {
            title: qsTr("Fields")

            Button {
                visible: settings.hasFieldDefaults
                text: qsTr("Apply Defaults")
                onClicked: alerts.openDefaults()
            }

            Separator {
                visible: settings.hasFieldDefaults
            }

            Repeater {
                model: settings.fields

                delegate: ColumnLayout {
                    id: fieldSection

                    required property string modelData
                    required property int index

                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    Separator {
                        visible: fieldSection.index > 0
                    }

                    AnkiFieldRow {
                        Layout.fillWidth: true
                        field: fieldSection.modelData
                        mapping: settings.fieldMappings[fieldSection.modelData] ?? ""
                        options: settings.mappingOptions
                        onEdited: text => settings.setFieldMapping(fieldSection.modelData, text)
                        onOptionChosen: option => settings.setFieldMapping(fieldSection.modelData, option)
                    }
                }
            }

            Separator {
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Tags")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }

                TextField {
                    id: tagsInput

                    Layout.fillWidth: true
                    placeholderText: qsTr("None")
                    selectByMouse: true
                    text: settings.tags
                    onTextEdited: settings.tags = tagsInput.text
                }
            }
        }
    }
}

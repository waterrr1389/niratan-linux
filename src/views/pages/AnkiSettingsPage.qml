// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/AnkiView.swift: body, duplicateScopeText and its open/save
// lifecycle; AnkiConnectView.swift: the masked API key.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

SettingsForm {
    id: page

    objectName: "settings-anki"
    readonly property list<string> scopes: ["collection", "deck", "deckroot"]
    readonly property real addressFieldWidth: Math.min(360, Math.max(0, cardWidth - 32) * 0.65)

    AnkiSettingsViewModel {
        id: settings
    }

    Component.onCompleted: settings.open()
    Component.onDestruction: settings.close()

    component ConfigPicker: ComboBox {
        id: picker

        readonly property real listHeight: popup.implicitContentHeight + popup.topPadding + popup.bottomPadding

        Layout.preferredWidth: 160
        // The list must scroll even when opened near the bottom of the window.
        popup.height: Math.min(picker.listHeight,
                               Math.max(0, (picker.Window.window?.height ?? picker.listHeight)
                                        - popup.topMargin - popup.bottomMargin))
    }

    SettingsCard {
        title: qsTr("AnkiConnect")
        footer: qsTr("Card creation uses AnkiConnect.")

        SettingRow {
            id: addressRow
            label: qsTr("Address")

            TextField {
                id: addressInput

                Layout.preferredWidth: page.addressFieldWidth
                Accessible.name: addressRow.label
                placeholderText: addressRow.label
                selectByMouse: true
                text: settings.address
                onTextEdited: settings.address = text

                Connections {
                    target: settings

                    function onSettingsChanged() {
                        if (addressInput.text !== settings.address)
                            addressInput.text = settings.address
                    }
                }
            }
        }

        SettingsSeparator {}
        SettingRow {
            id: keyRow
            label: qsTr("API Key (optional)")

            TextField {
                Layout.preferredWidth: page.addressFieldWidth
                Accessible.name: keyRow.label
                placeholderText: keyRow.label
                selectByMouse: true
                echoMode: TextInput.Password
                text: settings.apiKey
                onTextEdited: settings.apiKey = text
            }
        }

        SettingsSeparator {}
        SettingRow {
            label: settings.connected ? qsTr("Status: Connected") : qsTr("Status: Not connected")
            labelColor: Theme.textSecondary

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

    AnkiSettingsAlerts {
        id: alerts

        parent: page
        anchors.fill: parent
        errorText: settings.errorMessage
        onFetchRequested: settings.fetchMetadata()
        onDefaultsRequested: settings.applyDefaults()
        onErrorDismissed: settings.dismissError()
    }

    Component {
        id: connectedSections

        ColumnLayout {
            spacing: 22

            SettingsCard {
                title: qsTr("Refresh")
                footer: qsTr("Fetching refreshes decks and models while preserving mappings for fields that still exist.")

                SettingsActions {
                    Button {
                        text: qsTr("Fetch decks and models from AnkiConnect")
                        onClicked: alerts.openFetch()
                    }
                }
            }

            SettingsCard {
                title: qsTr("AnkiConnect Settings")

                SettingRow {
                    id: scopeRow
                    label: qsTr("Duplicate Scope")

                    SettingsSegments {
                        Accessible.name: scopeRow.label
                        labels: [qsTr("Collection"), qsTr("Deck"), qsTr("Deck Root")]
                        minimumSegmentWidth: 82
                        selectedIndex: page.scopes.indexOf(settings.duplicateScope)
                        onEdited: index => settings.duplicateScope = page.scopes[index]
                    }
                }

                SettingsSeparator {}
                SettingsToggle {
                    label: qsTr("Check All Models")
                    checked: settings.checkAllModels
                    onEdited: value => settings.checkAllModels = value
                }
            }

            SettingsCard {
                title: qsTr("Config")

                SettingRow {
                    id: deckRow
                    label: qsTr("Deck")

                    ConfigPicker {
                        Accessible.name: deckRow.label
                        model: settings.availableDecks
                        currentIndex: settings.availableDecks.indexOf(settings.selectedDeck)
                        displayText: settings.selectedDeck.length > 0 ? settings.selectedDeck : deckRow.label
                        onActivated: index => settings.selectedDeck = settings.availableDecks[index]
                    }
                }

                SettingsSeparator {}
                SettingRow {
                    id: modelRow
                    label: qsTr("Model")

                    ConfigPicker {
                        Accessible.name: modelRow.label
                        model: settings.availableNoteTypes
                        currentIndex: settings.availableNoteTypes.indexOf(settings.selectedNoteType)
                        displayText: settings.selectedNoteType.length > 0 ? settings.selectedNoteType : modelRow.label
                        onActivated: index => settings.selectedNoteType = settings.availableNoteTypes[index]
                    }
                }
            }

            SettingsCard {
                title: qsTr("Settings")

                SettingsToggle {
                    label: qsTr("Allow Duplicates")
                    checked: settings.allowDuplicates
                    onEdited: value => settings.allowDuplicates = value
                }

                SettingsSeparator {}
                SettingsToggle {
                    label: qsTr("Compact Glossaries")
                    checked: settings.compactGlossaries
                    onEdited: value => settings.compactGlossaries = value
                }

                SettingsSeparator {}
                SettingsToggle {
                    label: qsTr("Compress Images")
                    checked: settings.compressImages
                    onEdited: value => settings.compressImages = value
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

        SettingsCard {
            title: qsTr("Fields")

            SettingsActions {
                visible: settings.hasFieldDefaults

                Button {
                    text: qsTr("Apply Defaults")
                    onClicked: alerts.openDefaults()
                }
            }

            SettingsSeparator { visible: settings.hasFieldDefaults }

            Repeater {
                model: settings.fields

                delegate: ColumnLayout {
                    id: fieldSection

                    required property string modelData
                    required property int index

                    Layout.fillWidth: true
                    spacing: 0

                    SettingsSeparator { visible: fieldSection.index > 0 }

                    AnkiFieldRow {
                        field: fieldSection.modelData
                        mapping: settings.fieldMappings[fieldSection.modelData] ?? ""
                        options: settings.mappingOptions
                        onEdited: text => settings.setFieldMapping(fieldSection.modelData, text)
                        onOptionChosen: option => settings.setFieldMapping(fieldSection.modelData, option)
                    }
                }
            }

            SettingsSeparator { visible: settings.fields.length > 0 }

            AnkiFieldRow {
                field: qsTr("Tags")
                mapping: settings.tags
                options: []
                showMenu: false
                onEdited: text => settings.tags = text
            }
        }
    }
}

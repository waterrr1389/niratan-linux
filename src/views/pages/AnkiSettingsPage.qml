// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/AnkiView.swift: body, duplicateScopeText and its open/save
// lifecycle; AnkiConnectView.swift: the masked API key.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

PanelScaffold {
    id: page

    objectName: "settings-anki"
    readonly property list<string> scopes: ["collection", "deck", "deckroot"]

    AnkiSettingsViewModel {
        id: settings
    }

    Component.onCompleted: settings.open()
    Component.onDestruction: settings.close()

    component ConfigPicker: FormPickerRow {
        constrainMenuHeight: true
    }

    component MappingRow: FormRow {
        id: mappingRow

        required property string field
        required property string mapping
        required property var options
        property bool showMenu: true

        signal edited(string text)
        signal optionChosen(string option)

        fillsRow: true

        Column {
            width: mappingRow.width - 2 * Theme.fillRowPaddingHorizontal
            spacing: Theme.spacingXs

            StyledLabel {
                width: parent.width
                text: mappingRow.field
                textStyle: Theme.footnoteFont
                secondary: true
                wrapMode: Text.Wrap
            }

            Row {
                id: inputs

                width: parent.width
                spacing: Theme.spacingXs

                TextField {
                    anchors.verticalCenter: parent.verticalCenter
                    width: inputs.width - (menuButton.visible ? menuButton.width + inputs.spacing : 0)
                    Accessible.name: mappingRow.field
                    placeholderText: qsTr("None")
                    selectByMouse: true
                    text: mappingRow.mapping
                    onTextEdited: mappingRow.edited(text)
                }

                GlyphButton {
                    id: menuButton

                    anchors.verticalCenter: parent.verticalCenter
                    visible: mappingRow.showMenu
                    glyph: "chevron.down"
                    Accessible.name: qsTr("Fields")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Fields")
                    onClicked: optionMenu.popup(menuButton)

                    Menu {
                        id: optionMenu

                        MenuItem {
                            text: "-"
                            onTriggered: mappingRow.optionChosen("")
                        }

                        MenuSeparator {}

                        Repeater {
                            model: mappingRow.options

                            delegate: MenuItem {
                                required property string modelData

                                text: modelData
                                onTriggered: mappingRow.optionChosen(modelData)
                            }
                        }
                    }
                }
            }
        }
    }

    FormSection {
        title: qsTr("AnkiConnect")
        footer: qsTr("Card creation uses AnkiConnect.")

        FormRow {
            id: addressRow
            label: qsTr("Address")

            TextField {
                id: addressInput

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

        FormDivider {}
        FormRow {
            id: keyRow
            label: qsTr("API Key (optional)")

            TextField {
                Accessible.name: keyRow.label
                placeholderText: keyRow.label
                selectByMouse: true
                echoMode: TextInput.Password
                text: settings.apiKey
                onTextEdited: settings.apiKey = text
            }
        }

        FormDivider {}
        FormRow {
            id: statusRow

            label: settings.connected ? qsTr("Status: Connected") : qsTr("Status: Not connected")

            Binding {
                target: statusRow.labelItem
                property: "secondary"
                value: true
            }

            FormButton {
                text: qsTr("Connect")
                onClicked: settings.connectToAnki()
            }
        }
    }

    Loader {
        height: implicitHeight
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

        Column {
            spacing: Theme.formSpacing

            FormSection {
                title: qsTr("Refresh")
                footer: qsTr("Fetching refreshes decks and models while preserving mappings for fields that still exist.")

                FormButtonRow {
                    FormButton {
                        text: qsTr("Fetch decks and models from AnkiConnect")
                        onClicked: alerts.openFetch()
                    }
                }
            }

            FormSection {
                title: qsTr("AnkiConnect Settings")

                FormSegmentedRow {
                    label: qsTr("Duplicate Scope")
                    model: [qsTr("Collection"), qsTr("Deck"), qsTr("Deck Root")]
                    currentIndex: page.scopes.indexOf(settings.duplicateScope)
                    onActivated: index => settings.duplicateScope = page.scopes[index]
                }

                FormDivider {}
                FormToggleRow {
                    label: qsTr("Check All Models")
                    checked: settings.checkAllModels
                    onToggled: value => settings.checkAllModels = value
                }
            }

            FormSection {
                title: qsTr("Config")

                ConfigPicker {
                    label: qsTr("Deck")
                    model: settings.availableDecks
                    currentIndex: settings.availableDecks.indexOf(settings.selectedDeck)
                    placeholderText: settings.selectedDeck.length > 0 ? settings.selectedDeck : label
                    onActivated: index => settings.selectedDeck = settings.availableDecks[index]
                }

                FormDivider {}
                ConfigPicker {
                    label: qsTr("Model")
                    model: settings.availableNoteTypes
                    currentIndex: settings.availableNoteTypes.indexOf(settings.selectedNoteType)
                    placeholderText: settings.selectedNoteType.length > 0 ? settings.selectedNoteType : label
                    onActivated: index => settings.selectedNoteType = settings.availableNoteTypes[index]
                }
            }

            FormSection {
                title: qsTr("Settings")

                FormToggleRow {
                    label: qsTr("Allow Duplicates")
                    checked: settings.allowDuplicates
                    onToggled: value => settings.allowDuplicates = value
                }

                FormDivider {}
                FormToggleRow {
                    label: qsTr("Compact Glossaries")
                    checked: settings.compactGlossaries
                    onToggled: value => settings.compactGlossaries = value
                }

                FormDivider {}
                FormToggleRow {
                    label: qsTr("Compress Images")
                    checked: settings.compressImages
                    onToggled: value => settings.compressImages = value
                }
            }

            Loader {
                width: parent.width
                active: settings.hasSelectedNoteType
                sourceComponent: fieldsSection
            }
        }
    }

    Component {
        id: fieldsSection

        FormSection {
            title: qsTr("Fields")

            FormButtonRow {
                visible: settings.hasFieldDefaults

                FormButton {
                    text: qsTr("Apply Defaults")
                    onClicked: alerts.openDefaults()
                }
            }

            FormDivider { visible: settings.hasFieldDefaults }

            Repeater {
                model: settings.fields

                delegate: Column {
                    id: fieldSection

                    required property string modelData
                    required property int index

                    width: parent ? parent.width : implicitWidth

                    FormDivider { visible: fieldSection.index > 0 }

                    MappingRow {
                        field: fieldSection.modelData
                        mapping: settings.fieldMappings[fieldSection.modelData] ?? ""
                        options: settings.mappingOptions
                        onEdited: text => settings.setFieldMapping(fieldSection.modelData, text)
                        onOptionChosen: option => settings.setFieldMapping(fieldSection.modelData, option)
                    }
                }
            }

            FormDivider { visible: settings.fields.length > 0 }

            MappingRow {
                field: qsTr("Tags")
                mapping: settings.tags
                options: []
                showMenu: false
                onEdited: text => settings.tags = text
            }
        }
    }
}

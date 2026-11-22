// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/DictionaryView.swift: body, DictionaryBehaviorSettingsSections,
// dictionaryTypePicker and DictionaryCategoryPicker.
// Local imports are synchronous; recommendations are read-only and reordering
// uses menu actions. The library does not expose revision captions.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Niratan
import "../icons/icon_map.js" as IconMap

SettingsForm {
    id: page

    objectName: "settings-dictionaries"
    readonly property list<string> types: ["Term", "Frequency", "Pitch", "Kanji"]
    readonly property list<string> categories: ["none", "monolingual", "bilingual", "exclude"]
    readonly property list<string> collapseModes: ["Expand All", "Collapse All", "Custom"]

    SettingsViewModel {
        id: preferences
    }

    DictionarySettingsViewModel {
        id: library
        Component.onCompleted: library.refresh()
    }

    FileDialog {
        id: importDialog

        title: qsTr("Import Dictionaries")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Yomitan dictionaries (*.zip)")]
        onAccepted: library.importFiles(selectedFiles)
    }

    Dialog {
        id: cssDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(580, parent ? parent.width : 580)
        modal: true
        title: qsTr("Custom CSS")
        onAboutToShow: cssInput.text = preferences.customCSS
        onClosed: preferences.customCSS = cssInput.text

        contentItem: ScrollView {
            implicitHeight: 240
            contentWidth: availableWidth
            clip: true

            TextArea {
                id: cssInput

                Accessible.name: cssDialog.title
                font.family: "monospace"
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
            }
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Reset")
                DialogButtonBox.buttonRole: DialogButtonBox.ResetRole
                onClicked: cssInput.clear()
            }

            Button {
                text: qsTr("Close")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: cssDialog.close()
            }
        }
    }

    Dialog {
        id: recommendedDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(480, parent ? parent.width : 480)
        modal: true
        title: qsTr("Recommended for this profile")
        standardButtons: Dialog.Close

        contentItem: ColumnLayout {
            SettingsNote {
                text: qsTr("Use Import to install a local .zip of one of these dictionaries.")
            }

            Repeater {
                model: library.recommended

                delegate: RowLayout {
                    id: recommendation

                    required property var modelData

                    Label {
                        Layout.fillWidth: true
                        text: recommendation.modelData.name
                        wrapMode: Text.Wrap
                    }

                    SettingsNote {
                        Layout.fillWidth: false
                        text: recommendation.modelData.type
                    }
                }
            }
        }
    }

    SettingsNote {
        visible: library.error.length > 0 || preferences.error.length > 0
        text: [library.error, preferences.error].filter(message => message.length > 0).join("\n")
        color: Theme.popupDanger
    }

    SettingsCard {
        title: qsTr("Dictionaries")

        SettingsActions {
            Button {
                objectName: "recommendedDictionariesButton"
                text: qsTr("Recommended Dictionaries")
                enabled: library.recommended.length > 0
                onClicked: recommendedDialog.open()
            }

            Button {
                text: qsTr("Import")
                icon.source: IconMap.source("plus")
                onClicked: importDialog.open()
            }

            Button {
                objectName: "dictionaryCssButton"
                text: qsTr("Custom CSS")
                icon.source: IconMap.source("curlybraces")
                onClicked: cssDialog.open()
            }
        }

        SettingsSeparator {}
        SettingRow {
            label: qsTr("Supported Formats")

            SettingsNote {
                Layout.preferredWidth: 220
                Layout.minimumWidth: 0
                horizontalAlignment: Text.AlignRight
                text: qsTr("Yomitan term, frequency, pitch and kanji dictionaries (.zip) are supported")
            }
        }
    }

    SettingsCard {
        title: qsTr("Settings")

        SettingsToggle {
            label: qsTr("Default to Dictionary Tab")
            checked: preferences.dictionaryTabDefault
            onEdited: value => preferences.dictionaryTabDefault = value
        }
    }

    SettingsCard {
        title: qsTr("Lookup")

        SettingsToggle {
            label: qsTr("Scan Non-Japanese Text")
            checked: preferences.scanNonJapaneseText
            onEdited: value => preferences.scanNonJapaneseText = value
        }

        SettingsSeparator {}
        SettingsInteger {
            label: qsTr("Max Results")
            from: 1
            to: 50
            value: preferences.maxResults
            onEdited: value => preferences.maxResults = value
        }

        SettingsSeparator {}
        SettingsInteger {
            label: qsTr("Scan Length")
            from: 1
            to: 64
            value: preferences.scanLength
            onEdited: value => preferences.scanLength = value
        }
    }

    SettingsCard {
        title: qsTr("Collapse Dictionaries")

        SettingRow {
            id: collapseRow
            label: qsTr("Mode")

            SettingsSegments {
                Accessible.name: collapseRow.label
                labels: [qsTr("Expand All"), qsTr("Collapse All"), qsTr("Custom")]
                minimumSegmentWidth: 82
                selectedIndex: page.collapseModes.indexOf(preferences.collapseMode)
                onEdited: index => preferences.collapseMode = page.collapseModes[index]
            }
        }
    }

    SettingsCard {
        title: qsTr("Behaviour")
        footer: qsTr("Two-column layout is recommended only on larger screens or when using full-width.")

        SettingsToggle {
            label: qsTr("Two-Column Layout")
            checked: preferences.twoColumnLayout
            onEdited: value => preferences.twoColumnLayout = value
        }

        SettingsSeparator {}
        SettingsToggle {
            label: qsTr("Compact Glossaries")
            checked: preferences.compactGlossaries
            onEdited: value => preferences.compactGlossaries = value
        }
    }

    SettingsCard {
        title: qsTr("Dictionaries")
        footer: library.dictionaryType === "Term" && library.rows.length > 0
                ? qsTr("Category for Anki definition fields") : ""
        headerControls: SettingsSegments {
            Accessible.name: qsTr("Dictionary Type")
            labels: [qsTr("Term"), qsTr("Frequency"), qsTr("Pitch"), qsTr("Kanji")]
            minimumSegmentWidth: 72
            selectedIndex: page.types.indexOf(library.dictionaryType)
            onEdited: index => library.selectType(page.types[index])
        }

        Repeater {
            model: library.rows

            delegate: ColumnLayout {
                id: entry

                required property var modelData
                required property int index

                Layout.fillWidth: true
                spacing: 0

                SettingsSeparator { visible: entry.index > 0 }

                SettingRow {
                    label: entry.modelData.title

                    ComboBox {
                        visible: library.dictionaryType === "Term"
                        Accessible.name: qsTr("Category")
                        model: [qsTr("No Category"), qsTr("Monolingual"), qsTr("Bilingual"), qsTr("Exclude")]
                        currentIndex: page.categories.indexOf(entry.modelData.category)
                        onActivated: index => library.setCategory(entry.modelData.title, page.categories[index])
                    }

                    Switch {
                        Accessible.name: entry.modelData.title
                        checked: entry.modelData.enabled
                        onToggled: library.setEnabled(entry.modelData.title, checked)
                    }

                    ToolButton {
                        id: actions

                        Accessible.name: qsTr("Dictionary Actions")
                        text: "\u22EF"
                        onClicked: actionsMenu.popup(actions)

                        Menu {
                            id: actionsMenu

                            MenuItem {
                                text: qsTr("Move Up")
                                enabled: entry.index > 0
                                onTriggered: library.move(entry.modelData.title, library.rows[entry.index - 1].title)
                            }

                            MenuItem {
                                text: qsTr("Move Down")
                                enabled: entry.index + 1 < library.rows.length
                                onTriggered: library.move(entry.modelData.title, library.rows[entry.index + 1].title)
                            }

                            MenuSeparator {}

                            MenuItem {
                                text: qsTr("Delete")
                                onTriggered: library.remove(entry.modelData.title)
                            }
                        }
                    }
                }
            }
        }
    }
}

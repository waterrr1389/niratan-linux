// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/DictionaryView.swift: body, DictionaryBehaviorSettingsSections,
// dictionaryTypePicker and DictionaryCategoryPicker.
// Local imports are synchronous and recommendations are read-only. Rows
// reorder by dragging their handle (dictionaryReorderGesture), and the row
// menu keeps Move Up and Move Down for the keyboard. The library does not
// expose revision captions.
//
// The Lookup card follows DictionaryBehaviorSettingsSections in order:
// Frequency Sorting writes one of LookupFrequencySortOrder's raw strings, and
// while the order is not Auto a Frequency Dictionary picker and the ranking
// footer appear. The picker lists None first when the stored name is not an
// enabled title, carrying the stored name so choosing it changes nothing, then
// the enabled titles in resolved order. The Search Text card's Text Size row
// writes the resolved searchTextSize.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Niratan

PanelScaffold {
    id: page

    objectName: "settings-dictionaries"
    readonly property list<string> types: ["Term", "Frequency", "Pitch", "Kanji"]
    readonly property list<string> categories: ["none", "monolingual", "bilingual", "exclude"]
    readonly property list<string> collapseModes: ["Expand All", "Collapse All", "Custom"]
    readonly property list<string> frequencySortOrders: ["Auto", "Ascending", "Descending"]
    readonly property var frequencySortLabels: [qsTr("Auto"), qsTr("Ascending"), qsTr("Descending")]
    readonly property bool frequencyDictionaryUnknown:
        library.frequencyDictionaryTitles.indexOf(preferences.frequencySortDictionary) < 0
    readonly property var frequencyDictionaryValues:
        (frequencyDictionaryUnknown ? [preferences.frequencySortDictionary] : [])
            .concat(library.frequencyDictionaryTitles)
    readonly property var frequencyDictionaryLabels:
        (frequencyDictionaryUnknown ? [qsTr("None")] : []).concat(library.frequencyDictionaryTitles)

    SettingsViewModel {
        id: preferences
    }

    DictionarySettingsViewModel {
        id: library
        Component.onCompleted: library.openAsync()
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
        width: Math.min(Theme.wideDialogWidth, parent ? parent.width : Theme.wideDialogWidth)
        modal: true
        title: qsTr("Custom CSS")
        onAboutToShow: cssInput.text = preferences.customCSS
        onClosed: preferences.customCSS = cssInput.text

        contentItem: ScrollView {
            implicitHeight: Theme.dialogEditorHeight
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
        width: Math.min(Theme.dialogWidth, parent ? parent.width : Theme.dialogWidth)
        modal: true
        title: qsTr("Recommended for this profile")
        standardButtons: Dialog.Close

        contentItem: ColumnLayout {
            StyledLabel {
                Layout.fillWidth: true
                text: qsTr("Use Import to install a local .zip of one of these dictionaries.")
                textStyle: Theme.footnoteFont
                secondary: true
                wrapMode: Text.Wrap
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

                    StyledLabel {
                        text: recommendation.modelData.type
                        textStyle: Theme.footnoteFont
                        secondary: true
                    }
                }
            }
        }
    }

    StyledLabel {
        visible: library.error.length > 0 || preferences.error.length > 0
        text: [library.error, preferences.error].filter(message => message.length > 0).join("\n")
        color: Theme.popupDanger
        wrapMode: Text.Wrap
    }

    FormSection {
        title: qsTr("Dictionaries")

        FormButtonRow {
            FormButton {
                objectName: "recommendedDictionariesButton"
                text: qsTr("Recommended Dictionaries")
                enabled: library.recommended.length > 0
                onClicked: recommendedDialog.open()
            }

            FormButton {
                text: qsTr("Import")
                glyph: "plus"
                onClicked: importDialog.open()
            }

            FormButton {
                objectName: "dictionaryCssButton"
                text: qsTr("Custom CSS")
                glyph: "curlybraces"
                onClicked: cssDialog.open()
            }
        }

        FormDivider {}
        FormRow {
            label: qsTr("Supported Formats")
            subtitle: qsTr("Yomitan term, frequency, pitch and kanji dictionaries (.zip) are supported")
        }
    }

    FormSection {
        title: qsTr("Settings")

        FormToggleRow {
            label: qsTr("Default to Dictionary Tab")
            checked: preferences.dictionaryTabDefault
            onToggled: value => preferences.dictionaryTabDefault = value
        }
    }

    FormSection {
        title: qsTr("Lookup")
        footer: preferences.frequencySortOrder !== "Auto"
                ? qsTr("Results with the same match are ranked by the selected frequency dictionary. Results without a value are listed last.") : ""

        FormToggleRow {
            label: qsTr("Scan Non-Japanese Text")
            checked: preferences.scanNonJapaneseText
            onToggled: value => preferences.scanNonJapaneseText = value
        }

        FormDivider {}
        FormStepperRow {
            label: qsTr("Max Results")
            editable: true
            from: 1
            to: 50
            value: preferences.maxResults
            valueText: String(value)
            onStepped: value => preferences.maxResults = value
        }

        FormDivider {}
        FormStepperRow {
            label: qsTr("Scan Length")
            editable: true
            from: 1
            to: 64
            value: preferences.scanLength
            valueText: String(value)
            onStepped: value => preferences.scanLength = value
        }

        FormDivider {}
        FormPickerRow {
            controlName: "frequencySortPicker"
            label: qsTr("Frequency Sorting")
            model: page.frequencySortLabels
            currentIndex: Math.max(0, page.frequencySortOrders.indexOf(preferences.frequencySortOrder))
            onActivated: index => preferences.frequencySortOrder = page.frequencySortOrders[index]
        }

        Loader {
            width: parent ? parent.width : 0
            active: preferences.frequencySortOrder !== "Auto"

            sourceComponent: Column {
                FormDivider {}
                FormPickerRow {
                    controlName: "frequencyDictionaryPicker"
                    label: qsTr("Frequency Dictionary")
                    model: page.frequencyDictionaryLabels
                    currentIndex: Math.max(0, page.frequencyDictionaryValues.indexOf(preferences.frequencySortDictionary))
                    onActivated: index => preferences.frequencySortDictionary = page.frequencyDictionaryValues[index]
                }
            }
        }
    }

    FormSection {
        title: qsTr("Search Text")

        FormStepperRow {
            objectName: "searchTextSizeRow"
            label: qsTr("Text Size")
            editable: true
            from: 12
            to: 48
            value: preferences.searchTextSize
            valueText: String(value)
            onStepped: value => preferences.searchTextSize = value
        }
    }

    FormSection {
        title: qsTr("Collapse Dictionaries")

        FormSegmentedRow {
            label: qsTr("Mode")
            model: [qsTr("Expand All"), qsTr("Collapse All"), qsTr("Custom")]
            currentIndex: page.collapseModes.indexOf(preferences.collapseMode)
            onActivated: index => preferences.collapseMode = page.collapseModes[index]
        }
    }

    FormSection {
        title: qsTr("Behaviour")
        footer: qsTr("Two-column layout is recommended only on larger screens or when using full-width.")

        FormToggleRow {
            label: qsTr("Two-Column Layout")
            checked: preferences.twoColumnLayout
            onToggled: value => preferences.twoColumnLayout = value
        }

        FormDivider {}
        FormToggleRow {
            label: qsTr("Compact Glossaries")
            checked: preferences.compactGlossaries
            onToggled: value => preferences.compactGlossaries = value
        }
    }

    FormSection {
        title: qsTr("Dictionaries")
        footer: library.dictionaryType === "Term" && library.rows.length > 0
                ? qsTr("Category for Anki definition fields") : ""
        headerAccessory: SegmentedPicker {
            Accessible.name: qsTr("Dictionary Type")
            model: [qsTr("Term"), qsTr("Frequency"), qsTr("Pitch"), qsTr("Kanji")]
            currentIndex: page.types.indexOf(library.dictionaryType)
            onActivated: index => library.selectType(page.types[index])
        }

        DictionaryLibraryList {
            width: parent ? parent.width : 0
            libraryModel: library
            flickable: page.flickable
            categories: page.categories
        }
    }
}

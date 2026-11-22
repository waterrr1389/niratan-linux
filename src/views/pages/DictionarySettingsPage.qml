// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Dictionaries settings page, following Features/Settings/DictionaryView.swift
// (body, the Download/Import card, dictionaryTypePicker, the dictionary list
// rows with their enable toggle, DictionaryCategoryPicker, delete and drag
// reorder, the recommended sheet and the error alert) and
// NativeMac/NativeReuseViews.swift (the `.dictionaries` settings sidebar
// destination and its title). The page is pushed from SettingsPage's rail.
//
// Local zips are the only install path (Decision O3): the recommended list
// names the dictionaries for the profile's language, says a zip is imported
// above, and offers no download action. Reordering uses Up and Down buttons
// instead of Swift's drag gesture, and a row shows the directory name only
// because the library list carries no revision caption. The import runs
// synchronously in the view model, so there is no loading overlay.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
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

    background: Rectangle {
        color: Theme.windowBackground
    }

    DictionarySettingsViewModel {
        id: settings

        Component.onCompleted: settings.refresh()
    }

    readonly property var categories: ["none", "monolingual", "bilingual", "exclude"]
    readonly property var categoryLabels: [
        qsTr("No Category"), qsTr("Monolingual"), qsTr("Bilingual"), qsTr("Exclude")
    ]

    // The recommended table has at most seven English entries; each row binds
    // to its index so the page shows the list without a dynamically created
    // delegate.
    component RecommendedRow: RowLayout {
        id: recommendedRow

        property int recommendationIndex: 0

        readonly property var entry: recommendationIndex < settings.recommended.length
            ? settings.recommended[recommendationIndex] : null

        Layout.fillWidth: true
        spacing: Theme.spacingSm
        visible: entry !== null

        Label {
            Layout.fillWidth: true
            text: recommendedRow.entry ? recommendedRow.entry.name : ""
            color: Theme.textPrimary
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
        }

        Label {
            text: recommendedRow.entry ? recommendedRow.entry.type : ""
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
    }

    FileDialog {
        id: importDialog

        title: qsTr("Import Dictionaries")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Yomitan dictionaries (*.zip)")]

        onAccepted: settings.importFiles(selectedFiles)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PageHeader {
            title: qsTr("Dictionaries")

            onBackRequested: page.backRequested()
        }

        ScrollView {
            id: scroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: Theme.spacingLg
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

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd

                    Button {
                        text: qsTr("Import")

                        onClicked: importDialog.open()
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingXs

                        Label {
                            text: qsTr("Supported Formats")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Yomitan term, frequency, pitch and kanji dictionaries (.zip) are supported")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    Button {
                        text: qsTr("Term")
                        checkable: true
                        checked: settings.dictionaryType === "Term"
                        onClicked: settings.selectType("Term")
                    }

                    Button {
                        text: qsTr("Frequency")
                        checkable: true
                        checked: settings.dictionaryType === "Frequency"
                        onClicked: settings.selectType("Frequency")
                    }

                    Button {
                        text: qsTr("Pitch")
                        checkable: true
                        checked: settings.dictionaryType === "Pitch"
                        onClicked: settings.selectType("Pitch")
                    }

                    Button {
                        text: qsTr("Kanji")
                        checkable: true
                        checked: settings.dictionaryType === "Kanji"
                        onClicked: settings.selectType("Kanji")
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    Repeater {
                        model: settings.rows

                        delegate: Rectangle {
                            id: row

                            required property var modelData
                            required property int index

                            Layout.fillWidth: true
                            implicitHeight: 48
                            radius: Theme.controlRadius
                            color: Theme.cardBackground
                            border.width: Theme.cardBorderWidth
                            border.color: Theme.cardStroke

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.spacingMd
                                anchors.rightMargin: Theme.spacingSm
                                spacing: Theme.spacingSm

                                Label {
                                    Layout.fillWidth: true
                                    text: row.modelData.title
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontBody
                                    elide: Text.ElideRight
                                }

                                ComboBox {
                                    visible: settings.dictionaryType === "Term"
                                    implicitWidth: 160
                                    model: page.categoryLabels
                                    currentIndex: page.categories.indexOf(row.modelData.category)
                                    onActivated: settings.setCategory(
                                                     row.modelData.title,
                                                     page.categories[currentIndex])
                                }

                                Switch {
                                    checked: row.modelData.enabled
                                    onToggled: settings.setEnabled(row.modelData.title, checked)
                                }

                                Button {
                                    text: qsTr("Up")
                                    enabled: row.index > 0
                                    onClicked: settings.move(
                                                   row.modelData.title,
                                                   settings.rows[row.index - 1].title)
                                }

                                Button {
                                    text: qsTr("Down")
                                    enabled: row.index + 1 < settings.rows.length
                                    onClicked: settings.move(
                                                   row.modelData.title,
                                                   settings.rows[row.index + 1].title)
                                }

                                Button {
                                    text: qsTr("Delete")
                                    onClicked: settings.remove(row.modelData.title)
                                }
                            }
                        }
                    }

                    Label {
                        visible: settings.dictionaryType === "Term"
                        text: qsTr("Category for Anki definition fields")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingXs

                    Label {
                        text: qsTr("Recommended for this profile")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Import a local .zip of these dictionaries above")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        wrapMode: Text.WordWrap
                    }

                    RecommendedRow { recommendationIndex: 0 }
                    RecommendedRow { recommendationIndex: 1 }
                    RecommendedRow { recommendationIndex: 2 }
                    RecommendedRow { recommendationIndex: 3 }
                    RecommendedRow { recommendationIndex: 4 }
                    RecommendedRow { recommendationIndex: 5 }
                    RecommendedRow { recommendationIndex: 6 }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}

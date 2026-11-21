// SPDX-License-Identifier: GPL-3.0-or-later
//
// The profile management page, following Features/Settings/ProfilesView.swift:
// the active profile list with a checkmark, a language caption and the
// Built-in marker, the Create Profile and Rename editors (420 wide, name field,
// language picker locked while renaming), the delete confirmation and the error
// alert. The dialog width and the footer sentence about per-profile settings
// come from the same view.
//
// Activating a row only switches the stored profile. The reader and dictionary
// pages read their settings from the active profile the next time they load
// them, so this page never claims to have swapped them in place.
//
// Every colour comes from Theme; the list keeps the repository order, so the
// built-in profile stays first.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: page

    signal backRequested()

    // The language a new profile starts with: the active profile's language,
    // as ProfilesView.swift sets draftLanguage = repository.activeProfile.language.
    function activeLanguage() {
        for (var i = 0; i < profilesModel.profiles.length; ++i) {
            if (profilesModel.profiles[i].active)
                return profilesModel.profiles[i].language
        }
        return "ja"
    }

    function openCreate() {
        profilesModel.clearError()
        editorDialog.renaming = false
        editorDialog.profileId = ""
        editorDialog.profileName = ""
        editorDialog.profileLanguage = "ja"
        editorDialog.open()
    }

    function openRename(profile) {
        profilesModel.clearError()
        editorDialog.renaming = true
        editorDialog.profileId = profile.id
        editorDialog.profileName = profile.name
        editorDialog.profileLanguage = profile.language
        editorDialog.open()
    }

    function confirmDelete(profile) {
        profilesModel.clearError()
        deleteDialog.profileId = profile.id
        deleteDialog.profileName = profile.name
        deleteDialog.open()
    }

    ProfilesViewModel {
        id: profilesModel
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PageHeader {
            title: qsTr("Profiles")
            onBackRequested: page.backRequested()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 12
            Layout.preferredHeight: bannerRow.implicitHeight + 16
            visible: profilesModel.error.length > 0
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.width: Theme.cardBorderWidth
            border.color: Theme.popupDanger

            RowLayout {
                id: bannerRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 8
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: profilesModel.error
                    color: Theme.popupDanger
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Button {
                    text: qsTr("Dismiss")
                    onClicked: profilesModel.clearError()
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: page.width
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.topMargin: 16
                    implicitHeight: sectionColumn.implicitHeight + 24
                    radius: Theme.cardRadius
                    color: Theme.cardBackground
                    border.width: Theme.cardBorderWidth
                    border.color: Theme.cardStroke

                    ColumnLayout {
                        id: sectionColumn
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 2

                        Repeater {
                            model: profilesModel.profiles

                            delegate: ItemDelegate {
                                id: profileRow
                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: 56
                                onClicked: profilesModel.activateProfile(profileRow.modelData.id)

                                TapHandler {
                                    acceptedButtons: Qt.RightButton
                                    onTapped: profileMenu.popup(profileRow)
                                }

                                contentItem: RowLayout {
                                    spacing: 10

                                    Label {
                                        Layout.preferredWidth: 20
                                        text: profileRow.modelData.active
                                            ? "\u2713" : "\u25CB"
                                        color: profileRow.modelData.active
                                            ? Theme.accent : Theme.textSecondary
                                        font.pixelSize: 16
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Label {
                                            Layout.fillWidth: true
                                            text: profileRow.modelData.name
                                            color: Theme.textPrimary
                                            font.pixelSize: 14
                                            elide: Text.ElideRight
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: profileRow.modelData.language === "en"
                                                ? qsTr("English") : qsTr("Japanese")
                                            color: Theme.textSecondary
                                            font.pixelSize: 11
                                        }
                                    }

                                    Label {
                                        visible: profileRow.modelData.builtIn
                                        text: qsTr("Built-in")
                                        color: Theme.textSecondary
                                        font.pixelSize: 11
                                    }

                                    Button {
                                        id: rowMenuButton
                                        implicitWidth: 32
                                        implicitHeight: 28
                                        padding: 0
                                        text: "\u22EF"
                                        onClicked: profileMenu.popup(rowMenuButton)

                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Profile Actions")
                                    }
                                }

                                Menu {
                                    id: profileMenu

                                    MenuItem {
                                        text: qsTr("Rename")
                                        onClicked: page.openRename(profileRow.modelData)
                                    }

                                    MenuItem {
                                        text: qsTr("Delete")
                                        visible: !profileRow.modelData.isDefault
                                        onClicked: page.confirmDelete(profileRow.modelData)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            Layout.bottomMargin: 6
                            Layout.preferredHeight: 1
                            color: Theme.cardStroke
                        }

                        ItemDelegate {
                            id: createRow
                            Layout.fillWidth: true
                            implicitHeight: 44
                            onClicked: page.openCreate()

                            contentItem: RowLayout {
                                spacing: 10

                                Label {
                                    Layout.preferredWidth: 20
                                    text: "+"
                                    color: Theme.accent
                                    font.pixelSize: 16
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Create Profile")
                                    color: Theme.textPrimary
                                    font.pixelSize: 14
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.topMargin: 8
                    Layout.bottomMargin: 24
                    text: qsTr("Dictionary, reader appearance and Anki mining settings follow the active profile. Open pages keep the settings they already loaded until they load them again.")
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Dialog {
        id: editorDialog

        property bool renaming: false
        property string profileId: ""
        property string profileName: ""
        property string profileLanguage: "ja"

        modal: true
        anchors.centerIn: parent
        width: 420
        padding: 24
        title: editorDialog.renaming ? qsTr("Rename Profile") : qsTr("New Profile")

        function confirm() {
            const language = languageBox.currentIndex === 1 ? "en" : "ja"
            const changed = editorDialog.renaming
                ? profilesModel.renameProfile(editorDialog.profileId, nameField.text)
                : profilesModel.createProfile(nameField.text, language)
            if (changed)
                editorDialog.accept()
            else
                nameField.forceActiveFocus()
        }

        onAboutToShow: {
            nameField.text = editorDialog.renaming ? editorDialog.profileName : ""
            const language = editorDialog.renaming
                ? editorDialog.profileLanguage : page.activeLanguage()
            languageBox.currentIndex = language === "en" ? 1 : 0
        }

        onOpened: nameField.forceActiveFocus()

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: qsTr("Profile Name")
                color: Theme.textPrimary
                font.pixelSize: 12
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: qsTr("Profile Name")
                selectByMouse: true
                onAccepted: {
                    if (nameField.text.trim().length > 0)
                        editorDialog.confirm()
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Language")
                color: Theme.textPrimary
                font.pixelSize: 12
            }

            ComboBox {
                id: languageBox
                Layout.fillWidth: true
                model: [qsTr("Japanese"), qsTr("English")]
                enabled: !editorDialog.renaming
            }

            Label {
                Layout.fillWidth: true
                visible: profilesModel.error.length > 0
                text: profilesModel.error
                color: Theme.popupDanger
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        footer: RowLayout {
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Cancel")
                onClicked: editorDialog.reject()
            }

            Button {
                text: editorDialog.renaming ? qsTr("Save") : qsTr("Create")
                enabled: nameField.text.trim().length > 0
                onClicked: editorDialog.confirm()
            }
        }
    }

    Dialog {
        id: deleteDialog

        property string profileId: ""
        property string profileName: ""

        modal: true
        anchors.centerIn: parent
        width: 400
        title: qsTr("Delete Profile")
        standardButtons: Dialog.Yes | Dialog.No

        contentItem: Label {
            text: qsTr("Delete \"%1\"? This cannot be undone.").arg(deleteDialog.profileName)
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }

        onAccepted: profilesModel.deleteProfile(deleteDialog.profileId)
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/ProfilesView.swift: body, profileRow and profileEditor.
// Profile activation affects the next load of reader and dictionary settings.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan
import "../icons/icon_map.js" as IconMap

SettingsForm {
    id: page

    objectName: "settings-profiles"

    function activeLanguage() {
        for (const profile of profilesModel.profiles) {
            if (profile.active)
                return profile.language
        }
        return "ja"
    }

    function openEditor(profile) {
        profilesModel.clearError()
        editorDialog.profileId = profile ? profile.id : ""
        nameField.text = profile ? profile.name : ""
        languageBox.currentIndex = (profile ? profile.language : page.activeLanguage()) === "en" ? 1 : 0
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

    SettingRow {
        visible: profilesModel.error.length > 0 && !editorDialog.opened
        label: profilesModel.error

        Button {
            text: qsTr("Dismiss")
            onClicked: profilesModel.clearError()
        }
    }

    SettingsCard {
        title: qsTr("Active Profile")
        footer: qsTr("Dictionary, reader appearance and Anki mining settings follow the active profile. Open pages keep the settings they already loaded until they load them again.")

        Repeater {
            model: profilesModel.profiles

            delegate: ColumnLayout {
                id: profileEntry

                required property int index
                required property var modelData

                Layout.fillWidth: true
                spacing: 0

                SettingsSeparator { visible: profileEntry.index > 0 }

                ItemDelegate {
                    id: profileRow

                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    leftPadding: 4
                    rightPadding: 4
                    implicitHeight: 46
                    Accessible.name: profileEntry.modelData.name
                    onClicked: profilesModel.activateProfile(profileEntry.modelData.id)

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: profileMenu.popup(profileRow)
                    }

                    contentItem: RowLayout {
                        spacing: Theme.spacingSm

                        Icon {
                            size: 16
                            name: profileEntry.modelData.active ? "checkmark.circle.fill" : "circle"
                            color: profileEntry.modelData.active ? Theme.accent : Theme.textSecondary
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: profileEntry.modelData.name
                                elide: Text.ElideRight
                            }

                            SettingsNote {
                                text: profileEntry.modelData.language === "en" ? qsTr("English") : qsTr("Japanese")
                            }
                        }

                        SettingsNote {
                            Layout.fillWidth: false
                            visible: profileEntry.modelData.builtIn
                            text: qsTr("Built-in")
                        }

                        ToolButton {
                            id: menuButton

                            text: "\u22EF"
                            Accessible.name: qsTr("Profile Actions")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Profile Actions")
                            onClicked: profileMenu.popup(menuButton)
                        }
                    }

                    Menu {
                        id: profileMenu

                        MenuItem {
                            text: qsTr("Rename")
                            onTriggered: page.openEditor(profileEntry.modelData)
                        }

                        MenuItem {
                            text: qsTr("Delete")
                            visible: !profileEntry.modelData.isDefault
                            onTriggered: page.confirmDelete(profileEntry.modelData)
                        }
                    }
                }
            }
        }

        SettingsSeparator {}
        SettingsActions {
            Button {
                objectName: "createProfileButton"
                text: qsTr("Create Profile")
                icon.source: IconMap.source("plus")
                onClicked: page.openEditor(null)
            }
        }
    }

    Dialog {
        id: editorDialog

        objectName: "profileEditor"
        property string profileId
        readonly property bool renaming: profileId.length > 0

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 420
        title: renaming ? qsTr("Rename Profile") : qsTr("New Profile")

        function confirm() {
            if (nameField.text.trim().length === 0)
                return
            const changed = renaming
                ? profilesModel.renameProfile(profileId, nameField.text)
                : profilesModel.createProfile(nameField.text, languageBox.currentIndex === 1 ? "en" : "ja")
            if (changed)
                accept()
            else
                nameField.forceActiveFocus()
        }

        onOpened: nameField.forceActiveFocus()

        contentItem: ColumnLayout {
            spacing: Theme.spacingMd

            TextField {
                id: nameField

                objectName: "profileNameField"
                Layout.fillWidth: true
                Accessible.name: qsTr("Profile Name")
                placeholderText: qsTr("Profile Name")
                selectByMouse: true
                onAccepted: editorDialog.confirm()
            }

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Language")
                }

                ComboBox {
                    id: languageBox

                    Accessible.name: qsTr("Language")
                    model: [qsTr("Japanese"), qsTr("English")]
                    enabled: !editorDialog.renaming
                }
            }

            SettingsNote {
                visible: profilesModel.error.length > 0
                text: profilesModel.error
                color: Theme.popupDanger
            }
        }

        // The wrapper prevents Dialog from accepting automatically when the
        // button box emits accepted; only a successful write closes the editor.
        footer: Item {
            implicitHeight: buttons.implicitHeight

            DialogButtonBox {
                id: buttons

                anchors.fill: parent
                standardButtons: DialogButtonBox.Cancel
                onRejected: editorDialog.reject()
                onAccepted: editorDialog.confirm()

                Button {
                    objectName: "saveProfileButton"
                    text: editorDialog.renaming ? qsTr("Save") : qsTr("Create")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    prominent: true
                    enabled: nameField.text.trim().length > 0
                }
            }
        }
    }

    Dialog {
        id: deleteDialog

        property string profileId
        property string profileName

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 400
        title: qsTr("Delete Profile")
        standardButtons: Dialog.Ok | Dialog.Cancel
        Component.onCompleted: standardButton(Dialog.Ok).text = qsTr("Delete")

        contentItem: Label {
            text: qsTr("Delete \"%1\"? This cannot be undone.").arg(deleteDialog.profileName)
            wrapMode: Text.WordWrap
        }

        onAccepted: profilesModel.deleteProfile(profileId)
    }
}

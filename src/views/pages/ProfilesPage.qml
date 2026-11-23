// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/ProfilesView.swift: body, profileRow and profileEditor.
// Profile activation affects the next load of reader and dictionary settings.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

PanelScaffold {
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

    FormRow {
        visible: profilesModel.error.length > 0 && !editorDialog.opened
        label: profilesModel.error

        FormButton {
            text: qsTr("Dismiss")
            onClicked: profilesModel.clearError()
        }
    }

    FormSection {
        title: qsTr("Active Profile")
        footer: qsTr("Dictionary, reader appearance and Anki mining settings follow the active profile. Open pages keep the settings they already loaded until they load them again.")

        Repeater {
            model: profilesModel.profiles

            delegate: Column {
                id: profileEntry

                required property int index
                required property var modelData

                width: parent ? parent.width : implicitWidth

                FormDivider { visible: profileEntry.index > 0 }

                Item {
                    width: profileEntry.width
                    implicitHeight: profileRow.implicitHeight

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: profileMenu.popup(profileRow)
                    }

                    FormRow {
                        id: profileRow

                        label: profileEntry.modelData.name
                        subtitle: profileEntry.modelData.language === "en" ? qsTr("English") : qsTr("Japanese")
                        glyph: profileEntry.modelData.active ? "checkmark.circle.fill" : "circle"
                        glyphColor: profileEntry.modelData.active ? Theme.accent : Theme.textSecondary
                        clickable: true
                        Accessible.role: Accessible.Button
                        Accessible.name: profileEntry.modelData.name
                        onTapped: profilesModel.activateProfile(profileEntry.modelData.id)

                        Row {
                            spacing: Theme.capsuleSpacing

                            StyledLabel {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: profileEntry.modelData.builtIn
                                text: qsTr("Built-in")
                                textStyle: Theme.footnoteFont
                                secondary: true
                            }

                            // The menu button is a sibling so its tap does
                            // not also activate the profile.
                            Item {
                                width: menuButton.width
                                height: menuButton.height
                            }
                        }
                    }

                    GlyphButton {
                        id: menuButton

                        anchors.right: parent.right
                        anchors.rightMargin: profileRow.padding
                        anchors.verticalCenter: parent.verticalCenter
                        glyph: "ellipsis"
                        Accessible.name: qsTr("Profile Actions")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Profile Actions")
                        onClicked: profileMenu.popup(menuButton)
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

        FormDivider {}
        FormButtonRow {
            FormButton {
                objectName: "createProfileButton"
                text: qsTr("Create Profile")
                glyph: "plus"
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
        width: Math.min(page.width, Theme.dialogWidth)
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

            FormPickerRow {
                id: languageBox

                Layout.fillWidth: true
                label: qsTr("Language")
                model: [qsTr("Japanese"), qsTr("English")]
                placeholderText: label
                constrainMenuHeight: true
                enabled: !editorDialog.renaming
                onActivated: index => currentIndex = index
            }

            StyledLabel {
                Layout.fillWidth: true
                visible: profilesModel.error.length > 0
                text: profilesModel.error
                textStyle: Theme.footnoteFont
                wrapMode: Text.Wrap
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

                FormButton {
                    objectName: "saveProfileButton"
                    text: editorDialog.renaming ? qsTr("Save") : qsTr("Create")
                    prominent: true
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
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
        width: editorDialog.width
        title: qsTr("Delete Profile")
        standardButtons: Dialog.Ok | Dialog.Cancel
        Component.onCompleted: standardButton(Dialog.Ok).text = qsTr("Delete")

        contentItem: StyledLabel {
            text: qsTr("Delete \"%1\"? This cannot be undone.").arg(deleteDialog.profileName)
            wrapMode: Text.WordWrap
        }

        onAccepted: profilesModel.deleteProfile(profileId)
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
// SyncView.swift: the TTU-only Syncing form; no Provider or shared-library card.
// The process SyncSettingsViewModel owns credentials and the shared Drive cache.
// Rows delegate to that model, with no file access or browser sign-in in QML.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

PanelScaffold {
    id: page

    objectName: "settings-sync"

    FormSection {
        objectName: "syncSection"
        title: qsTr("Syncing")
        footer: qsTr("Sync bookmarks and statistics with ッツ Reader per book via Google Drive.")
            + (SyncSettingsViewModel.enabled ? "<br/>" + [
                qsTr("A <b><a href=\"https://github.com/ttu-ttu/ebook-reader?tab=readme-ov-file#storage-sources\">Google Cloud project</a></b> is necessary for syncing."),
                qsTr("1. After the initial setup, create another <b>OAuth client ID</b> in the same project."),
                qsTr("2. Select <b>Desktop app</b> as the <b>Application type</b>."),
                qsTr("3. Paste the <b>Client ID</b> in the textbox below and press '<b>Connect Google Drive</b>'."),
                qsTr("4. You can sync individual books by long-pressing and selecting '<b>Sync</b>'."),
                qsTr("<b><a href=\"https://github.com/Manhhao/Hoshi-Reader/blob/develop/TTUSYNC.md\">More...</a></b>")
            ].join("<br/>") : "")

        FormToggleRow {
            objectName: "syncEnable"
            label: qsTr("Enable")
            checked: SyncSettingsViewModel.enabled
            onToggled: value => SyncSettingsViewModel.enabled = value
        }
    }

    FormSection {
        objectName: "syncConnectionSection"
        title: qsTr("Connection")
        visible: SyncSettingsViewModel.enabled

        FormRow {
            id: clientRow

            label: qsTr("Client ID")

            TextField {
                objectName: "clientIdField"
                width: implicitWidth
                Accessible.name: clientRow.label
                text: SyncSettingsViewModel.clientId
                placeholderText: qsTr("Required")
                enabled: !SyncSettingsViewModel.authenticated && !SyncSettingsViewModel.connecting
                opacity: SyncSettingsViewModel.authenticated ? 0.6 : 1
                onTextEdited: SyncSettingsViewModel.clientId = text
                onTextChanged: if (!enabled) cursorPosition = 0
                onEnabledChanged: if (!enabled) cursorPosition = 0
            }
        }

        FormDivider {}

        FormRow {
            label: qsTr("Status")

            StyledLabel {
                objectName: "syncStatus"
                text: SyncSettingsViewModel.connectionStatus
                secondary: true
            }
        }

        FormDivider {}

        FormButtonRow {
            FormButton {
                objectName: "syncConnect"
                sizeToLabel: true
                text: qsTr("Connect Google Drive")
                visible: !SyncSettingsViewModel.authenticated
                enabled: !SyncSettingsViewModel.connecting
                onClicked: SyncSettingsViewModel.signIn()
            }

            FormButton {
                objectName: "syncCancel"
                sizeToLabel: true
                text: qsTr("Cancel")
                visible: !SyncSettingsViewModel.authenticated && SyncSettingsViewModel.connecting
                onClicked: SyncSettingsViewModel.cancelSignIn()
            }

            FormButton {
                objectName: "syncClearCache"
                sizeToLabel: true
                text: qsTr("Clear Cache")
                visible: SyncSettingsViewModel.authenticated
                destructive: true
                onClicked: clearCacheAlert.open()
            }

            FormButton {
                objectName: "syncSignOut"
                sizeToLabel: true
                text: qsTr("Sign out")
                visible: SyncSettingsViewModel.hasCredentials
                destructive: true
                onClicked: signOutAlert.open()
            }
        }
    }

    FormSection {
        objectName: "syncBehaviourSection"
        title: qsTr("Behaviour")
        visible: SyncSettingsViewModel.enabled

        FormSegmentedRow {
            objectName: "syncDirection"
            label: qsTr("Direction")
            model: [qsTr("Auto"), qsTr("Manual")]
            currentIndex: SyncSettingsViewModel.syncMode === "Manual" ? 1 : 0
            onActivated: index => SyncSettingsViewModel.syncMode = index === 1 ? "Manual" : "Auto"
        }

        FormDivider {}

        FormToggleRow {
            objectName: "syncAuto"
            label: qsTr("Auto Sync")
            checked: SyncSettingsViewModel.autoSync
            onToggled: value => SyncSettingsViewModel.autoSync = value
        }
    }

    FormSection {
        objectName: "syncDataSection"
        title: qsTr("Data")
        visible: SyncSettingsViewModel.enabled

        FormToggleRow {
            objectName: "syncUploadBooks"
            label: qsTr("Upload Books")
            subtitle: qsTr("Uploads books on first sync if no bookdata is stored on Google Drive.")
            checked: SyncSettingsViewModel.uploadBooks
            onToggled: value => SyncSettingsViewModel.uploadBooks = value
        }

        FormDivider { visible: SyncSettingsViewModel.statisticsEnabled }

        FormToggleRow {
            objectName: "syncStats"
            label: qsTr("Sync Stats")
            visible: SyncSettingsViewModel.statisticsEnabled
            checked: SyncSettingsViewModel.syncStatistics
            onToggled: value => SyncSettingsViewModel.syncStatistics = value
        }

        FormDivider { visible: SyncSettingsViewModel.sasayakiEnabled }

        FormToggleRow {
            objectName: "syncAudioBook"
            label: qsTr("Sync Audiobook Progress")
            visible: SyncSettingsViewModel.sasayakiEnabled
            checked: SyncSettingsViewModel.syncAudioBook
            onToggled: value => SyncSettingsViewModel.syncAudioBook = value
        }
    }

    AlertDialog {
        id: clearCacheAlert

        objectName: "syncClearCacheAlert"
        title: qsTr("Clear Cache?")
        message: qsTr("This will clear cached folder ids and book covers.")
        acceptText: qsTr("Clear")
        rejectText: qsTr("Cancel")
        acceptDestructive: true
        onAccepted: SyncSettingsViewModel.clearCache()
    }

    AlertDialog {
        id: signOutAlert

        objectName: "syncSignOutAlert"
        title: qsTr("Sign out?")
        message: qsTr("Signing out will clear authorization tokens, cached folder ids and book covers.")
        acceptText: qsTr("Confirm")
        rejectText: qsTr("Cancel")
        acceptDestructive: true
        onAccepted: SyncSettingsViewModel.signOut()
    }

    AlertDialog {
        objectName: "syncErrorAlert"
        title: qsTr("Error")
        message: SyncSettingsViewModel.error
        visible: message !== ""
        onClosed: SyncSettingsViewModel.clearError()
        onAccepted: SyncSettingsViewModel.clearError()
    }
}

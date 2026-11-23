// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/AudioView.swift: audio sources and Yomitan JSON support.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

PanelScaffold {
    objectName: "settings-wordAudio"

    FormSection {
        id: section

        title: qsTr("Preferred Audio Source")
        footer: qsTr("Use a Yomitan-compatible JSON audio URL. The default online source is tried if this source is unavailable or has no recording. Leave empty to use only the default.")

        FormRow {
            id: urlRow
            fillsRow: true

            TextField {
                id: source

                objectName: "preferredAudioSource"
                width: Math.max(0, urlRow.width - 2 * Theme.fillRowPaddingHorizontal)
                Accessible.name: qsTr("Audio source URL")
                placeholderText: "http://127.0.0.1:5050/localaudio/get/?term={term}&reading={reading}"
                Component.onCompleted: text = settings.preferredAudioSource()
                onAccepted: saveRow.save()
            }
        }

        FormDivider {}

        FormButtonRow {
            FormButton {
                id: saveRow

                objectName: "savePreferredAudioSource"
                text: qsTr("Save")
                onClicked: save()

                function save() {
                    const error = settings.savePreferredAudioSource(source.text)
                    if (error === "")
                        source.text = settings.preferredAudioSource()
                    status.text = error === "" ? qsTr("Audio source saved.") : error
                }
            }
        }

        FormRow {
            id: status

            property string text
            visible: text !== ""
            label: text
        }

        SettingsViewModel { id: settings }
    }
}

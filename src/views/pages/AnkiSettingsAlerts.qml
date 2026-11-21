// SPDX-License-Identifier: GPL-3.0-or-later
//
// The three alerts of Features/Settings/AnkiView.swift: "Fetch from Anki?"
// (OK runs the fetch), "Apply defaults?" (Apply runs the defaults) and "Error"
// (OK clears the message). Each keeps Swift's title and message text.
//
// The page owns the view model and connects the signals to its commands;
// openFetch() and openDefaults() open the confirmation dialogs. The root fills
// the page so the centred dialogs land in its middle.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: alerts

    property string errorText: ""

    signal fetchRequested
    signal defaultsRequested
    signal errorDismissed

    function openFetch() {
        fetchDialog.open();
    }

    function openDefaults() {
        defaultsDialog.open();
    }

    Dialog {
        id: fetchDialog

        modal: true
        anchors.centerIn: parent
        width: 420
        padding: Theme.spacingXl
        title: qsTr("Fetch from Anki?")

        contentItem: Label {
            text: qsTr("This will refresh decks and models while preserving mappings for fields that still exist.")
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }

        footer: RowLayout {
            spacing: Theme.spacingSm

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Cancel")
                onClicked: fetchDialog.reject()
            }

            Button {
                text: qsTr("OK")
                onClicked: {
                    fetchDialog.accept();
                    alerts.fetchRequested();
                }
            }
        }
    }

    Dialog {
        id: defaultsDialog

        modal: true
        anchors.centerIn: parent
        width: 420
        padding: Theme.spacingXl
        title: qsTr("Apply defaults?")

        contentItem: Label {
            text: qsTr("This replaces mappings for fields defined by the selected note type. Other fields are preserved.")
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }

        footer: RowLayout {
            spacing: Theme.spacingSm

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Cancel")
                onClicked: defaultsDialog.reject()
            }

            Button {
                text: qsTr("Apply")
                onClicked: {
                    defaultsDialog.accept();
                    alerts.defaultsRequested();
                }
            }
        }
    }

    Dialog {
        id: errorDialog

        modal: true
        anchors.centerIn: parent
        width: 420
        padding: Theme.spacingXl
        title: qsTr("Error")
        visible: alerts.errorText.length > 0

        contentItem: Label {
            text: alerts.errorText
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }

        footer: RowLayout {
            spacing: Theme.spacingSm

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("OK")
                onClicked: errorDialog.close()
            }
        }

        onClosed: alerts.errorDismissed()
    }
}

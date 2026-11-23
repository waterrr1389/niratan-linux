// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary search bar, following Features/Dictionary/DictionarySearchView.swift
// `DictionarySearchBar.body` and Features/Dictionary/CustomSearchField.swift
// `CustomSearchField`. One row inside the capsule: a secondary magnifier, a
// borderless text field that fills the row, and a clear control shown while
// the text is not empty. Return submits and drops focus; the caller reads
// `text`.
//
// The field is objectName "field" and the clear control objectName
// "clearButton", so the QML load test can reach them. `placeholderText` is
// empty by default, as CustomSearchField's TextField("") is; the video library
// passes "Search Videos".

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: searchBar

    property alias text: field.text
    property alias placeholderText: field.placeholderText

    signal submitted()

    function forceActiveFocus() {
        field.forceActiveFocus()
    }

    implicitHeight: Math.max(leadingIcon.implicitHeight, field.implicitHeight,
                             clearButton.implicitHeight) + 2 * 12

    GlassCapsule {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            spacing: 10

            Icon {
                id: leadingIcon

                name: "magnifyingglass"
                size: 16
                color: Theme.textSecondary
                Layout.alignment: Qt.AlignVCenter
            }

            TextField {
                id: field

                objectName: "field"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                background: null
                padding: 0
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                placeholderText: ""
                selectByMouse: true
                activeFocusOnTab: true

                onAccepted: {
                    searchBar.submitted()
                    focus = false
                }
            }

            Item {
                id: clearButton

                objectName: "clearButton"
                implicitWidth: 22
                implicitHeight: 22
                visible: field.text.length > 0
                Layout.alignment: Qt.AlignVCenter

                signal clicked()

                onClicked: {
                    field.text = ""
                    searchBar.forceActiveFocus()
                }

                TapHandler {
                    onTapped: clearButton.clicked()
                }

                Icon {
                    anchors.centerIn: parent
                    name: "xmark.circle.fill"
                    size: 16
                    color: Theme.textSecondary
                }
            }
        }
    }
}

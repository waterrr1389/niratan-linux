// SPDX-License-Identifier: GPL-3.0-or-later
//
// One video library section title, following
// Features/Video/VideoLibraryView.swift `VideoLibraryCollapsibleSectionHeader`,
// `VideoLibraryPosterSectionHeader` and
// `VideoLibraryCollectionActionsMenu`.
//
// A collapsible header shows a chevron that turns down when expanded, the
// title, the count and, when the section has a collection, a collection
// actions menu holding a destructive "Delete Collection". A plain header
// shows only the title. Clicking the title area toggles the section; the
// actions button is separate so it never toggles by accident.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan
import "../icons/icon_map.js" as IconMap

Item {
    id: header

    property string title
    property int count
    property bool collapsible: false
    property bool expanded: true
    property bool hasCollection: false

    signal clicked()
    signal deleteCollectionRequested()

    implicitHeight: collapsible ? Theme.videoSectionHeaderHeight : Theme.videoSectionTitleHeight

    // The plain header used by the generated modes.
    StyledLabel {
        visible: !header.collapsible
        width: parent.width
        text: header.title
        textStyle: Theme.title3Font
        weight: Theme.weightSemibold
        elide: Text.ElideRight
    }

    Item {
        id: toggleArea

        visible: header.collapsible
        anchors.left: parent.left
        anchors.right: actions.visible ? actions.left : parent.right
        anchors.rightMargin: Theme.spacingSm
        height: parent.height

        TapHandler {
            onTapped: header.clicked()
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spacingSm

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "chevron.right"
                size: Theme.videoSectionChevron
                color: Theme.textTertiary
                rotation: header.expanded ? 90 : 0
            }

            StyledLabel {
                anchors.verticalCenter: parent.verticalCenter
                text: header.title
                textStyle: Theme.title3Font
                weight: Theme.weightSemibold
                elide: Text.ElideRight
            }

            StyledLabel {
                anchors.verticalCenter: parent.verticalCenter
                text: header.count
                textStyle: Theme.calloutFont
                secondary: true
                monospacedDigits: true
            }
        }
    }

    // VideoLibraryCollectionActionsMenu: a bare ellipsis glyph button.
    GlyphButton {
        id: actions

        objectName: "collectionActionsButton"
        visible: header.hasCollection
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        diameter: Theme.videoSectionActionDiameter
        glyph: "ellipsis"
        filled: false
        Accessible.name: qsTr("Collection Actions")

        Menu {
            id: actionsMenu

            MenuItem {
                text: qsTr("Delete Collection")
                icon.source: IconMap.source("trash")
                onTriggered: header.deleteCollectionRequested()
            }
        }

        onClicked: actionsMenu.popup(actions, 0, actions.height + Theme.spacingXs)
    }
}

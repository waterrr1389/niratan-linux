// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Drawer {
    id: gallery

    property int previewIndex: -1
    property var seenPaths: ({})
    required property Item readerPage

    edge: Qt.RightEdge
    width: Math.min(560, readerPage.width * 0.82)
    height: readerPage.height
    modal: true
    focus: true
    interactive: false
    closePolicy: Popup.CloseOnPressOutside
    padding: 0

    function closePreview() {
        previewIndex = -1
        galleryBody.forceActiveFocus()
    }

    function markSeen(path) {
        const updated = Object.assign(Object.create(null), seenPaths)
        updated[path] = true
        seenPaths = updated
    }

    function showPreview(index) {
        previewIndex = index
        markSeen(ReaderViewModel.galleryImages[index].path)
        preview.forceActiveFocus()
    }

    background: Rectangle {
        color: Theme.cardBackground
        border.width: 1
        border.color: Theme.cardStroke
    }

    ColumnLayout {
        id: galleryBody
        anchors.fill: parent
        spacing: 0

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape) {
                if (gallery.previewIndex >= 0)
                    gallery.closePreview()
                else
                    gallery.close()
                event.accepted = true
            } else if (preview.visible && event.modifiers === Qt.NoModifier) {
                if (event.key === Qt.Key_Left) {
                    preview.navigate(-1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Right) {
                    preview.navigate(1)
                    event.accepted = true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingMd
            Label {
                Layout.fillWidth: true
                text: qsTr("Gallery")
                color: Theme.textPrimary
                font.pixelSize: 16
                font.bold: true
            }
            ToolButton {
                text: "\u00d7"
                Accessible.name: qsTr("Close")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Close")
                onClicked: gallery.close()
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            BusyIndicator {
                anchors.centerIn: parent
                running: ReaderViewModel.galleryLoading
                visible: running
            }

            Label {
                anchors.centerIn: parent
                visible: !ReaderViewModel.galleryLoading && ReaderViewModel.galleryImages.length === 0
                text: qsTr("No Images")
                color: Theme.textSecondary
            }

            ListView {
                id: sections
                anchors.fill: parent
                anchors.margins: Theme.spacingMd
                clip: true
                enabled: !preview.visible
                spacing: Theme.spacingMd
                visible: !ReaderViewModel.galleryLoading && ReaderViewModel.galleryImages.length > 0
                model: {
                    const groups = []
                    const images = ReaderViewModel.galleryImages
                    for (let i = 0; i < images.length; ++i) {
                        const title = images[i].chapter || qsTr("Untitled Chapter")
                        let group = groups.length > 0 ? groups[groups.length - 1] : null
                        if (!group || group.chapterIndex !== images[i].chapterIndex) {
                            group = { title: title, chapterIndex: images[i].chapterIndex, images: [] }
                            groups.push(group)
                        }
                        group.images.push({ image: images[i], index: i })
                    }
                    return groups
                }
                delegate: ColumnLayout {
                    id: chapterSection
                    required property var modelData
                    width: ListView.view.width
                    spacing: Theme.spacingSm

                    Label {
                        Layout.fillWidth: true
                        text: chapterSection.modelData.title
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }

                    GridView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.ceil(count / Math.max(1, Math.floor(width / cellWidth))) * cellHeight
                        cellWidth: 112
                        cellHeight: 112
                        interactive: false
                        model: chapterSection.modelData.images
                        delegate: Item {
                            id: thumbnail
                            required property var modelData
                            width: 106
                            height: 106
                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: thumbnail.modelData.image.path.split("/").pop()
                            Accessible.onPressAction: gallery.showPreview(thumbnail.modelData.index)
                            Keys.onReturnPressed: gallery.showPreview(thumbnail.modelData.index)
                            Keys.onSpacePressed: gallery.showPreview(thumbnail.modelData.index)

                            Rectangle {
                                anchors.fill: parent
                                color: Theme.windowBackground
                                border.width: gallery.seenPaths[thumbnail.modelData.image.path] ? 3 : 1
                                border.color: thumbnail.activeFocus || gallery.seenPaths[thumbnail.modelData.image.path]
                                    ? Theme.accent : Theme.cardStroke
                                scale: hover.hovered ? 1.05 : 1
                                Behavior on scale { NumberAnimation { duration: 160 } }
                                transform: Scale {
                                    origin.x: 53
                                    origin.y: 53
                                    xScale: tap.pressed ? 0.95 : 1
                                    yScale: xScale
                                    Behavior on xScale { NumberAnimation { duration: 120 } }
                                }
                                clip: true
                                Image {
                                    anchors.centerIn: parent
                                    width: 100
                                    height: 100
                                    sourceSize: Qt.size(200, 200)
                                    source: thumbnail.modelData.image.url
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                }
                            }
                            HoverHandler { id: hover }
                            TapHandler {
                                id: tap
                                onTapped: gallery.showPreview(thumbnail.modelData.index)
                            }
                        }
                    }
                }
            }

            Item {
                id: preview
                anchors.fill: parent
                visible: gallery.previewIndex >= 0 && gallery.previewIndex < ReaderViewModel.galleryImages.length
                z: 2

                function navigate(offset) {
                    const target = gallery.previewIndex + offset
                    if (target < 0 || target >= ReaderViewModel.galleryImages.length)
                        return
                    gallery.showPreview(target)
                }

                Rectangle { anchors.fill: parent; color: Theme.windowBackground }
                Image {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMd
                    source: preview.visible
                        ? ReaderViewModel.galleryImages[gallery.previewIndex].url : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }
                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    ToolButton {
                        text: "\u2190"
                        enabled: gallery.previewIndex > 0
                        Accessible.name: qsTr("Previous")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Previous")
                        onClicked: preview.navigate(-1)
                    }
                    Item { Layout.fillWidth: true }
                    ToolButton {
                        text: "\u2192"
                        enabled: gallery.previewIndex >= 0
                            && gallery.previewIndex < ReaderViewModel.galleryImages.length - 1
                        Accessible.name: qsTr("Next")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Next")
                        onClicked: preview.navigate(1)
                    }
                }
            }
        }
    }

    onClosed: previewIndex = -1
    onOpened: galleryBody.forceActiveFocus()

    Connections {
        target: ReaderViewModel
        function onBookChanged() {
            gallery.close()
            gallery.seenPaths = ({})
        }
    }
}

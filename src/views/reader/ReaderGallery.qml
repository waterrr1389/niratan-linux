// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's Gallery, following Features/Reader/Gallery/GalleryView.swift
// (body, imageCountText, sectionHeader, thumbnail, GalleryImagePreview) and
// NativeMac/NativeReaderView.swift (the .gallery overlay). It covers the whole
// reader above the side panels: a Gallery header whose subtitle is absent with
// no images, else "<count> images", scrolling chapter sections with pinned
// headers and an adaptive grid of 250...380 px columns with 16 px gaps.
//
// ReaderImageGalleryIndex places each image at a character; an image at or
// before the reading position is read. Until the user reveals it, an unread
// thumbnail is blurred by 18 with an eye-slash badge and is labelled "Unread
// Image"; its preview is blurred by 36 and a click on the preview reveals only
// that image. The preview covers the gallery including the header; its 38 px
// xmark closes only the preview, and Previous and Next (44 px, 32 px inset)
// stay disabled at the ends. Escape dismisses the preview first, then the
// gallery, and browsing never moves the chapter.
//
// Swift's CoverImage sizing and shadow are flattened with the other glass
// surfaces; the unread state comes from the reader model rather than a
// separate indexer.

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import Niratan

pragma ComponentBehavior: Bound

Item {
    id: gallery

    objectName: "readerGallery"

    property bool opened: false
    property int previewIndex: -1
    property var revealed: ({})

    signal closeRequested()

    readonly property var images: ReaderViewModel.galleryImages
    readonly property int imageCount: images.length

    visible: opened
    focus: true
    z: 100

    function isRevealed(path) {
        return gallery.revealed[path] === true;
    }

    function isUnread(image) {
        return image && image.isRead === false && !gallery.isRevealed(image.path);
    }

    function showPreview(index) {
        if (index < 0 || index >= gallery.imageCount)
            return;
        gallery.previewIndex = index;
        preview.forceActiveFocus();
    }

    function closePreview() {
        gallery.previewIndex = -1;
        gallery.forceActiveFocus();
    }

    function revealCurrent() {
        if (gallery.previewIndex < 0 || gallery.previewIndex >= gallery.imageCount)
            return;
        const path = gallery.images[gallery.previewIndex].path;
        const updated = Object.assign({}, gallery.revealed);
        updated[path] = true;
        gallery.revealed = updated;
    }

    function handleEscape() {
        if (gallery.previewIndex >= 0)
            closePreview();
        else
            closeRequested();
    }

    onOpenedChanged: {
        if (opened) {
            previewIndex = -1;
            revealed = ({});
            stickyIndex = 0;
            forceActiveFocus();
        }
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            gallery.handleEscape();
            event.accepted = true;
        } else if (preview.visible && event.modifiers === Qt.NoModifier) {
            if (event.key === Qt.Key_Left) {
                preview.navigate(-1);
                event.accepted = true;
            } else if (event.key === Qt.Key_Right) {
                preview.navigate(1);
                event.accepted = true;
            }
        }
    }

    property int stickyIndex: 0

    Rectangle {
        anchors.fill: parent
        color: ReaderViewModel.hasBook ? ReaderViewModel.pageBackground
                                       : Theme.windowBackground
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PanelHeader {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingMd
            Layout.rightMargin: Theme.spacingMd
            Layout.topMargin: Theme.galleryHeaderTopInset
            title: qsTr("Gallery")
            subtitle: gallery.imageCount > 0
                      ? qsTr("%1 images").arg(gallery.imageCount) : ""
            onCloseRequested: gallery.closeRequested()
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListStatus {
                horizontalInset: Theme.galleryInset
                busy: ReaderViewModel.galleryLoading
                visible: busy || gallery.imageCount === 0
                text: busy ? "" : qsTr("No Images")
                symbol: "photo.on.rectangle"
            }

            InspectorList {
                id: sections
                anchors.fill: parent
                anchors.leftMargin: Theme.galleryInset
                anchors.rightMargin: Theme.galleryInset
                anchors.bottomMargin: Theme.galleryInset
                enabled: !preview.visible
                spacing: Theme.gallerySectionSpacing
                visible: !ReaderViewModel.galleryLoading && gallery.imageCount > 0
                model: {
                    const groups = [];
                    const images = gallery.images;
                    for (let i = 0; i < images.length; ++i) {
                        const title = images[i].chapter || qsTr("Untitled Chapter");
                        let group = groups.length > 0 ? groups[groups.length - 1] : null;
                        if (!group || group.chapterIndex !== images[i].chapterIndex) {
                            group = { title: title, chapterIndex: images[i].chapterIndex,
                                      images: [] };
                            groups.push(group);
                        }
                        group.images.push({ image: images[i], index: i });
                    }
                    return groups;
                }
                onContentYChanged: {
                    const index = sections.indexAt(sections.contentX, sections.contentY);
                    if (index >= 0)
                        gallery.stickyIndex = index;
                }

                delegate: ColumnLayout {
                    id: chapterSection
                    required property var modelData
                    width: ListView.view.width
                    spacing: Theme.spacingLg

                    ListSectionHeader {
                        Layout.fillWidth: true
                        title: chapterSection.modelData.title
                        countText: String(chapterSection.modelData.images.length)
                    }

                    GridView {
                        id: grid
                        Layout.fillWidth: true
                        readonly property int columns: Math.max(1,
                            Math.floor((width + gap) / (Theme.galleryColumnMinimum + gap)))
                        readonly property real gap: Theme.spacingLg
                        Layout.preferredHeight: Math.ceil(
                            count / columns) * cellHeight
                        cellWidth: (width - (columns - 1) * gap) / columns
                        cellHeight: Math.max(Theme.galleryThumbnailMinHeight,
                                             Math.min(Theme.galleryThumbnailMaxHeight, cellWidth * 0.75))
                        interactive: false
                        boundsBehavior: Flickable.StopAtBounds
                        model: chapterSection.modelData.images

                        delegate: Item {
                            id: thumbnail
                            required property var modelData
                            width: grid.cellWidth - grid.gap
                            height: grid.cellHeight - grid.gap
                            objectName: "galleryThumbnail"
                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: gallery.isUnread(thumbnail.modelData.image)
                                             ? qsTr("Unread Image")
                                             : thumbnail.modelData.image.path.split("/").pop()
                            Accessible.onPressAction: gallery.showPreview(
                                                          thumbnail.modelData.index)
                            Keys.onReturnPressed: gallery.showPreview(
                                                      thumbnail.modelData.index)
                            Keys.onSpacePressed: gallery.showPreview(
                                                     thumbnail.modelData.index)

                            Rectangle {
                                anchors.fill: parent
                                radius: Theme.galleryThumbnailRadius
                                color: Theme.galleryThumbnailBackground
                                clip: true

                                Image {
                                    id: thumbnailImage
                                    readonly property bool unread:
                                        gallery.isUnread(thumbnail.modelData.image)
                                    readonly property bool shaderBlur:
                                        GraphicsInfo.api !== GraphicsInfo.Software
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingXs
                                    // Without a shader the software renderer keeps a
                                    // downsampled image, which still hides the unread art.
                                    sourceSize: unread && !shaderBlur
                                                ? Qt.size(24, 24) : Qt.size(1600, 1600)
                                    source: thumbnail.modelData.image.url
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    layer.enabled: unread && shaderBlur
                                    layer.effect: MultiEffect {
                                        blurEnabled: true
                                        blurMax: 36
                                        blur: thumbnailImage.unread ? 0.5 : 0
                                    }
                                }
                            }

                            MediaCue {
                                visible: gallery.isUnread(thumbnail.modelData.image)
                                anchors.centerIn: parent
                                symbol: "eye.slash.fill"
                            }

                            TapHandler {
                                onTapped: gallery.showPreview(thumbnail.modelData.index)
                            }
                        }
                    }
                }
            }

            // The pinned chapter header stays over the grid while a section
            // scrolls under it.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: pinnedHeader.implicitHeight
                visible: sections.visible && sections.contentY > 0
                color: ReaderViewModel.hasBook ? ReaderViewModel.pageBackground
                                               : Theme.windowBackground

                ListSectionHeader {
                    id: pinnedHeader

                    readonly property var group: sections.model[gallery.stickyIndex]
                    anchors.fill: parent
                    anchors.leftMargin: Theme.galleryInset
                    anchors.rightMargin: Theme.galleryInset
                    title: group ? group.title : ""
                    countText: group ? String(group.images.length) : ""
                }
            }
        }
    }

    Item {
        id: preview
        objectName: "galleryPreview"
        anchors.fill: parent
        z: 2
        visible: gallery.previewIndex >= 0 && gallery.previewIndex < gallery.imageCount
        focus: visible

        function navigate(offset) {
            const target = gallery.previewIndex + offset;
            if (target < 0 || target >= gallery.imageCount)
                return;
            gallery.showPreview(target);
        }

        Rectangle {
            anchors.fill: parent
            color: ReaderViewModel.hasBook ? ReaderViewModel.pageBackground
                                           : Theme.windowBackground
        }

        Image {
            id: previewImage
            readonly property bool unread: preview.visible
                && gallery.isUnread(gallery.images[gallery.previewIndex])
            readonly property bool shaderBlur: GraphicsInfo.api !== GraphicsInfo.Software
            anchors.fill: parent
            anchors.margins: Theme.spacingXl
            sourceSize: unread && !shaderBlur ? Qt.size(32, 32) : Qt.size(4096, 4096)
            source: preview.visible ? gallery.images[gallery.previewIndex].url : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            layer.enabled: unread && shaderBlur
            layer.effect: MultiEffect {
                blurEnabled: true
                blurMax: 36
                blur: previewImage.unread ? 1 : 0
            }
        }

        // NativeFullscreenImageView.onReveal: a click reveals this image only.
        TapHandler {
            onTapped: gallery.revealCurrent()
        }

        // GalleryImagePreview's unread cue: a large-title eye-slash in a circle.
        // It carries no input handling, so a tap reaches the reveal above.
        MediaCue {
            anchors.centerIn: parent
            large: true
            symbol: "eye.slash.fill"
            visible: previewImage.unread
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("Unread Image")
        }

        IconButton {
            objectName: "galleryPreviewClose"
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: Theme.spacingXl
            anchors.rightMargin: Theme.spacingXl
            icon: "xmark"
            diameter: Theme.galleryPreviewCloseDiameter
            iconSize: Theme.galleryPreviewCloseGlyph
            accessibleName: qsTr("Close")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: gallery.closePreview()
        }

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Theme.galleryNavigationInset
            anchors.rightMargin: Theme.galleryNavigationInset

            IconButton {
                objectName: "galleryPrevious"
                icon: "chevron.left"
                diameter: Theme.galleryNavigationDiameter
                iconSize: Theme.galleryNavigationGlyph
                accessibleName: qsTr("Previous")
                ToolTip.visible: hovered
                ToolTip.text: accessibleName
                enabled: gallery.previewIndex > 0
                opacity: enabled ? 1 : Theme.disabledOpacity
                onClicked: preview.navigate(-1)
            }
            Item { Layout.fillWidth: true }
            IconButton {
                objectName: "galleryNext"
                icon: "chevron.right"
                diameter: Theme.galleryNavigationDiameter
                iconSize: Theme.galleryNavigationGlyph
                accessibleName: qsTr("Next")
                ToolTip.visible: hovered
                ToolTip.text: accessibleName
                enabled: gallery.previewIndex >= 0
                         && gallery.previewIndex < gallery.imageCount - 1
                opacity: enabled ? 1 : Theme.disabledOpacity
                onClicked: preview.navigate(1)
            }
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// File drops over the player, following
// Features/Video/VideoPlayerScreen+Opening.swift `handleDroppedItems`,
// `handleDroppedFileURLs` and `loadDroppedMedia`,
// Features/Video/VideoPlayerScreen.swift's `.onDrop` on `observedContent`,
// `subtitleFileExtensions` and `loadDroppedSubtitle`, and
// Features/Video/VideoMediaTypes.swift `supportedExtensions`/`isMediaFile`.
//
// The first media file wins and takes the first subtitle file from the same
// drop; without a media file, a subtitle loads into the video that is already
// open. Any other file, a remote URL, or a drop over an open lookup popup does
// nothing. Extensions are compared case-insensitively.
//
// Object names: playerDropArea.

import QtQuick
import Niratan

Item {
    id: handler

    required property Item dropTarget
    required property var supportedExtensions
    property url videoFile
    property bool blocked: false

    signal mediaDropped(url media, url subtitle)
    signal subtitleDropped(url file)

    anchors.fill: dropTarget
    z: 1.5

    function extensionOf(url) {
        const text = String(url)
        if (text.indexOf("file:") !== 0)
            return ""
        const withoutFragment = text.split("#")[0].split("?")[0]
        const encoded = withoutFragment.substring(withoutFragment.lastIndexOf("/") + 1)
        let name = encoded
        try {
            name = decodeURIComponent(encoded)
        } catch (error) {
            name = encoded
        }
        const dot = name.lastIndexOf(".")
        return dot >= 0 ? name.substring(dot + 1).toLowerCase() : ""
    }

    function isMediaFile(url) {
        return handler.supportedExtensions.indexOf(handler.extensionOf(url)) >= 0
    }

    function isSubtitleFile(url) {
        return ["srt", "vtt", "ass", "ssa"].indexOf(handler.extensionOf(url)) >= 0
    }

    function handleDroppedUrls(urls) {
        if (!urls || urls.length === undefined)
            return
        let mediaUrl = null
        let subtitleUrl = null
        for (let index = 0; index < urls.length; ++index) {
            const url = urls[index]
            if (String(url).indexOf("file:") !== 0)
                continue
            if (mediaUrl === null && handler.isMediaFile(url))
                mediaUrl = url
            if (subtitleUrl === null && handler.isSubtitleFile(url))
                subtitleUrl = url
        }
        if (mediaUrl !== null) {
            handler.mediaDropped(mediaUrl, subtitleUrl === null ? "" : subtitleUrl)
        } else if (subtitleUrl !== null && handler.videoFile.toString().length > 0) {
            handler.subtitleDropped(subtitleUrl)
        }
    }

    DropArea {
        objectName: "playerDropArea"
        anchors.fill: parent
        enabled: !handler.blocked
        onDropped: function (drop) {
            if (!drop.hasUrls || drop.urls.length === 0) {
                drop.accepted = false
                return
            }
            let fileUrls = 0
            for (let index = 0; index < drop.urls.length; ++index) {
                if (String(drop.urls[index]).indexOf("file:") === 0)
                    ++fileUrls
            }
            if (fileUrls === 0) {
                drop.accepted = false
                return
            }
            handler.handleDroppedUrls(drop.urls)
            drop.acceptProposedAction()
        }
    }
}

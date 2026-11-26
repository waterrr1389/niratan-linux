// SPDX-License-Identifier: GPL-3.0-or-later
//
// The screenshot button's capture and save panel, following
// Features/Video/VideoControlsView.swift `screenshotButton`/`canSaveScreenshot`
// and Features/Video/VideoPlayerScreen.swift `saveCleanScreenshot`.
//
// A capture is taken before the panel opens, so the frame cannot move while
// the user chooses a destination: mpv writes its video-only still into a
// temporary PNG, then the chosen file receives a copy. The default name is the
// video stem with ":" replaced by "-" and the current seconds with three
// decimals. A failed capture or write reports `failed`; accepting the panel
// reports `saved`; a cancelled panel discards the capture and reports neither.
//
// `canSave` is VideoControlsView.canSaveScreenshot: loaded, settled, with a
// known display size and no capture already running.
// Object names: playerScreenshot for the controller, screenshotFileDialog for
// the save panel.

import QtQuick
import QtQuick.Dialogs
import Niratan

Item {
    id: controller

    objectName: "playerScreenshot"
    required property MpvItem engine
    property url videoFile
    readonly property bool canSave: engine.loaded && !engine.seeking
        && engine.videoDisplaySize.width > 0 && engine.videoDisplaySize.height > 0
        && !controller.saving
    property bool saving: false
    property url pendingCapture
    property string pendingDestination
    property string pendingName

    signal saved()
    signal failed()

    visible: false

    // VideoPlayerScreen.saveCleanScreenshot's file name.
    function fileName(seconds) {
        const text = String(controller.videoFile)
        const withoutFragment = text.split("#")[0].split("?")[0]
        const encoded = withoutFragment.substring(withoutFragment.lastIndexOf("/") + 1)
        let stem = encoded
        try {
            stem = decodeURIComponent(encoded)
        } catch (error) {
            stem = encoded
        }
        const dot = stem.lastIndexOf(".")
        if (dot > 0)
            stem = stem.substring(0, dot)
        stem = stem.replace(/:/g, "-")
        const time = isFinite(seconds) ? Math.max(0, seconds) : 0
        return stem + "-" + time.toFixed(3) + ".png"
    }

    function videoDirectory() {
        const text = String(controller.videoFile)
        const slash = text.lastIndexOf("/")
        return slash > 0 ? text.substring(0, slash) : ""
    }

    function joinFileUrl(folder, name) {
        if (folder.length === 0)
            return name
        return folder.replace(/\/+$/, "") + "/" + name
    }

    // The button: capture, then open the panel on success.
    function requestScreenshot() {
        if (!controller.canSave)
            return
        controller.beginCapture("")
    }

    // saveCleanScreenshot(destination): capture first, then write the chosen
    // file; an empty destination is ignored.
    function saveCleanScreenshot(destination) {
        if (!controller.canSave)
            return
        const target = destination === undefined || destination === null
            ? "" : String(destination)
        if (target.length === 0)
            return
        controller.beginCapture(target)
    }

    function beginCapture(destination) {
        controller.saving = true
        controller.pendingDestination = destination
        controller.pendingName = controller.fileName(controller.engine.position)
        controller.pendingCapture = controller.engine.temporaryScreenshotFile()
        if (!controller.engine.screenshot(controller.pendingCapture)) {
            controller.pendingCapture = ""
            controller.reportFailure()
        }
    }

    function handleCaptured(file, ok) {
        if (!controller.saving)
            return
        if (String(file) !== String(controller.pendingCapture))
            return
        if (!ok) {
            controller.reportFailure()
            return
        }
        if (controller.pendingDestination.length > 0) {
            controller.write(controller.pendingDestination)
            return
        }
        const folder = dialog.currentFolder.toString()
        const base = folder.length > 0 ? folder : controller.videoDirectory()
        dialog.selectedFile = controller.joinFileUrl(base, controller.pendingName)
        dialog.open()
    }

    function write(destination) {
        const written = String(destination).length > 0
            && controller.engine.writeScreenshotFile(controller.pendingCapture, destination)
        if (!written && controller.pendingCapture.toString().length > 0)
            controller.engine.writeScreenshotFile(controller.pendingCapture, "")
        controller.pendingCapture = ""
        controller.pendingDestination = ""
        controller.pendingName = ""
        controller.saving = false
        if (written)
            controller.saved()
        else
            controller.failed()
    }

    // The panel was dismissed: remove the temporary capture, keep quiet.
    function cancelSave() {
        if (controller.pendingCapture.toString().length > 0)
            controller.engine.writeScreenshotFile(controller.pendingCapture, "")
        controller.pendingCapture = ""
        controller.pendingDestination = ""
        controller.pendingName = ""
        controller.saving = false
    }

    function reportFailure() {
        if (controller.pendingCapture.toString().length > 0)
            controller.engine.writeScreenshotFile(controller.pendingCapture, "")
        controller.pendingCapture = ""
        controller.pendingDestination = ""
        controller.pendingName = ""
        controller.saving = false
        controller.failed()
    }

    Connections {
        target: controller.engine

        function onScreenshotSaved(file, ok) {
            controller.handleCaptured(file, ok)
        }
    }

    FileDialog {
        id: dialog

        objectName: "screenshotFileDialog"
        title: qsTr("Save Screenshot")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG images (*.png)")]
        defaultSuffix: "png"
        onAccepted: controller.write(selectedFile)
        onRejected: controller.cancelSave()
    }
}

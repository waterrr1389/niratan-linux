// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader window, following NativeMac/ReaderWindowPresenter.swift
// `makeWindow` and NativeMac/ReaderWindowGeometry.swift `minimumSize`: an
// 1100x760 window with a 720x520 minimum, titled with the open book's title,
// holding ReaderPage across the whole surface. MainWindow opens or raises this
// single window after a book opens; the back control closes it and closing
// marks the reader inactive for the statistics context. The window has no
// native menu bar, and Qt keeps no restorable frame state for it.

import QtQuick
import QtQuick.Controls
import Niratan

ApplicationWindow {
    id: readerWindow

    width: 1100
    height: 760
    minimumWidth: 720
    minimumHeight: 520
    visible: false
    title: ReaderViewModel.title
    color: Theme.windowBackground

    ReaderPage {
        anchors.fill: parent

        onBackRequested: readerWindow.close()
    }

    onClosing: {
        ReaderViewModel.setReaderActive(false)
        // flushAutoSync: a close after a bookmark write exports through the
        // sync model; prepareForClose consumes the pending export so a later
        // close without another write sends nothing.
        const folder = ReaderViewModel.bookFolder
        const edited = ReaderViewModel.bookmarkEdited
        ReaderViewModel.prepareForClose()
        if (folder.length > 0)
            SyncSettingsViewModel.syncReaderClose(folder, edited)
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The title of a sidebar column in its toolbar band, as Mac Niratan names an
// inner library column (Features/Video/VideoLibraryView.swift
// `VideoLibrarySidebarView`, Features/Bookshelf/LibraryShelfSidebar.swift):
// bold on one line, vertically centred in the toolbar height. `leadingInset`
// adds room for window controls drawn over the band.
import QtQuick
import Niratan

StyledLabel {
    property real leadingInset: 0

    x: Theme.sourceListTitleInset + leadingInset
    width: parent ? parent.width - x - Theme.sourceListTitleTrailing : 0
    height: Theme.sourceListToolbarHeight
    verticalAlignment: Text.AlignVCenter
    textStyle: Theme.title3Font
    weight: Theme.weightBold
    elide: Text.ElideRight
}

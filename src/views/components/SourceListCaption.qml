// SPDX-License-Identifier: GPL-3.0-or-later
//
// A section caption of a sidebar source list, as SwiftUI draws a
// List Section header in the sidebar style (Features/Video/VideoLibraryView.swift
// `VideoLibrarySidebarView`): subheadline semibold in the secondary colour.
// The first caption sits close under the toolbar; later ones leave a gap
// above to separate their group.
import QtQuick
import Niratan

StyledLabel {
    property bool first: false

    leftPadding: Theme.sourceListInset
    topPadding: first ? Theme.sourceListInset : Theme.sourceListSectionGap
    bottomPadding: first ? Theme.sourceListRowSpacing : Theme.spacingXs
    textStyle: Theme.subheadlineFont
    weight: Theme.weightSemibold
    secondary: true
}

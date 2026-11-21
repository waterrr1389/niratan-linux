// SPDX-License-Identifier: GPL-3.0-or-later
//
// The delete confirmation of the statistics dashboard, following the three
// confirmationDialog blocks in Features/Bookshelf/StatisticsDashboardView.swift
// (body) and their deleteSelectedSession, deleteSelectedDay and
// deleteAllStatistics actions: the title and message depend on the scope, and
// the destructive action is confirmed before the owner performs the deletion.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Dialog {
    id: dialog

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: parent ? Math.min(420, parent.width - 32) : 420
    modal: true
    standardButtons: Dialog.Cancel | Dialog.Ok

    // 0 deletes the selected session, 1 the selected day, 2 every recorded day.
    property int scope: 0
    // The selection the action refers to, supplied by the dashboard.
    property string sessionId
    property string dateKey

    function ask(value) {
        scope = value
        open()
    }

    title: scope === 0 ? qsTr("Delete This Session") : scope === 1
        ? qsTr("Delete This Day") : qsTr("Delete All Statistics")
    contentItem: Label {
        text: dialog.scope === 0 ? qsTr("Delete the selected reading session? This cannot be undone.")
            : dialog.scope === 1 ? qsTr("Delete the selected day's statistics? This cannot be undone.")
            : qsTr("Delete all statistics for this book? This cannot be undone.")
        wrapMode: Text.WordWrap
    }
}

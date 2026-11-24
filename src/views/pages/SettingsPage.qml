// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeSettingsReuseView and NativeSettingsDetailView.
//
// The main window keeps this page and its Appearance form alive after the
// first visit, so entering Settings again is immediate. enter() restores what
// NativeSettingsReuseView shows on appearing: Appearance selected, scrolled to
// the top, the source list focused. leave() closes any menu, popup or dialog
// the page opened so nothing stays up behind another section, and returns to
// Appearance. The other forms keep their own lifetime: created when selected,
// destroyed on deselection or when Settings is left.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

RowLayout {
    id: page

    property real toolbarInset: 0
    readonly property real columnWidth: Math.min(240, Math.max(180, width * 0.28))

    // Every section except Appearance; null while Appearance is selected.
    readonly property Component otherForm: {
        switch (sidebar.selected) {
        case 0: return profiles
        case 2: return dictionaries
        case 3: return anki
        case 4: return statistics
        case 5: return sasayaki
        case 6: return wordAudio
        case 7: return syncSettings
        default: return null
        }
    }

    // False between leave() and enter(), while another section shows.
    property bool shown: true

    objectName: "settingsPage"
    spacing: 0

    // The kept Appearance form re-reads the active profile whenever it
    // appears again, as a profile switch may have happened meanwhile.
    onOtherFormChanged: {
        if (page.shown && page.otherForm === null)
            appearanceForm.settingsModel.reload()
    }

    Component.onCompleted: sidebar.forceActiveFocus()

    function enter() {
        page.shown = true
        sidebar.selected = 1
        appearanceForm.settingsModel.reload()
        const flick = appearanceForm.flickable
        flick.contentY = flick.originY
        sidebar.forceActiveFocus()
    }

    // Selecting Appearance also ends the lifetime of any other open form, as
    // leaving Settings did when the whole page was destroyed.
    function leave() {
        page.shown = false
        page.closeTransient(page)
        sidebar.selected = 1
    }

    // Closes open popups, menus and dialogs among the page's objects.
    function closeTransient(object) {
        const owned = object.data ?? object.contentData ?? []
        for (let index = 0; index < owned.length; ++index) {
            const child = owned[index]
            if (!child)
                continue
            if (typeof child.close === "function" && child.visible === true)
                child.close()
            page.closeTransient(child)
        }
    }

    SettingsSectionRail {
        id: sidebar

        Layout.minimumWidth: page.columnWidth
        Layout.preferredWidth: page.columnWidth
        Layout.maximumWidth: page.columnWidth
        Layout.fillHeight: true
        toolbarInset: page.toolbarInset
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 0

        // Match the leading column's toolbar; the form clips below it.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: sidebar.toolbarHeight
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            AppearanceSettings {
                id: appearanceForm

                anchors.fill: parent
                visible: page.otherForm === null
            }

            Loader {
                anchors.fill: parent
                active: page.otherForm !== null
                sourceComponent: page.otherForm
            }
        }
    }

    Component { id: profiles; ProfilesPage {} }
    Component { id: dictionaries; DictionarySettingsPage {} }
    Component { id: anki; AnkiSettingsPage {} }
    Component { id: statistics; StatisticsSettings {} }
    Component { id: sasayaki; SasayakiSettingsForm {} }
    Component { id: wordAudio; WordAudioSettings {} }
    Component { id: syncSettings; SyncSettings {} }
}

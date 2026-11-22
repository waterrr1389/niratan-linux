// SPDX-License-Identifier: GPL-3.0-or-later
//
// The five Sasayaki shortcuts, following NativeMac/NativeReaderView.swift
// (sasayakiShortcutHandlers, canHandleSasayakiShortcut, toggleSasayakiPlayback,
// playPreviousSasayakiCue, playNextSasayakiCue, replaySasayakiCue and
// jumpToSasayakiCue) and Features/Sasayaki/SasayakiShortcutActions.swift:
// `[` previous cue, `p` play/pause, `]` next cue, `r` replay the popup cue and
// `j` jump to it.
//
// The page's `active` already covers canHandleSasayakiShortcut's scope: the
// Sasayaki sheet is allowed, no other reader sheet is open, and Sasayaki is
// enabled. The actions need loaded audio as well; `r` and `j` also need the
// lookup popup's cue. `p` clears the lookup's remembered pause before toggling,
// as toggleSasayakiPlayback does. The page hands its unhandled keys to
// handleKey, which reports whether it consumed the key.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

QtObject {
    id: shortcuts

    // ReaderLookupSurface: the player, the popup cue, the remembered pause and
    // the three control actions it owns.
    required property var surface
    property bool active: false

    readonly property var player: surface.player

    function handleKey(event): bool {
        if (!active || !player.hasAudio || event.modifiers !== Qt.NoModifier)
            return false
        if (event.key === Qt.Key_BracketLeft) {
            player.previousCue()
            event.accepted = true
            return true
        }
        if (event.key === Qt.Key_P) {
            surface.togglePlayback()
            event.accepted = true
            return true
        }
        if (event.key === Qt.Key_BracketRight) {
            player.nextCue()
            event.accepted = true
            return true
        }
        if (event.key === Qt.Key_R) {
            if (!surface.hasPopupCue)
                return false
            surface.replayCue()
            event.accepted = true
            return true
        }
        if (event.key === Qt.Key_J) {
            if (!surface.hasPopupCue)
                return false
            surface.jumpToCue()
            event.accepted = true
            return true
        }
        return false
    }
}

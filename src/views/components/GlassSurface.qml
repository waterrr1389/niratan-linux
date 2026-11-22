// SPDX-License-Identifier: GPL-3.0-or-later
//
// A rounded solid surface for cards and panels, following
// NativeMac/NativeGlassSurface.swift `nativeGlassCardSurface(cornerRadius:)` and
// NativeGlassCardSurfaceModifier. It fills the card tint, strokes it with the
// hairline, clips its content and draws nothing else: PLAN.md keeps glass a
// solid theme surface, so the blur, shadow and gradient of the Swift modifier
// are not reproduced.
//
// `cornerRadius` is the shape's radius and defaults to Theme.cardRadius; the
// stroke width is Theme.cardBorderWidth. A page places its own content in the
// default slot.
//
// Qt Quick clips an item to its bounding rectangle, and a rounded mask needs a
// shader, which the software renderer used for review does not draw. The
// surface therefore clips rectangularly; the cards keep their content inside
// the radius with their own padding, so nothing reaches the corner notches.

import QtQuick
import Niratan

Item {
    id: surface

    property int cornerRadius: Theme.cardRadius
    readonly property int borderWidth: Theme.cardBorderWidth
    default property alias content: contentItem.data

    Rectangle {
        id: shape

        anchors.fill: parent
        radius: surface.cornerRadius
        color: Theme.cardBackground
        border.width: surface.borderWidth
        border.color: Theme.cardStroke
        clip: true

        Item {
            id: contentItem

            anchors.fill: parent
        }
    }
}

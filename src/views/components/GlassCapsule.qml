// SPDX-License-Identifier: GPL-3.0-or-later
//
// A capsule solid surface for the search field and menu pickers, following
// NativeMac/NativeGlassSurface.swift `nativeGlassCapsuleSurface()` and
// NativeGlassCapsuleSurfaceModifier. The radius is always half the height and
// follows height changes; the fill and hairline stroke are the card tint and
// the stroke token. PLAN.md keeps glass solid, so there is no blur or shadow.
//
// The capsule clips rectangularly for the same reason as GlassSurface: a
// rounded clip needs a shader, which the software renderer used for review
// does not draw. Its content sits well inside the radius.

import QtQuick
import Niratan

Item {
    id: capsule

    readonly property real radius: height / 2
    readonly property int borderWidth: Theme.cardBorderWidth
    default property alias content: contentItem.data

    Rectangle {
        anchors.fill: parent
        radius: capsule.radius
        color: Theme.cardBackground
        border.width: capsule.borderWidth
        border.color: Theme.cardStroke
        clip: true

        Item {
            id: contentItem

            anchors.fill: parent
        }
    }
}

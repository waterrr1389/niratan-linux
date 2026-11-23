// SPDX-License-Identifier: GPL-3.0-or-later
//
// A grouped card in the Inspector, following VideoInspectorView.swift
// `inspectorSection` and `VideoInspectorSectionGlassSurface`: the kit's
// compact FormSection, named for the Inspector tests.
//
// Object names: inspectorSection; `sectionTitle` holds the heading text and
// `headerAccessory` takes the trailing header item (an episode count, the
// current speed or a Reset link). `footer` is drawn only when non-empty.

import QtQuick
import Niratan

FormSection {
    id: section

    property alias sectionTitle: section.title

    objectName: "inspectorSection"
    compact: true
}

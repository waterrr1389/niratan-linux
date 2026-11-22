// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video_library.json codec shared by the store's load and save paths.
// Module-private: nothing outside services/video includes it.
//
// The document holds sources, items, remoteItems (always written empty),
// itemMetadataByPath and collections. Dates are seconds since 2001-01-01 UTC,
// UUIDs are uppercase strings without braces, and the macOS bookmark is
// neither read nor written. Decoding fails as a whole, as Swift's throwing
// decoder does, so a damaged document reads as the empty catalog.

#pragma once

#include <QJsonObject>

#include <optional>

#include "services/video/library_catalog.h"

namespace niratan::video::detail {

QJsonObject catalog_to_json(const LibraryCatalog& catalog);
std::optional<LibraryCatalog> catalog_from_json(const QJsonObject& object);

}  // namespace niratan::video::detail

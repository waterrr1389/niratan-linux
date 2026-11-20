// SPDX-License-Identifier: GPL-3.0-or-later
//
// MIME types for dictionary media, following the ImageHandler.mimeType
// switch in Features/Popup/PopupWebView.swift.
//
// The extension is the last path component's suffix after its final dot,
// lowercased; png, jpg, jpeg, gif, webp, avif, heic and svg map to their image
// types and everything else is application/octet-stream. A name without a
// dot, a dot as the first character or a trailing dot has no extension.
// Swift lowercases with Unicode rules; this port folds ASCII, which differs
// only for scalars that fold into ASCII.

#pragma once

#include <string_view>

namespace niratan::dictionary {

std::string_view dictionary_media_mime_type(std::string_view path);

}  // namespace niratan::dictionary

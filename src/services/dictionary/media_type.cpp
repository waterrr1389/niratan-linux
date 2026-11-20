// SPDX-License-Identifier: GPL-3.0-or-later

#include "media_type.h"

#include <cstddef>
#include <string_view>

namespace niratan::dictionary {

std::string_view dictionary_media_mime_type(std::string_view path) {
    // Only the last path component carries the extension, and a dot at
    // offset 0 or at the end of that component does not start one.
    const auto slash = path.rfind('/');
    const auto component = slash == std::string_view::npos ? path : path.substr(slash + 1);
    const auto dot = component.rfind('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 == component.size()) {
        return "application/octet-stream";
    }

    const auto extension = component.substr(dot + 1);
    const auto matches = [extension](std::string_view name) {
        if (extension.size() != name.size()) {
            return false;
        }
        for (std::size_t i = 0; i < extension.size(); ++i) {
            char folded = extension[i];
            if (folded >= 'A' && folded <= 'Z') {
                folded = static_cast<char>(folded - 'A' + 'a');
            }
            if (folded != name[i]) {
                return false;
            }
        }
        return true;
    };

    if (matches("png")) {
        return "image/png";
    }
    if (matches("jpg") || matches("jpeg")) {
        return "image/jpeg";
    }
    if (matches("gif")) {
        return "image/gif";
    }
    if (matches("webp")) {
        return "image/webp";
    }
    if (matches("avif")) {
        return "image/avif";
    }
    if (matches("heic")) {
        return "image/heic";
    }
    if (matches("svg")) {
        return "image/svg+xml";
    }
    return "application/octet-stream";
}

}  // namespace niratan::dictionary

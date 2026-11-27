// SPDX-License-Identifier: GPL-3.0-or-later
//
// Bytes and MIME type served by the reader's URL scheme.

#pragma once

#include <string>

namespace niratan::reader {

struct SessionResource {
    std::string bytes;
    std::string mime_type;
};

}  // namespace niratan::reader

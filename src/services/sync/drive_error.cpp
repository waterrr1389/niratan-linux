// SPDX-License-Identifier: GPL-3.0-or-later

#include "drive_error.h"

namespace niratan::sync {

std::string GoogleDriveError::description() const {
    if (kind == GoogleDriveErrorKind::invalid_response) {
        return "Invalid response from Google Drive";
    }
    return message;
}

bool GoogleDriveError::is_stale_cache_error() const {
    return kind == GoogleDriveErrorKind::api_error && status_code == 404;
}

}  // namespace niratan::sync

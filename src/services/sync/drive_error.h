// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveHandler.swift: GoogleDriveError.errorDescription and isStaleCacheError.
// Only an API error with HTTP status 404 invalidates the cached folder IDs.

#pragma once

#include <optional>
#include <string>

namespace niratan::sync {

enum class GoogleDriveErrorKind { invalid_response, api_error, unavailable };

struct GoogleDriveError {
    GoogleDriveErrorKind kind = GoogleDriveErrorKind::invalid_response;
    std::string message;
    std::optional<int> status_code;

    std::string description() const;
    bool is_stale_cache_error() const;
    bool operator==(const GoogleDriveError&) const = default;
};

}  // namespace niratan::sync

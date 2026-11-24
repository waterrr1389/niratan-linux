// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveAuthorizationPolicy.swift: scope grants and credential invalidation.
// GoogleDriveAuth.swift: isValidGoogleClientId (does not trim).

#pragma once

#include <QByteArrayView>

#include <optional>
#include <string>
#include <string_view>

namespace niratan::sync {

inline constexpr std::string_view file_scope = "https://www.googleapis.com/auth/drive.file";
inline constexpr std::string_view library_scope = "https://www.googleapis.com/auth/drive";
inline constexpr std::string_view requested_scope = file_scope;

bool permits_stored_scope(std::optional<std::string_view> scope);
bool scope_includes(std::optional<std::string_view> scope, std::string_view required);
std::optional<std::string> granted_scope(std::optional<std::string_view> response,
                                         std::string_view requested);
bool invalidates_stored_credentials(std::optional<int> status_code, QByteArrayView body);
bool valid_client_id(std::string_view client_id);

}  // namespace niratan::sync

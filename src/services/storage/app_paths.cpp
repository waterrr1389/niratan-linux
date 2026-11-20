// SPDX-License-Identifier: GPL-3.0-or-later

#include "app_paths.h"

#include <cstdlib>

namespace niratan::storage {
namespace {

// The base directory specification discards an empty or relative value, so only
// a non-empty absolute path selects the root; otherwise HOME is the fallback.
std::filesystem::path root_from(const char* variable, const char* home_suffix) {
    if (const char* value = std::getenv(variable); value != nullptr && *value != '\0') {
        const std::filesystem::path candidate(value);
        if (candidate.is_absolute()) {
            return candidate / "niratan";
        }
    }

    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') {
        return {};
    }
    return std::filesystem::path(home) / home_suffix / "niratan";
}

}  // namespace

std::filesystem::path data_root() {
    return root_from("XDG_DATA_HOME", ".local/share");
}

std::filesystem::path cache_root() {
    return root_from("XDG_CACHE_HOME", ".cache");
}

std::optional<std::filesystem::path> resolve_within(const std::filesystem::path& root,
                                                    std::string_view relative) {
    const std::filesystem::path candidate(relative);
    if (candidate.is_absolute()) {
        return std::nullopt;
    }
    // ".." is rejected wherever it appears, even when the normal form would
    // still stay below the root, so a stored path can never climb out.
    for (const std::filesystem::path& component : candidate) {
        if (component == "..") {
            return std::nullopt;
        }
    }
    return (root / candidate).lexically_normal();
}

}  // namespace niratan::storage

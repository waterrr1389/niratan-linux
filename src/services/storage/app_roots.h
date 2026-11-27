// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPEC.md 3.5: XDG_DATA_HOME/niratan and XDG_CACHE_HOME/niratan. An unset
// or relative XDG base falls back to .local/share or .cache under HOME.
// An unusable HOME leaves the fallback empty. Neither root is created here.

#pragma once

#include <filesystem>

namespace niratan::storage {

std::filesystem::path data_root();
std::filesystem::path cache_root();

}  // namespace niratan::storage

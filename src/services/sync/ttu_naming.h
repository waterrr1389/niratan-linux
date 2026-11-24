// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Sync/TtuSyncNaming.swift: sanitize, desanitize and statisticsFileName.
// For UTF-8 titles, mark a final space, then a final dot in the result, then
// replace stars, then percent-encode each remaining reserved scalar as %HH
// with uppercase hex. The reserved scalars are ASCII and matched bytewise.
// Percent-decode the entire name, or retain it unchanged on failure, then
// always substitute the star, dot and space markers in that order.
// Statistics time and averages use Swift's shortest round-trip Double
// description (including .0 for fixed-form integral values). Non-finite
// values and magnitudes at or above 1e16 are outside the supported range.

#pragma once

#include "models/statistics/statistics.h"

#include <span>
#include <string>
#include <string_view>

namespace niratan::sync {

std::string sanitize_ttu_title(std::string_view title);
std::string desanitize_ttu_title(std::string_view title);
std::string ttu_statistics_file_name(std::span<const statistics::Statistics> rows);

}  // namespace niratan::sync

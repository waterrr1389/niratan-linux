// SPDX-License-Identifier: GPL-3.0-or-later
//
// Result codes shared by the settings stores.

#pragma once

namespace niratan::settings {

// missing: no file, the defaults apply. invalid: the file exists but cannot
// be read or decoded; the defaults apply and the file is left untouched.
enum class LoadStatus { missing, success, invalid };

enum class StoreError { none, unsafe_profile_id, unknown_profile, io_error };

}  // namespace niratan::settings

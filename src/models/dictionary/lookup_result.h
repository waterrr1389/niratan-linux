// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

namespace niratan::dictionary {

// hoshidicts types stay inside services/dictionary. Callers see only these.

struct LookupEntry {
    std::string expression;
    std::string reading;
    std::string matched;
};

struct LookupOutcome {
    bool ok = false;
    std::string error;
    std::vector<LookupEntry> entries;
};

}  // namespace niratan::dictionary

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Settings/AnkiView.swift (availableHandlebars) and Models/Anki.swift
// (Handlebars.allCases, singleGlossaryPrefix): menu values in declaration
// order, excluding the hidden variants, then one literal single-glossary
// value per term dictionary, in input order. Titles are not escaped. The
// separate clear item ("-") is not a template and is not in this list.

#pragma once

#include <string>
#include <vector>

#include "models/anki/mining_context.h"

namespace niratan::anki {

std::vector<std::string> mapping_options(const std::vector<TermDictionary>& dictionaries);

}  // namespace niratan::anki

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Glossary HTML clean-up for card fields, matching Core/AnkiManager.swift
// (stripGlossaryHeaders, stripDictionaryName).
//
// Glossary HTML is a list whose items start with a header:
//   <li data-dictionary="NAME"><i>LABEL</i> 
// The header is the tag, the label in <i>, and one space after </i>. NAME is
// any run of characters except '"', LABEL any run except '<'. Both may be
// empty here. Matching is leftmost and non-overlapping, and text that is not
// part of a match is copied unchanged.
//
// strip_glossary_headers replaces every header by its <li data-dictionary="NAME">
// tag, so the label and the space are gone.
//
// strip_dictionary_name only touches headers whose NAME is not empty. It
// removes the dictionary from the label: every ", NAME)" in LABEL becomes
// ")". NAME is matched literally. If the new label is exactly "(NAME)" the
// header is replaced by the bare <li data-dictionary="NAME"> tag, otherwise
// by <li data-dictionary="NAME"><i>NEW LABEL</i> with its space.

#pragma once

#include <string>
#include <string_view>

namespace niratan::anki {

std::string strip_glossary_headers(std::string_view html);

std::string strip_dictionary_name(std::string_view html);

}  // namespace niratan::anki

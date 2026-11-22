// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where Niratan keeps its data on Linux, following SPEC.md 3.5 and the
// internal layout of Niratan-win's Helpers/AppDataHelper.cs.
//
// The data root is $XDG_DATA_HOME/niratan, or $HOME/.local/share/niratan when
// XDG_DATA_HOME is unset or relative. The cache root is $XDG_CACHE_HOME/
// niratan, or $HOME/.cache/niratan. The XDG base directory specification
// requires relative values to be ignored, which is why only an absolute path
// is accepted. An empty result means neither the XDG variable nor HOME was
// usable; this function does not create directories.
//
// resolve_within() confines a stored relative path to a root. It rejects
// absolute paths and any ".." component and returns nullopt for them. "."
// components are removed by the normal form, so a path can never climb out of
// the root. Names are not otherwise validated or normalized.

#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "models/dictionary/dictionary_config.h"

namespace niratan::storage {

std::filesystem::path data_root();
std::filesystem::path cache_root();

// SPEC.md 3.5's internal layout: the imported dictionary directories.
std::filesystem::path dictionaries_root();

// The typed dictionary directory, <dictionaries_root>/<Type> (Term,
// Frequency, Pitch or Kanji); empty when the data root is empty.
std::filesystem::path dictionary_type_directory(dictionary::DictionaryType type);

// nullopt when the path is absolute or contains "..".
std::optional<std::filesystem::path> resolve_within(const std::filesystem::path& root,
                                                    std::string_view relative);

}  // namespace niratan::storage

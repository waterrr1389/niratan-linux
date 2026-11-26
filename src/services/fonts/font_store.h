// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's imported fonts, following Core/FontManager.swift
// (fontsDirectory, storedFonts, storedFontUrl, importFont, deleteFont,
// isDefaultFont, defaultFonts, cssFontName, resourceName and fontfaceCSS) and
// the copy helpers in Core/BookStorage.swift (copySecurityScopedFile,
// copyFile, replaceFile).
//
// Files
// -----
// Fonts live in <app directory>/Fonts. A stored font's name is its file name
// without the extension, so "My Font.otf" is the font "My Font". The stored
// list is the regular files of that directory in name order, skipping hidden
// entries and the "System" directory Swift stores downloaded fonts in; a
// directory whose name looks like a font is not a font. Swift's
// contentsOfDirectory would list it as one; the port ignores non-files because
// it can neither serve nor delete them.
//
// Import
// ------
// import_font copies the source under its own file name into Fonts, creating
// the directory, and replaces an existing file of that name byte for byte. A
// missing or unreadable source, a directory, or an empty file name is refused
// before anything is written; `error` carries a message for the page. Only
// unreadable bytes or an unwritable destination fail after validation, and
// then nothing is written.
//
// Delete
// ------
// delete_font removes a stored font's file by name. A name that is not stored,
// a directory or a default font changes nothing and reports success, as
// Swift's storedFontUrl lookup and isDefaultFont guard do. `false` with a
// message means the file existed but could not be removed.
//
// Stylesheet and serving
// ----------------------
// font_face_css is FontManager.fontfaceCSS with only the imported fonts this
// build has (the downloadable fonts are not ported): one block per stored font
// in list order, the family its base name, and the source
// reader://font/<percent-encoded file name>. Blocks follow each other directly
// and there is no block without a stored font.
//
// font_resource serves one stored file by its exact file name for the reader
// scheme: no path separator, no parent component, and only the font
// extensions Swift's import accepts, ttf, otf, ttc, woff and woff2, each with
// its font MIME type. Anything else is not a font resource.
//
// Swift's cssFontName falls back to the name for an imported font, and
// resourceName percent-encodes the file name; both apply unchanged here.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::fonts {

// The MIME type for a stored font's extension, empty when it is not one of the
// five font extensions.
std::string_view font_mime_type(std::string_view file_name);

struct FontResource {
    std::string bytes;
    std::string mime_type;

    bool operator==(const FontResource&) const = default;
};

class FontStore {
   public:
    // FontManager.defaultFonts, in order; [0] is the delete fallback.
    static std::vector<std::string_view> default_fonts();
    static bool is_default_font(std::string_view name);

    explicit FontStore(std::filesystem::path app_directory);

    // <app directory>/Fonts.
    std::filesystem::path directory() const;

    // The stored font files in name order, with the directory created first as
    // storedFonts does. Hidden entries, "System" and non-files are skipped.
    std::vector<std::filesystem::path> stored_fonts() const;
    // Their names, the file name without the extension.
    std::vector<std::string> stored_font_names() const;
    // The first stored file with this base name.
    std::optional<std::filesystem::path> stored_font(std::string_view name) const;

    bool import_font(const std::filesystem::path& source, std::string* error = nullptr) const;
    // A no-op (true) for a name that is not a stored font or is a default.
    bool delete_font(std::string_view name, std::string* error = nullptr) const;

    // FontManager.fontfaceCSS for the stored fonts here.
    std::string font_face_css() const;

    std::optional<FontResource> font_resource(std::string_view file_name) const;

   private:
    std::filesystem::path app_directory_;
};

}  // namespace niratan::fonts

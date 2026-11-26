// SPDX-License-Identifier: GPL-3.0-or-later

#include "font_store.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <utility>

namespace niratan::fonts {
namespace {

constexpr std::string_view kDownloadableFonts[] = {
    "Klee", "Tsukushi A Round Gothic", "YuKyokasho", "YuMincho", "YuGothic",
};

std::string lower_extension(std::string_view file_name) {
    const std::size_t dot = file_name.find_last_of('.');
    if (dot == std::string_view::npos) {
        return {};
    }
    std::string extension(file_name.substr(dot + 1));
    for (char& character : extension) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return extension;
}

// Foundation's .urlPathAllowed for a file name: the RFC 3986 path characters
// stay as they are, every other byte, including each UTF-8 byte of a
// non-ASCII name, becomes an uppercase %XX escape.
std::string percent_encode(std::string_view value) {
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (const unsigned char character : value) {
        const bool allowed = std::isalnum(character) != 0 || character == '-' || character == '.' ||
            character == '_' || character == '~' || character == '!' || character == '$' ||
            character == '&' || character == '\'' || character == '(' || character == ')' ||
            character == '*' || character == '+' || character == ',' || character == ';' ||
            character == '=' || character == ':' || character == '@' || character == '/';
        if (allowed) {
            encoded.push_back(static_cast<char>(character));
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex[character >> 4]);
            encoded.push_back(kHex[character & 0x0F]);
        }
    }
    return encoded;
}

bool read_file(const std::filesystem::path& path, std::string* bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    bytes->assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !input.bad();
}

}  // namespace

std::string_view font_mime_type(std::string_view file_name) {
    const std::string extension = lower_extension(file_name);
    if (extension == "ttf") {
        return "font/ttf";
    }
    if (extension == "otf") {
        return "font/otf";
    }
    if (extension == "ttc") {
        return "font/collection";
    }
    if (extension == "woff") {
        return "font/woff";
    }
    if (extension == "woff2") {
        return "font/woff2";
    }
    return {};
}

std::vector<std::string_view> FontStore::default_fonts() {
    return {"Hiragino Mincho ProN", "Hiragino Kaku Gothic ProN"};
}

bool FontStore::is_default_font(std::string_view name) {
    const auto defaults = default_fonts();
    if (std::find(defaults.begin(), defaults.end(), name) != defaults.end()) {
        return true;
    }
    return std::find(std::begin(kDownloadableFonts), std::end(kDownloadableFonts), name) !=
        std::end(kDownloadableFonts);
}

FontStore::FontStore(std::filesystem::path app_directory)
    : app_directory_(std::move(app_directory)) {}

std::filesystem::path FontStore::directory() const {
    return app_directory_ / "Fonts";
}

std::vector<std::filesystem::path> FontStore::stored_fonts() const {
    const std::filesystem::path folder = directory();
    std::error_code error;
    std::filesystem::create_directories(folder, error);

    std::vector<std::filesystem::path> fonts;
    std::filesystem::directory_iterator entry(folder, error);
    const std::filesystem::directory_iterator end;
    while (!error && entry != end) {
        const std::filesystem::path path = entry->path();
        const std::string name = path.filename().string();
        std::error_code type_error;
        if (!name.empty() && name.front() != '.' && name != "System" &&
            entry->is_regular_file(type_error) && !type_error) {
            fonts.push_back(path);
        }
        entry.increment(error);
    }

    std::sort(fonts.begin(), fonts.end(), [](const auto& left, const auto& right) {
        return left.filename().string() < right.filename().string();
    });
    return fonts;
}

std::vector<std::string> FontStore::stored_font_names() const {
    std::vector<std::string> names;
    for (const auto& path : stored_fonts()) {
        names.push_back(path.stem().string());
    }
    return names;
}

std::optional<std::filesystem::path> FontStore::stored_font(std::string_view name) const {
    for (const auto& path : stored_fonts()) {
        if (path.stem().string() == name) {
            return path;
        }
    }
    return std::nullopt;
}

bool FontStore::import_font(const std::filesystem::path& source, std::string* error) const {
    const auto fail = [error](std::string_view message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    if (error != nullptr) {
        error->clear();
    }
    if (source.empty() || source.filename().empty()) {
        return fail("The font file name is empty.");
    }
    std::error_code error_code;
    if (!std::filesystem::is_regular_file(source, error_code) || error_code) {
        return fail("The selected font file could not be read.");
    }
    std::string bytes;
    if (!read_file(source, &bytes)) {
        return fail("The selected font file could not be read.");
    }

    const std::filesystem::path folder = directory();
    std::filesystem::create_directories(folder, error_code);
    if (error_code) {
        return fail("The Fonts folder could not be created.");
    }
    const std::filesystem::path destination = folder / source.filename();
    const std::filesystem::path temporary = folder / ("." + source.filename().string() + ".import");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            return fail("The font file could not be saved.");
        }
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output) {
            std::filesystem::remove(temporary, error_code);
            return fail("The font file could not be saved.");
        }
    }
    std::filesystem::rename(temporary, destination, error_code);
    if (error_code) {
        std::filesystem::remove(temporary, error_code);
        return fail("The font file could not be saved.");
    }
    return true;
}

bool FontStore::delete_font(std::string_view name, std::string* error) const {
    if (error != nullptr) {
        error->clear();
    }
    if (is_default_font(name)) {
        return true;
    }
    const auto font = stored_font(name);
    if (!font.has_value()) {
        return true;
    }
    std::error_code error_code;
    std::filesystem::remove(*font, error_code);
    if (error_code) {
        if (error != nullptr) {
            *error = "The font file could not be deleted.";
        }
        return false;
    }
    return true;
}

std::string FontStore::font_face_css() const {
    std::string css;
    for (const auto& path : stored_fonts()) {
        css += "@font-face {\n";
        css += "    font-family: '" + path.stem().string() + "';\n";
        css += "    src: url('reader://font/" + percent_encode(path.filename().string()) + "');\n";
        css += "}";
    }
    return css;
}

std::optional<FontResource> FontStore::font_resource(std::string_view file_name) const {
    if (file_name.empty() || file_name.find('/') != std::string_view::npos ||
        file_name.find('\\') != std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view mime_type = font_mime_type(file_name);
    if (mime_type.empty()) {
        return std::nullopt;
    }
    for (const auto& path : stored_fonts()) {
        if (path.filename().string() != std::string(file_name)) {
            continue;
        }
        std::string bytes;
        if (!read_file(path, &bytes)) {
            return std::nullopt;
        }
        return FontResource{std::move(bytes), std::string(mime_type)};
    }
    return std::nullopt;
}

}  // namespace niratan::fonts

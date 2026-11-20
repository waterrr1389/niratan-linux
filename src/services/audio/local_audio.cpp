// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/audio/local_audio.h"

#include <array>
#include <cstddef>

namespace niratan::audio {

namespace {

// Extensions that the local audio table may serve, in the order the query
// filter lists them.
constexpr std::array<std::string_view, 7> kSupportedAudioExtensions{
    "mp3", "opus", "ogg", "m4a", "aac", "wav", "flac"};

// Characters that .urlPathAllowed keeps besides alphanumerics.
constexpr std::string_view kPathLiterals = "-._~!$&'()*+,;=:@/";

std::string_view path_extension(std::string_view file) {
    const std::size_t slash = file.rfind('/');
    const std::string_view name = slash == std::string_view::npos ? file : file.substr(slash + 1);
    const std::size_t dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 == name.size()) {
        return {};
    }
    return name.substr(dot + 1);
}

std::string ascii_lowercased(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char character : text) {
        if (character >= 'A' && character <= 'Z') {
            lowered += static_cast<char>(character - 'A' + 'a');
        } else {
            lowered += character;
        }
    }
    return lowered;
}

void append_utf8(std::string& text, char32_t scalar) {
    if (scalar < 0x80) {
        text += static_cast<char>(scalar);
    } else if (scalar < 0x800) {
        text += static_cast<char>(0xC0 | (scalar >> 6));
        text += static_cast<char>(0x80 | (scalar & 0x3F));
    } else if (scalar < 0x10000) {
        text += static_cast<char>(0xE0 | (scalar >> 12));
        text += static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (scalar & 0x3F));
    } else {
        text += static_cast<char>(0xF0 | (scalar >> 18));
        text += static_cast<char>(0x80 | ((scalar >> 12) & 0x3F));
        text += static_cast<char>(0x80 | ((scalar >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (scalar & 0x3F));
    }
}

std::string supported_file_filter() {
    std::string filter;
    for (const std::string_view extension : kSupportedAudioExtensions) {
        if (!filter.empty()) {
            filter += " OR ";
        }
        filter += "LOWER(file) LIKE '%.";
        filter += extension;
        filter += "'";
    }
    return filter;
}

std::string source_rank() {
    std::string rank = "CASE source ";
    for (std::size_t index = 0; index < default_sources().size(); ++index) {
        rank += "WHEN ? THEN " + std::to_string(index) + " ";
    }
    rank += "ELSE 999 END";
    return rank;
}

std::string select_row(bool has_reading) {
    std::string sql = "SELECT source, file FROM entries\nWHERE ";
    if (has_reading) {
        sql += "(expression = ? OR reading = ?) AND (";
        sql += supported_file_filter();
        sql += ")\nORDER BY CASE\n"
               "    WHEN expression = ? AND reading = ? THEN 0\n"
               "    WHEN reading = ? THEN 1\n"
               "    WHEN expression = ? THEN 2\n"
               "    ELSE 3\n"
               "END, ";
    } else {
        sql += "expression = ? AND (";
        sql += supported_file_filter();
        sql += ")\nORDER BY ";
    }
    sql += source_rank();
    sql += "\nLIMIT 1;";
    return sql;
}

// The bound text outlives the statement, so SQLITE_STATIC is enough.
void bind_text(sqlite3_stmt* statement, int index, std::string_view value) {
    sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                      SQLITE_STATIC);
}

}  // namespace

const std::array<std::string_view, 10>& default_sources() {
    static constexpr std::array<std::string_view, 10> kDefaultSources{
        "nhk16", "daijisen", "shinmeikai8", "jpod", "jpod_alternate",
        "taas", "ozk5", "forvo", "forvo_ext", "forvo_ext2"};
    return kDefaultSources;
}

bool is_supported_audio_file(std::string_view file) {
    const std::string extension = ascii_lowercased(path_extension(file));
    for (const std::string_view supported : kSupportedAudioExtensions) {
        if (std::string_view(extension) == supported) {
            return true;
        }
    }
    return false;
}

std::string katakana_to_hiragana(std::string_view text) {
    std::string converted;
    converted.reserve(text.size());

    std::size_t index = 0;
    while (index < text.size()) {
        const auto lead = static_cast<unsigned char>(text[index]);
        std::size_t length = 0;
        char32_t scalar = 0;
        if (lead < 0x80) {
            length = 1;
            scalar = lead;
        } else if ((lead & 0xE0) == 0xC0) {
            length = 2;
            scalar = lead & 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
            scalar = lead & 0x0F;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
            scalar = lead & 0x07;
        }

        bool valid = length != 0 && index + length <= text.size();
        for (std::size_t offset = 1; valid && offset < length; ++offset) {
            const auto continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0) != 0x80) {
                valid = false;
            } else {
                scalar = (scalar << 6) | (continuation & 0x3F);
            }
        }
        if (!valid) {
            converted += text[index];
            ++index;
            continue;
        }

        if (scalar >= 0x30A1 && scalar <= 0x30F6) {
            append_utf8(converted, scalar - 0x60);
        } else {
            converted.append(text.substr(index, length));
        }
        index += length;
    }

    return converted;
}

std::string percent_encode_path(std::string_view text) {
    constexpr std::string_view kHexDigits = "0123456789ABCDEF";

    std::string encoded;
    encoded.reserve(text.size());
    for (const char character : text) {
        const auto byte = static_cast<unsigned char>(character);
        const bool alphanumeric = (byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') ||
                                  (byte >= 'a' && byte <= 'z');
        if (alphanumeric || kPathLiterals.find(character) != std::string_view::npos) {
            encoded += character;
        } else {
            encoded += '%';
            encoded += kHexDigits[byte >> 4];
            encoded += kHexDigits[byte & 0x0F];
        }
    }
    return encoded;
}

std::string audio_mime_type(std::string_view file) {
    const std::string extension = ascii_lowercased(path_extension(file));
    if (extension == "mp3") {
        return "audio/mpeg";
    }
    if (extension == "opus") {
        return "audio/opus";
    }
    if (extension == "ogg") {
        return "audio/ogg";
    }
    if (extension == "m4a") {
        return "audio/mp4";
    }
    if (extension == "aac") {
        return "audio/aac";
    }
    if (extension == "wav") {
        return "audio/wav";
    }
    if (extension == "flac") {
        return "audio/flac";
    }
    return "application/octet-stream";
}

std::optional<LocalAudioEntry> find_local_audio(sqlite3* db, std::string_view term,
                                                std::string_view raw_reading) {
    const std::string reading = katakana_to_hiragana(raw_reading);
    const std::string sql = select_row(!reading.empty());

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        sqlite3_finalize(statement);
        return std::nullopt;
    }

    bind_text(statement, 1, term);
    int next_index = 2;
    if (!reading.empty()) {
        bind_text(statement, 2, reading);
        bind_text(statement, 3, term);
        bind_text(statement, 4, reading);
        bind_text(statement, 5, reading);
        bind_text(statement, 6, term);
        next_index = 7;
    }
    const std::array<std::string_view, 10>& sources = default_sources();
    for (std::size_t index = 0; index < sources.size(); ++index) {
        bind_text(statement, next_index + static_cast<int>(index), sources[index]);
    }

    std::optional<LocalAudioEntry> entry;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* source = sqlite3_column_text(statement, 0);
        const int source_size = sqlite3_column_bytes(statement, 0);
        const unsigned char* file = sqlite3_column_text(statement, 1);
        const int file_size = sqlite3_column_bytes(statement, 1);
        if (source != nullptr && file != nullptr) {
            entry = LocalAudioEntry{
                std::string(reinterpret_cast<const char*>(source),
                            static_cast<std::size_t>(source_size)),
                std::string(reinterpret_cast<const char*>(file),
                            static_cast<std::size_t>(file_size))};
        }
    }
    sqlite3_finalize(statement);
    return entry;
}

std::string local_audio_url(std::string_view source, std::string_view file) {
    std::string url = "http://localhost:";
    url += std::to_string(kLocalAudioPort);
    url += "/localaudio/";
    url += source;
    url += '/';
    url += percent_encode_path(file);
    return url;
}

}  // namespace niratan::audio

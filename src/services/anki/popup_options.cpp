// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/popup_options.h"

#include <algorithm>
#include <string_view>

namespace niratan::anki {
namespace {

constexpr std::string_view audio_handlebar = "{audio}";

}  // namespace

PopupOptions popup_options(const AnkiSettings& settings) {
    PopupOptions options;
    options.needs_audio = std::ranges::any_of(
        settings.field_mappings, [](const auto& field) { return field.second == audio_handlebar; });
    options.allow_dupes = settings.allow_dupes;
    options.use_anki_connect = true;
    options.embed_media = settings.embed_media;
    options.compact_glossaries = settings.compact_glossaries;
    return options;
}

}  // namespace niratan::anki

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Builds the document the reader loads for one chapter, following
// NativeMac/NativeReaderView.swift (injectReader) and the Reader/ScrollReader
// web view controllers.
//
// The chapter's own XHTML is kept as it is; this module inserts two blocks:
// the stylesheet from services/reader/reader_css.h before </head>, and one
// script before </body> that sets the reader variables, installs the page
// viewport, appends the padding spacer, and then evaluates the original
// selection, reader, highlights and paragraph scripts. When the document has
// no head or body markers the blocks are prepended or appended instead.
//
// The scripts are the upstream files verbatim, loaded from Qt resources.
// They talk to the platform through window.webkit.messageHandlers;
// the generated preamble installs that object on top of the QWebChannel
// bridge and queues messages until the channel is ready.
//
// Values injected (same names and defaults as injectReader):
//   page width/height, horizontal page columns, horizontal spread page size,
//   content language, scan length, hover delay, shared sync coordinates,
//   restore token, writing mode through the stylesheet, and the initial
//   restore call (a fragment jump or a progress restore).
//
// NativeReaderView.injectReader sizes illustrations, not the publisher: every
// svg loses preserveAspectRatio="none", and every img that is not gaiji and
// larger than 256 px in either natural dimension gains block-img so the
// stylesheet caps it by its own ratio. With UserConfig.blurImages each such
// image is wrapped and blurred, and a click reveals it; any other illustration
// reports its URL through imageTapped. The host does not yet open an image
// viewer, so imageTapped has no reader.
// After fonts and images are ready, paginated paragraph mode prepares its
// layout before highlights and the initial restore expression are applied.

#pragma once

#include <string>

#include "services/reader/reader_css.h"

namespace niratan::reader {

struct ShellInput {
    Appearance appearance;
    PageSize page;
    Colors colors;
    std::string font_face_css;

    // Reader interaction settings from UserConfig.
    bool wheel_page_turn = true;
    // UserConfig.scanNonJapaneseText defaults to true.
    bool scan_non_japanese_text = true;
    int scan_length = 0;
    int hover_delay_ms = 0;
    std::string content_language = "ja";
    bool shared_sync_coordinates = false;

    // Distinguishes restores of the same page; the web view echoes it back.
    std::string restore_token;

    // NativeReaderView.pageLayoutKey: measure page starts only when pages are
    // shown and at least one progress line uses them.
    bool measure_page_starts = false;

    // A JavaScript expression evaluated once fonts are ready, such as
    // "window.hoshiReader.restoreProgress(0.25)". Empty does nothing.
    std::string initial_restore_expression;
    // Serialized current-spine highlights, with HTML delimiters escaped.
    std::string highlights_json;
};

struct ReaderScripts {
    std::string selection;
    std::string reader;
    std::string scrollreader;
    std::string highlights;
    std::string paragraph;
    // qwebchannel.js, so the page can reach the bridge without a network load.
    std::string webchannel;
};

// Loads the reader scripts from Qt resources. Call once at startup.
ReaderScripts load_reader_scripts();

// The chapter with the stylesheet and script inserted. `scripts.reader` is
// used unless appearance.continuous selects `scripts.scrollreader`.
std::string reader_document(const std::string& chapter_html, const ShellInput& input,
                            const ReaderScripts& scripts);

// The stylesheet and script blocks alone, for tests and for reloads that only
// replace the script.
std::string reader_style_block(const ShellInput& input);
std::string reader_script_block(const ShellInput& input, const ReaderScripts& scripts);

}  // namespace niratan::reader

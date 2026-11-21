// SPDX-License-Identifier: GPL-3.0-or-later
//
// A two-chapter EPUB written for flow tests. The container is the smallest one
// services/novels/epub_book parses: a container rootfile, a package with a
// title, two manifest documents and a two-item spine. Each chapter document
// holds one paragraph, so its readable character count is exactly the kana in
// its text (services/reader/character_normalizer).
//
// The writer is shared by flows that need a countable book whose spine has a
// chapter boundary; a single-chapter fixture cannot exercise the mid-flow
// statistics save a chapter change performs.

#pragma once

#include <zip.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace niratan::test {

inline bool write_epub(const std::filesystem::path& path, std::string_view title,
                       std::string_view first_chapter, std::string_view second_chapter) {
    int error = 0;
    zip_t* archive = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (archive == nullptr) {
        return false;
    }

    const std::string opf =
        R"XML(<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
    <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
        <dc:title>)XML" +
        std::string(title) +
        R"XML(</dc:title>
    </metadata>
    <manifest>
        <item id="first" href="first.xhtml" media-type="application/xhtml+xml"/>
        <item id="second" href="second.xhtml" media-type="application/xhtml+xml"/>
    </manifest>
    <spine>
        <itemref idref="first"/>
        <itemref idref="second"/>
    </spine>
</package>
)XML";

    const auto chapter = [](std::string_view text) {
        return R"XML(<html><body><p>)XML" + std::string(text) + R"XML(</p></body></html>)XML";
    };

    const std::vector<std::pair<std::string, std::string>> entries{
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml",
         R"XML(<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0">
    <rootfiles>
        <rootfile full-path="book.opf" media-type="application/oebps-package+xml"/>
    </rootfiles>
</container>
)XML"},
        {"book.opf", opf},
        {"first.xhtml", chapter(first_chapter)},
        {"second.xhtml", chapter(second_chapter)},
    };

    for (const auto& [name, bytes] : entries) {
        zip_source_t* source = zip_source_buffer(archive, bytes.data(), bytes.size(), 0);
        if (source == nullptr || zip_file_add(archive, name.c_str(), source, 0) < 0) {
            if (source != nullptr) {
                zip_source_free(source);
            }
            zip_discard(archive);
            return false;
        }
    }
    if (zip_close(archive) < 0) {
        zip_discard(archive);
        return false;
    }
    return true;
}

}  // namespace niratan::test

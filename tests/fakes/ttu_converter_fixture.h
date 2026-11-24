// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <zip.h>

#include <QByteArray>

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <vector>

namespace ttu_converter_fixture {

struct Entry {
    std::string name;
    QByteArray bytes;
    int compression = ZIP_CM_DEFLATE;
};

inline bool write_zip(const std::filesystem::path& path, std::initializer_list<Entry> entries) {
    auto* archive = zip_open(path.c_str(), ZIP_CREATE | ZIP_EXCL, nullptr);
    if (!archive) {
        return false;
    }
    for (const auto& entry : entries) {
        auto* source = zip_source_buffer(archive, entry.bytes.constData(), entry.bytes.size(), 0);
        const auto index =
            source ? zip_file_add(archive, entry.name.c_str(), source, ZIP_FL_ENC_UTF_8) : -1;
        if (index < 0) {
            zip_source_free(source);
        }
        if (index < 0 || zip_set_file_compression(archive, index, entry.compression, 0) != 0) {
            zip_discard(archive);
            return false;
        }
    }
    if (zip_close(archive) != 0) {
        zip_discard(archive);
        return false;
    }
    return true;
}

inline std::optional<std::vector<Entry>> read_zip(const std::filesystem::path& path) {
    auto* archive = zip_open(path.c_str(), ZIP_RDONLY, nullptr);
    if (!archive) {
        return std::nullopt;
    }
    std::vector<Entry> entries;
    for (zip_int64_t i = 0; i < zip_get_num_entries(archive, 0); ++i) {
        zip_stat_t stat;
        if (zip_stat_index(archive, i, 0, &stat) != 0) {
            zip_discard(archive);
            return std::nullopt;
        }
        auto* file = zip_fopen_index(archive, i, 0);
        if (!file) {
            zip_discard(archive);
            return std::nullopt;
        }
        QByteArray bytes(static_cast<qsizetype>(stat.size), Qt::Uninitialized);
        const auto count = zip_fread(file, bytes.data(), stat.size);
        zip_fclose(file);
        if (count < 0 || static_cast<zip_uint64_t>(count) != stat.size) {
            zip_discard(archive);
            return std::nullopt;
        }
        entries.push_back({stat.name, std::move(bytes), stat.comp_method});
    }
    zip_discard(archive);
    return entries;
}

inline constexpr auto container =
    R"(<?xml version="1.0"?><container xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>)";

inline constexpr auto import_data = R"({
    "title":"A & B: 猫", "styleSheet":"p{color:red}",
    "elementHtml":"ignored<div id=\"ttu-ztoc\"><div class=\"ttu-book-html-wrapper top ttu-no-text\"><div class=\"ttu-book-body-wrapper main ttu-no-text\"><p>猫&nbsp;犬<br><hr/><img src=\"ttu:Images/pic.png\"><img src=\"data:image/gif;ttu:Images/cover.JPG;base64,AAA=\"><img src=\"ttu:Images/self.png\"/></p><svg xmlns=\"http://www.w3.org/2000/svg\"/></div></div></div><div id=\"ttu-a\"><p>鳥</p></div>",
    "sections":[{"reference":"ttu-a","charactersWeight":1,"parentChapter":"ttu-ztoc"},
                {"reference":"ttu-missing","charactersWeight":1},
                {"reference":"ttu-ztoc","charactersWeight":2,"label":"目次 & 本","startCharacter":0,"characters":2}],
    "ignored":true
})";

inline bool make_ttu(const std::filesystem::path& path) {
    return write_zip(path,
                     {{"staticdata.json", import_data},
                      {"blobs/Images/pic.png", "PNG"},
                      {"blobs/Images/cover.JPG", "JPEG"},
                      {"blobs/nested/a.GIF", "GIF"},
                      {"blobs/z.svg", "<svg/>"},
                      {"blobs/ignored.txt", "ignored"},
                      {"cover.jpg", "ROOT COVER"}});
}

inline bool make_epub(const std::filesystem::path& path, bool chapter = true, bool css = true) {
    const QByteArray opf = R"(<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Archive title</dc:title><dc:language>ja</dc:language></metadata>
<manifest>
<item id="css" href="style.css" media-type="text/css"/>
<item id="pic" href="Images/pic.png" media-type="image/png"/>
<item id="one" href="Text/one.xhtml" media-type="application/xhtml+xml"/>
<item id="two" href="Text/%74wo.xhtml" media-type="application/xhtml+xml"/>
<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
<item id="three" href="Text/three.xhtml" media-type="application/xhtml+xml"/>
<item id="one" href="Text/duplicate.xhtml" media-type="application/xhtml+xml"/>
<item id="extra" href="other.css" media-type="text/css"/>
<item id="jpeg" href="Images/gone.jpg" media-type="image/jpeg"/>
<item id="webp" href="Images/no.webp" media-type="image/webp"/>
</manifest><spine><itemref idref="one"/><itemref idref="two"/><itemref idref="three"/></spine></package>)";
    return write_zip(
        path,
        {{"mimetype", "application/epub+zip", ZIP_CM_STORE},
         {"META-INF/container.xml", container},
         {"OEBPS/book.opf", opf},
         {"OEBPS/nav.xhtml",
          R"(<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><body><nav epub:type="toc"><ol><li><span>Group</span><ol><li><a href="Text/two.xhtml#here">Part &amp; Two</a></li></ol></li></ol></nav></body></html>)"},
         {css ? "OEBPS/style.css" : "missing-css", "p{color:red}"},
         {"OEBPS/other.css", "body{margin:0}"},
         {"OEBPS/Images/pic.png", "PNG"},
         {"OEBPS/Images/no.webp", "WEBP"},
         {chapter ? "OEBPS/Text/one.xhtml" : "missing-chapter",
          R"(<html class=" top "><body class=" main "><p id="start">猫&nbsp;犬<br/><hr /><img src="../Images/./pic.png?x#frag"/><image xlink:href="/Images/pic.png#frag"></image><image href="../../Images/pic.png?x"></image></p><p>a<br>b<hr>c<img src="../Images/pic.png">d</p></body></html>)"},
         {"OEBPS/Text/two.xhtml", "<html><body><p id=\"here\"></p></body></html>"},
         {"OEBPS/Text/three.xhtml", "<html><body><p>鳥</p></body></html>"}});
}

}  // namespace ttu_converter_fixture

// SPDX-License-Identifier: GPL-3.0-or-later

#include "ttu_convert_xhtml.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace niratan::sync::detail {
namespace {

QString capture(const QString& text, const QString& pattern) {
    return QRegularExpression(pattern).match(text).captured(1);
}

QString escape_xml(QString text) {
    return text.replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;");
}

QString image_uri(const QString& src, const QString& chapter) {
    auto parts = chapter.split('/', Qt::SkipEmptyParts);
    if (src.startsWith('/')) {
        parts.clear();
    } else if (!parts.empty()) {
        parts.removeLast();
    }
    for (const auto& part :
         src.section('#', 0, 0).section('?', 0, 0).split('/', Qt::SkipEmptyParts)) {
        if (part == "..") {
            if (!parts.empty()) {
                parts.removeLast();
            }
        } else if (part != ".") {
            parts.append(part);
        }
    }
    return "data:image/gif;ttu:" + parts.join('/') +
        ";base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";
}

void rewrite(QString& html, const QString& pattern, const QString& chapter) {
    auto matches = QRegularExpression(pattern).globalMatch(html);
    qsizetype delta = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        const auto uri = image_uri(match.captured(1), chapter);
        html.replace(match.capturedStart(1) + delta, match.capturedLength(1), uri);
        delta += uri.size() - match.capturedLength(1);
    }
}

QString wrapper(QString base, const QString& classes, bool no_text) {
    if (const auto trimmed = classes.trimmed(); !trimmed.isEmpty()) {
        base += ' ' + trimmed;
    }
    if (no_text) {
        base += " ttu-no-text";
    }
    return base;
}

QString file_id(const XhtmlFile& file) {
    return file.name.chopped(6);
}

}  // namespace

QString export_element(QString reference, const QString& chapter, const QString& manifest_path,
                       bool no_text) {
    const auto html_classes = wrapper(
        "ttu-book-html-wrapper", capture(chapter, R"rx(<html\b[^>]*\bclass="([^"]*)")rx"), no_text);
    const auto body_classes = wrapper(
        "ttu-book-body-wrapper", capture(chapter, R"rx(<body\b[^>]*\bclass="([^"]*)")rx"), no_text);
    auto body = capture(chapter, R"(<body\b[^>]*>([\s\S]*)</body>)");
    rewrite(body, R"rx(<img\b[^>]*\bsrc="([^"]+)")rx", manifest_path);
    rewrite(body, R"rx(<image\b[^>]*\sxlink:href="([^"]+)")rx", manifest_path);
    rewrite(body, R"rx(<image\b[^>]*\shref="([^"]+)")rx", manifest_path);
    body.replace(QRegularExpression(R"(<(br|hr|img)\b([^>]*)/>)"), R"(<\1\2>)");
    return "<div id=\"" + reference + "\"><div class=\"" + html_classes + "\"><div class=\"" +
        body_classes + "\">" + body + "</div></div></div>";
}

std::vector<XhtmlFile> split_elements(QString html, const std::vector<Section>& sections) {
    html.replace(QRegularExpression(R"rx(data:image/[^;"]+;ttu:([^;"]+);base64,[^"]*)rx"),
                 R"(../\1)")
        .replace(QRegularExpression(R"rx(ttu:([^"']+))rx"), R"(../\1)")
        .replace(QRegularExpression(R"(<(br|hr)\b([^>/]*)>)"), R"(<\1\2/>)")
        .replace(QRegularExpression(R"((<img [^>]+)>)"), R"(\1/>)")
        .replace("&nbsp;", "&#160;");
    std::vector<std::pair<qsizetype, const Section*>> markers;
    for (const auto& section : sections) {
        const auto position = html.indexOf("<div id=\"" + section.reference + '"');
        if (position >= 0) {
            markers.emplace_back(position, &section);
        }
    }
    std::stable_sort(markers.begin(), markers.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<XhtmlFile> files;
    for (std::size_t i = 0; i < markers.size(); ++i) {
        const auto [start, section] = markers[i];
        const auto end = i + 1 < markers.size() ? markers[i + 1].first : html.size();
        files.push_back(
            {section->reference.mid(4) + ".xhtml", section->label, html.mid(start, end - start)});
    }
    return files;
}

QString xhtml(const XhtmlFile& file, const QString& title) {
    auto content = file.slice.mid(QString("<div id=\"ttu-" + file_id(file) + "\">").size());
    const auto wrappers =
        content.contains("ttu-book-html-wrapper") && content.contains("ttu-book-body-wrapper");
    content.chop(wrappers ? 18 : 6);
    const auto html_class =
        capture(content, R"rx(ttu-book-html-wrapper\s*([^"]*)")rx").remove("ttu-no-text").trimmed();
    const auto body_class =
        capture(content, R"rx(ttu-book-body-wrapper\s*([^"]*)")rx").remove("ttu-no-text").trimmed();
    content.remove(QRegularExpression(R"(<div class="ttu-book-html-wrapper[^"]*"[^>]*>)"))
        .remove(QRegularExpression(R"(<div class="ttu-book-body-wrapper[^"]*"[^>]*>)"));
    return QStringLiteral(R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html
 xmlns="http://www.w3.org/1999/xhtml"
 xmlns:epub="http://www.idpf.org/2007/ops"
 xml:lang="ja"
 class="%1"
>
<head>
<meta charset="UTF-8"/>
<title>%2</title>
<link rel="stylesheet" type="text/css" href="../stylesheet.css"/>
</head>
<body class="%3">
%4
</body>
</html>)")
        .arg(html_class, escape_xml(title), body_class, content);
}

QString navigation(const std::vector<XhtmlFile>& files) {
    QStringList items;
    QString toc;
    for (const auto& file : files) {
        if (file.label) {
            items.append("<li><a href=\"xhtml/" + file.name + "\">" + escape_xml(*file.label) +
                         "</a></li>");
        }
        if (toc.isEmpty() && file.name.contains("toc")) {
            toc = "<li><a epub:type=\"toc\" href=\"xhtml/" + file.name + "\">" +
                escape_xml(file.label.value_or("toc")) + "</a></li>";
        }
    }
    return QStringLiteral(R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html
 xmlns="http://www.w3.org/1999/xhtml"
 xmlns:epub="http://www.idpf.org/2007/ops"
 xml:lang="ja"
>
<head>
<meta charset="UTF-8"/>
<title>Navigation</title>
</head>
<body>
<nav epub:type="toc" id="toc">
<h1>Navigation</h1>
<ol>
%1
</ol>
</nav>

<nav epub:type="landmarks" id="guide">
<h1>Guide</h1>
<ol>
%2
</ol>
</nav>

</body>
</html>)")
        .arg(items.join('\n'), toc);
}

QString opf(const QStringList& images, const std::vector<XhtmlFile>& files, const QString& title,
            const QString& uuid) {
    QStringList image_items;
    for (const auto& path : images) {
        const QFileInfo file(path);
        const auto ext = file.suffix().toLower();
        const auto mime = ext == "svg" ? "image/svg+xml"
            : ext == "gif"             ? "image/gif"
            : ext == "png"             ? "image/png"
                                       : "image/jpeg";
        const auto name = file.completeBaseName();
        const bool cover = name == "cover";
        image_items.append(QString("<item media-type=\"") + mime + "\" id=\"" +
                           (cover ? name : "i-" + name) + "\" href=\"" + path + '"' +
                           (cover ? " properties=\"cover-image\"" : "") + "/>");
    }
    QStringList spine;
    auto sorted = files;
    std::ranges::stable_sort(sorted, {}, &XhtmlFile::name);
    QStringList chapters;
    for (const auto& file : sorted) {
        chapters.append("<item media-type=\"application/xhtml+xml\" id=\"" + file_id(file) +
                        "\" href=\"xhtml/" + file.name + '"' +
                        (file.slice.contains("<svg") ? " properties=\"svg\"" : "") + "/>");
    }
    for (const auto& file : files) {
        spine.append("<itemref linear=\"yes\" idref=\"" + file_id(file) + "\"/>");
    }
    return QStringLiteral(R"(<?xml version="1.0" encoding="UTF-8"?>
<package
 xmlns="http://www.idpf.org/2007/opf"
 version="3.0"
 xml:lang="ja"
 unique-identifier="book-uuid"
>

<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">

<!-- 作品名 -->
<dc:title id="title">%1</dc:title>

<!-- 言語 -->
<dc:language>ja</dc:language>

<!-- ファイルid -->
<dc:identifier id="book-uuid">%2</dc:identifier>

</metadata>

<manifest>

<!-- navigation -->
<item media-type="application/xhtml+xml" id="nav" href="navigation-documents.xhtml" properties="nav"/>

<!-- style -->
<item media-type="text/css" id="stylesheet" href="stylesheet.css"/>

<!-- image -->
%3

<!-- xhtml -->
%4

</manifest>

<spine page-progression-direction="rtl">

%5

</spine>

</package>)")
        .arg(escape_xml(title), uuid, image_items.join('\n'), chapters.join('\n'),
             spine.join('\n'));
}

}  // namespace niratan::sync::detail

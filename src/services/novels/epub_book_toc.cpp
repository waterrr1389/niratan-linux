// SPDX-License-Identifier: GPL-3.0-or-later

#include "epub_book_internal.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include <cstddef>
#include <optional>
#include <string>

namespace niratan::novels::detail {

namespace {

bool nav_is_toc(const QXmlStreamAttributes& attributes) {
    for (const QXmlStreamAttribute& attribute : attributes) {
        if (attribute.name() == QLatin1String("type") &&
            attribute.value() == QLatin1String("toc")) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool parse_nav(const std::string& xml, const std::string& package_dir,
               std::vector<EpubTocEntry>* toc, std::string* error) {
    QXmlStreamReader reader(QByteArray::fromStdString(xml));
    QStringList stack;
    // Stack size at the toc nav, negative while no toc nav is open.
    qsizetype toc_nav_depth = -1;
    int ol_depth = 0;
    bool finished = false;

    while (!reader.atEnd() && !finished) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: {
                const QString name = reader.name().toString();
                if (toc_nav_depth < 0) {
                    stack.push_back(name);
                    if (name == QLatin1String("nav") && nav_is_toc(reader.attributes())) {
                        toc_nav_depth = stack.size();
                    }
                    break;
                }
                if (name == QLatin1String("a")) {
                    const std::optional<std::string> href = resolve_href(
                        attribute_value(reader.attributes(), QStringLiteral("href")).toStdString(),
                        package_dir);
                    const int level = ol_depth > 0 ? ol_depth - 1 : 0;
                    const QString title =
                        reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    toc->push_back(
                        EpubTocEntry{level, title.toStdString(), href.value_or(std::string())});
                    break;
                }
                if (name == QLatin1String("ol")) {
                    ++ol_depth;
                }
                stack.push_back(name);
                break;
            }
            case QXmlStreamReader::EndElement: {
                if (stack.isEmpty()) {
                    break;
                }
                const QString name = stack.last();
                stack.removeLast();
                if (toc_nav_depth < 0) {
                    break;
                }
                if (name == QLatin1String("ol") && ol_depth > 0) {
                    --ol_depth;
                }
                // The toc nav's own end element closes the collected list.
                if (stack.size() < toc_nav_depth) {
                    toc_nav_depth = -1;
                    ol_depth = 0;
                    finished = true;
                }
                break;
            }
            default:
                break;
        }
    }

    if (!finished && reader.hasError()) {
        set_error(error,
                  "cannot parse the navigation document: " + reader.errorString().toStdString());
        return false;
    }
    return true;
}

bool parse_ncx(const std::string& xml, const std::string& package_dir,
               std::vector<EpubTocEntry>* toc, std::string* error) {
    struct NavPoint {
        std::size_t entry = 0;
        bool in_label = false;
        bool has_title = false;
        bool has_href = false;
    };

    QXmlStreamReader reader(QByteArray::fromStdString(xml));
    QStringList stack;
    std::vector<NavPoint> points;
    bool in_nav_map = false;
    bool finished = false;

    while (!reader.atEnd() && !finished) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: {
                const QString name = reader.name().toString();
                if (!in_nav_map) {
                    if (name == QLatin1String("navMap")) {
                        in_nav_map = true;
                    }
                    stack.push_back(name);
                    break;
                }
                if (name == QLatin1String("navPoint")) {
                    const int level = static_cast<int>(points.size());
                    points.push_back(NavPoint{toc->size()});
                    toc->push_back(EpubTocEntry{level, std::string(), std::string()});
                    stack.push_back(name);
                    break;
                }
                if (!points.empty() && name == QLatin1String("navLabel")) {
                    points.back().in_label = true;
                    stack.push_back(name);
                    break;
                }
                if (!points.empty() && points.back().in_label && !points.back().has_title &&
                    name == QLatin1String("text")) {
                    const QString title =
                        reader.readElementText(QXmlStreamReader::IncludeChildElements);
                    (*toc)[points.back().entry].title = title.toStdString();
                    points.back().has_title = true;
                    break;
                }
                if (!points.empty() && !points.back().has_href &&
                    name == QLatin1String("content")) {
                    const std::optional<std::string> href = resolve_href(
                        attribute_value(reader.attributes(), QStringLiteral("src")).toStdString(),
                        package_dir);
                    if (href.has_value()) {
                        (*toc)[points.back().entry].href = *href;
                        points.back().has_href = true;
                    }
                    stack.push_back(name);
                    break;
                }
                stack.push_back(name);
                break;
            }
            case QXmlStreamReader::EndElement: {
                const QString name = stack.isEmpty() ? QString() : stack.last();
                if (!stack.isEmpty()) {
                    stack.removeLast();
                }
                if (name == QLatin1String("navLabel") && !points.empty()) {
                    points.back().in_label = false;
                } else if (name == QLatin1String("navPoint") && !points.empty()) {
                    const NavPoint point = points.back();
                    points.pop_back();
                    // A navPoint without a usable content target is dropped
                    // together with everything nested after it.
                    if (!point.has_href) {
                        toc->resize(point.entry);
                    }
                } else if (name == QLatin1String("navMap") && in_nav_map) {
                    finished = true;
                }
                break;
            }
            default:
                break;
        }
    }

    if (!finished && reader.hasError()) {
        set_error(error, "cannot parse the NCX document: " + reader.errorString().toStdString());
        return false;
    }
    return true;
}

}  // namespace niratan::novels::detail

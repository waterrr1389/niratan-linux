// SPDX-License-Identifier: GPL-3.0-or-later
//
// Module-private helpers shared by the EPUB package parser and its table of
// contents parser. Nothing outside src/services/novels includes this header.

#pragma once

#include <QString>
#include <QXmlStreamAttributes>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "services/novels/epub_book.h"

namespace niratan::novels::detail {

void set_error(std::string* error, std::string message);

QString attribute_value(const QXmlStreamAttributes& attributes, const QString& name);

// Turns an href from the package into a container path: percent-decoded once,
// resolved against the package directory and normalized. A path that would
// leave the container is unusable, as is an href that names no document.
std::optional<std::string> resolve_href(std::string_view href, std::string_view package_dir);

// The EPUB 3 navigation document's and the EPUB 2 NCX's table of contents.
bool parse_nav(const std::string& xml, const std::string& package_dir,
               std::vector<EpubTocEntry>* toc, std::string* error);
bool parse_ncx(const std::string& xml, const std::string& package_dir,
               std::vector<EpubTocEntry>* toc, std::string* error);

}  // namespace niratan::novels::detail

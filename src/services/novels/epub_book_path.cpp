// SPDX-License-Identifier: GPL-3.0-or-later

#include "epub_book_internal.h"

#include <QUrl>

namespace niratan::novels::detail {

// Turns an href from the package into a container path: percent-decoded once,
// resolved against the package directory and normalized. A path that would
// leave the container is unusable, as is an href that names no document.
std::optional<std::string> resolve_href(std::string_view href, std::string_view package_dir) {
    const std::size_t fragment_at = href.find('#');
    const std::string_view path = href.substr(0, fragment_at);
    const std::string_view fragment =
        fragment_at == std::string_view::npos ? std::string_view{} : href.substr(fragment_at);
    if (path.empty()) {
        return std::nullopt;
    }

    const QString decoded =
        QUrl::fromPercentEncoding(QByteArray(path.data(), static_cast<qsizetype>(path.size())));
    std::string relative = decoded.toStdString();

    std::size_t start = 0;
    while (start < relative.size()) {
        if (relative[start] == '/') {
            ++start;
        } else if (relative.compare(start, 2, "./") == 0) {
            start += 2;
        } else {
            break;
        }
    }
    relative.erase(0, start);
    if (relative.empty()) {
        return std::nullopt;
    }

    std::filesystem::path combined(relative);
    if (!package_dir.empty()) {
        combined = std::filesystem::path(std::string(package_dir)) / combined;
    }
    combined = combined.lexically_normal();
    if (combined.empty()) {
        return std::nullopt;
    }
    if (*combined.begin() == "..") {
        return std::nullopt;
    }

    std::string result = combined.generic_string();
    result.append(fragment);
    return result;
}

}  // namespace niratan::novels::detail

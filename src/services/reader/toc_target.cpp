// SPDX-License-Identifier: GPL-3.0-or-later

#include "toc_target.h"

#include <QByteArray>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QUrl>

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "services/reader/character_normalizer.h"

namespace niratan::reader {

std::string percent_decoded(std::string_view text) {
    const QByteArray encoded(text.data(), static_cast<qsizetype>(text.size()));
    return QUrl::fromPercentEncoding(encoded).toUtf8().toStdString();
}

namespace {

QString from_utf8(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::string to_utf8(const QString& text) { return text.toUtf8().toStdString(); }

// The value of the tag's id attribute, else xml:id, following
// ReaderChapterIndex.attributeValues: attribute names are lowercased and the
// first spelling of a name wins. Character references are still encoded.
std::optional<QString> tag_identifier(const QString& tag) {
    static const QRegularExpression attribute_regex(QStringLiteral(
        R"regex(\s([A-Za-z_:][A-Za-z0-9_.:-]*)\s*=\s*(?:"([^"]*)"|'([^']*)'))regex"));

    QHash<QString, QString> attributes;
    QRegularExpressionMatchIterator matches = attribute_regex.globalMatch(tag);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const QString name = match.captured(1).toLower();
        if (attributes.contains(name)) {
            continue;
        }
        if (match.capturedStart(2) >= 0) {
            attributes.insert(name, match.captured(2));
        } else if (match.capturedStart(3) >= 0) {
            attributes.insert(name, match.captured(3));
        }
    }

    const auto id = attributes.constFind(QStringLiteral("id"));
    if (id != attributes.cend()) {
        return *id;
    }
    const auto xml_id = attributes.constFind(QStringLiteral("xml:id"));
    if (xml_id != attributes.cend()) {
        return *xml_id;
    }
    return std::nullopt;
}

}  // namespace

TocTarget split_target(const std::string& href) {
    const std::size_t fragment_at = href.find('#');
    const std::string raw_path =
        fragment_at == std::string::npos ? href : href.substr(0, fragment_at);

    // Strip query string: normalizedChapterPath splits on "?" before
    // removingPercentEncoding.
    const std::size_t query_at = raw_path.find('?');
    const std::string path_no_query =
        query_at == std::string::npos ? raw_path : raw_path.substr(0, query_at);

    // Remove leading "./" prefixes, then percent-decode.
    std::string_view trimmed = path_no_query;
    while (trimmed.substr(0, 2) == "./") {
        trimmed.remove_prefix(2);
    }
    const std::string path = percent_decoded(trimmed);

    if (fragment_at == std::string::npos) {
        return TocTarget{path, std::nullopt};
    }
    const std::string fragment = href.substr(fragment_at + 1);
    // Swift's target(from:) drops an empty fragment, so "#" alone is no target.
    return TocTarget{path, fragment.empty() ? std::nullopt
                                            : std::optional<std::string>(fragment)};
}

std::map<std::string, int> fragment_offsets(std::string_view markup,
                                            const std::set<std::string>& fragments) {
    std::map<std::string, int> offsets;
    if (fragments.empty()) {
        return offsets;
    }

    // A fragment is reachable by its raw and its percent-decoded spelling.
    // Character references are decoded on both sides, as the tag's attribute is.
    std::map<std::string, std::vector<std::string>> pending;
    for (const std::string& fragment : fragments) {
        offsets.emplace(fragment, 0);
        const std::string decoded = decoded_character_references(fragment);
        const std::string decoded_percent = decoded_character_references(percent_decoded(fragment));
        pending[decoded].push_back(fragment);
        if (decoded_percent != decoded) {
            pending[decoded_percent].push_back(fragment);
        }
    }

    const QString text = from_utf8(markup);

    static const QRegularExpression body_regex(QStringLiteral(R"(<body\b[^>]*>)"),
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression closing_body_regex(QStringLiteral(R"(</body\s*>)"),
                                                       QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch body_match = body_regex.match(text);
    if (!body_match.hasMatch()) {
        return offsets;
    }
    const qsizetype content_start = body_match.capturedEnd();
    qsizetype content_end = text.size();
    const QRegularExpressionMatch closing_match = closing_body_regex.match(text, content_start);
    if (closing_match.hasMatch()) {
        content_end = closing_match.capturedStart();
    }
    const QString content = text.mid(content_start, content_end - content_start);

    static const QRegularExpression comment_regex(QStringLiteral(R"((?s)<!--.*?-->)"));
    static const QRegularExpression script_style_regex(
        QStringLiteral(R"((?s)<(script|style)\b[^>]*>.*?</\1\s*>)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cdata_regex(QStringLiteral(R"((?s)<!\[CDATA\[.*?\]\]>)"));
    static const QRegularExpression opening_tag_regex(
        QStringLiteral(R"(<\s*[A-Za-z][A-Za-z0-9:_-]*\b[^>]*>)"),
        QRegularExpression::CaseInsensitiveOption);

    std::vector<std::pair<qsizetype, qsizetype>> ignored;
    for (const QRegularExpression& regex : {comment_regex, script_style_regex, cdata_regex}) {
        QRegularExpressionMatchIterator matches = regex.globalMatch(content);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            ignored.emplace_back(match.capturedStart(), match.capturedEnd());
        }
    }
    std::sort(ignored.begin(), ignored.end(),
              [](const std::pair<qsizetype, qsizetype>& left,
                 const std::pair<qsizetype, qsizetype>& right) { return left.first < right.first; });

    std::size_t ignored_index = 0;
    QRegularExpressionMatchIterator tags = opening_tag_regex.globalMatch(content);
    while (tags.hasNext()) {
        const QRegularExpressionMatch tag = tags.next();
        const qsizetype tag_start = tag.capturedStart();
        while (ignored_index < ignored.size() && ignored[ignored_index].second <= tag_start) {
            ++ignored_index;
        }
        if (ignored_index < ignored.size() && ignored[ignored_index].first <= tag_start) {
            continue;
        }

        const std::optional<QString> raw_identifier = tag_identifier(tag.captured(0));
        if (!raw_identifier.has_value()) {
            continue;
        }
        const auto match = pending.find(decoded_character_references(to_utf8(*raw_identifier)));
        if (match == pending.end()) {
            continue;
        }
        const int offset = readable_character_count(to_utf8(content.left(tag_start)));
        for (const std::string& fragment : match->second) {
            offsets[fragment] = offset;
        }
        pending.erase(match);
        if (pending.empty()) {
            break;
        }
    }

    return offsets;
}

}  // namespace niratan::reader

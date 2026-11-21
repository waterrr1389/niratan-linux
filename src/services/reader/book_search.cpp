// SPDX-License-Identifier: GPL-3.0-or-later

#include "book_search.h"

#include <QRegularExpression>

#include <algorithm>

#include "services/reader/book_session.h"

namespace niratan::reader {
namespace {

QString visible_text(QString html) {
    constexpr auto flags = QRegularExpression::CaseInsensitiveOption |
                           QRegularExpression::DotMatchesEverythingOption;
    static const QRegularExpression body(QStringLiteral("<body[^>]*>.*?</body>"), flags);
    static const QRegularExpression ruby(QStringLiteral("<rt[^>]*>.*?</rt>"), flags);
    static const QRegularExpression scripts(QStringLiteral("<(script|style)[^>]*>.*?</\\1>"), flags);
    static const QRegularExpression tags(QStringLiteral("<[^>]+>"));
    if (auto match = body.match(html); match.hasMatch()) {
        html = match.captured();
    }
    return html.remove(ruby).remove(scripts).remove(tags)
        .replace(QLatin1StringView("&nbsp;"), QLatin1StringView(" "))
        .replace(QLatin1StringView("&amp;"), QLatin1StringView("&"))
        .replace(QLatin1StringView("&lt;"), QLatin1StringView("<"))
        .replace(QLatin1StringView("&gt;"), QLatin1StringView(">"))
        .replace(QLatin1StringView("&quot;"), QLatin1StringView("\""))
        .replace(QLatin1StringView("&apos;"), QLatin1StringView("'"));
}

bool matchable(char32_t ch) {
    constexpr std::pair<char32_t, char32_t> ranges[]{
        {0x30, 0x39}, {0x41, 0x5a}, {0x61, 0x7a}, {0x25cb, 0x25cb}, {0x3005, 0x3007},
        {0x303b, 0x303b}, {0x3041, 0x3096}, {0x309d, 0x309e}, {0x30a1, 0x30fa},
        {0x30fc, 0x30fc}, {0xff10, 0xff19}, {0xff21, 0xff3a}, {0xff41, 0xff5a},
        {0xff66, 0xff9d}, {0x2e80, 0x2fdf}, {0x3400, 0x4dbf}, {0x4e00, 0x9fff},
        {0x20000, 0x2a6df}, {0x2a700, 0x2b73f}, {0x2b740, 0x2b81f}, {0x2b820, 0x2ceaf},
        {0x2ceb0, 0x2ebef}, {0x30000, 0x3134f}, {0x31350, 0x323af},
    };
    return std::ranges::any_of(ranges, [ch](auto r) { return r.first <= ch && ch <= r.second; });
}

QString query_text(const QString& query) {
    QString result;
    for (char32_t ch : visible_text(query).toUcs4()) {
        if (matchable(ch)) {
            result.append(QString::fromUcs4(&ch, 1));
        }
    }
    return result;
}

}  // namespace

bool has_searchable_text(const QString& query) { return !query_text(query).isEmpty(); }

std::vector<SearchHit> search_book(const std::vector<SearchChapter>& chapters, const QString& query,
                                   int limit) {
    const QString needle = query_text(query);
    if (limit <= 0 || needle.isEmpty()) {
        return {};
    }
    std::vector<SearchHit> hits;
    QString label;
    for (const SearchChapter& chapter : chapters) {
        if (!chapter.label.isEmpty()) {
            label = chapter.label;
        }
        QString searchable;
        std::vector<qsizetype> search_offsets;
        std::vector<qsizetype> display_offsets;
        QString display;
        bool pending_space = false;
        for (char32_t ch : visible_text(chapter.markup).toUcs4()) {
            if (QChar::isSpace(ch)) {
                pending_space = !display.isEmpty();
                continue;
            }
            if (pending_space) {
                display_offsets.push_back(display.size());
                display.append(QLatin1Char(' '));
                pending_space = false;
            }
            const QString scalar = QString::fromUcs4(&ch, 1);
            if (matchable(ch)) {
                for (qsizetype unit = 0; unit < scalar.size(); ++unit) {
                    search_offsets.push_back(static_cast<qsizetype>(display_offsets.size()));
                }
                searchable.append(scalar);
            }
            display_offsets.push_back(display.size());
            display.append(scalar);
        }
        const auto end = static_cast<qsizetype>(display_offsets.size());
        display_offsets.push_back(display.size());
        qsizetype from = 0;
        qsizetype preceding_scalars = 0;
        while (static_cast<int>(hits.size()) < limit) {
            const qsizetype start = searchable.indexOf(needle, from, Qt::CaseInsensitive);
            if (start < 0) {
                break;
            }
            preceding_scalars += QStringView(searchable).mid(from, start - from).toUcs4().size();
            const qsizetype first = search_offsets[static_cast<std::size_t>(start)];
            const qsizetype last = search_offsets[static_cast<std::size_t>(start + needle.size() - 1)] + 1;
            const auto slice_start = std::max<qsizetype>(0, first - 24);
            const auto slice_end = std::min(end, last + 48);
            const QString prefix = slice_start > 0 ? QStringLiteral("...") : QString();
            const QString suffix = slice_end < end ? QStringLiteral("...") : QString();
            const auto left = display_offsets[static_cast<std::size_t>(slice_start)];
            const auto match_start = prefix.size() + display_offsets[static_cast<std::size_t>(first)] - left;
            const auto match_end = prefix.size() + display_offsets[static_cast<std::size_t>(last)] - left;
            hits.push_back({chapter.spine_index, chapter.character_start + static_cast<int>(preceding_scalars),
                            label, prefix + display.mid(left, display_offsets[static_cast<std::size_t>(slice_end)] - left) + suffix,
                            static_cast<int>(match_start), static_cast<int>(match_end)});
            from = start + needle.size();
            preceding_scalars += needle.toUcs4().size();
        }
        if (static_cast<int>(hits.size()) == limit) {
            break;
        }
    }
    return hits;
}

std::optional<std::vector<SearchHit>> search_epub(const std::filesystem::path& path,
                                                const QString& query) {
    // Each worker opens its own archive; the visible reader owns a different
    // libzip handle and can continue fetching resources during the query.
    const auto session = BookSession::open(path);
    if (!session) {
        return std::nullopt;
    }
    std::vector<QString> labels(static_cast<std::size_t>(session->spine_count()));
    QString group;
    for (const auto& entry : session->contents()) {
        if (entry.level == 0 || group.isEmpty()) {
            group = QString::fromStdString(entry.title);
        }
        auto& label = labels.at(static_cast<std::size_t>(entry.spine_index));
        if (label.isEmpty() && !session->book().toc().empty()) {
            label = group;
        }
    }
    std::vector<SearchChapter> chapters;
    chapters.reserve(labels.size());
    for (int i = 0; i < session->spine_count(); ++i) {
        const auto markup = session->book().read(session->spine_path(i));
        chapters.push_back({i, session->character_offset(i, 0),
                            labels.at(static_cast<std::size_t>(i)),
                            markup ? QString::fromStdString(*markup) : QString()});
    }
    return search_book(chapters, query);
}

}  // namespace niratan::reader

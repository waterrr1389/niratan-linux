// SPDX-License-Identifier: GPL-3.0-or-later
//
// ReaderSearchEngine.swift (search, ReaderSearchIndex and ReaderSearchTextFilter).

#include <QtTest>

#include "services/reader/book_search.h"

using namespace niratan::reader;

class BookSearchTest : public QObject {
    Q_OBJECT

   private slots:
    void spine_order_and_offsets() {
        const std::vector<SearchChapter> chapters{
            {0, 0, QStringLiteral("One"), QStringLiteral("<head>fox</head><body>A f-o-x Fox</body>")},
            {1, 7, {}, QStringLiteral("<body>FOX</body>")},
        };
        const auto hits = search_book(chapters, QStringLiteral("fOX"));
        QCOMPARE(hits.size(), 3U);
        QCOMPARE(hits.at(0), (SearchHit{0, 1, QStringLiteral("One"), QStringLiteral("A f-o-x Fox"), 2, 7}));
        QCOMPARE(hits.at(1).character, 4);
        QCOMPARE(hits.at(2), (SearchHit{1, 7, QStringLiteral("One"), QStringLiteral("FOX"), 0, 3}));
        QCOMPARE(search_book(chapters, QStringLiteral("Foxfox"), 1).size(), 1U);
        QCOMPARE(search_book(chapters, QStringLiteral("Foxfox")).size(), 1U);
        QVERIFY(search_book(chapters, QStringLiteral("Foxfoxf")).empty());
    }

    void filter_and_limit() {
        const std::vector<SearchChapter> chapters{
            {0, 0, {}, QStringLiteral("<body><script>fox</script><ruby>狐<rt>fox</rt></ruby>　A&nbsp;A A</body>")},
        };
        QVERIFY(!has_searchable_text(QStringLiteral("!?　")));
        QVERIFY(has_searchable_text(QStringLiteral("狐")));
        QVERIFY(search_book(chapters, QStringLiteral("fox")).empty());
        const auto hits = search_book(chapters, QStringLiteral("a"), 2);
        QCOMPARE(hits.size(), 2U);
        QCOMPARE(hits.at(0).character, 1);
        QCOMPARE(hits.at(1).character, 2);
        QCOMPARE(hits.at(0).snippet, QStringLiteral("狐 A A A"));
    }

    // A named chapter keeps its own label even when the name is empty; only
    // a spine without any contents entry inherits the label before it.
    void empty_label_is_not_inherited() {
        const std::vector<SearchChapter> chapters{
            {0, 0, QStringLiteral("One"), QStringLiteral("<body>fox</body>")},
            {1, 3, QString(), QStringLiteral("<body>fox</body>")},
            {2, 6, std::nullopt, QStringLiteral("<body>fox</body>")},
        };
        const auto hits = search_book(chapters, QStringLiteral("fox"));
        QCOMPARE(hits.size(), 3U);
        QCOMPARE(hits.at(0).label, QStringLiteral("One"));
        QCOMPARE(hits.at(1).label, QString());
        QCOMPARE(hits.at(2).label, QString());
    }

    // The deepest contents entry naming a spine labels it, the first among
    // equals; a spine nothing names has no label of its own.
    void labels_from_contents() {
        const std::vector<ContentsEntry> contents{
            {0, 0, "Part One", ""},
            {0, 1, "Chapter One", ""},
            {0, 1, "Chapter Two", "two"},
            {2, 0, "Part Two", ""},
            {3, 0, "", ""},
            {-1, 2, "Before", ""},
            {5, 2, "After", ""},
        };
        const auto labels = search_labels(contents, 5);
        QCOMPARE(labels.size(), 5U);
        QCOMPARE(labels.at(0), std::optional<QString>(QStringLiteral("Chapter One")));
        QCOMPARE(labels.at(1), std::optional<QString>());
        QCOMPARE(labels.at(2), std::optional<QString>(QStringLiteral("Part Two")));
        QCOMPARE(labels.at(3), std::optional<QString>(QString()));
        QCOMPARE(labels.at(4), std::optional<QString>());
        QCOMPARE(search_labels({}, 2),
                 (std::vector<std::optional<QString>>{std::nullopt, std::nullopt}));
    }

    // Consecutive hits under one label form one group, even across spines,
    // so a chapter split into several files reads as one chapter.
    void group_counts_follow_label_runs() {
        const auto hit = [](int spine, const char* label) {
            return SearchHit{spine, 0, QString::fromUtf8(label), {}, 0, 0};
        };
        const std::vector<SearchHit> hits{hit(0, "A"), hit(1, "A"), hit(1, "A"), hit(2, "B"),
                                          hit(3, ""),  hit(4, ""),  hit(5, "A")};
        QCOMPARE(search_group_counts(hits), (std::vector<int>{3, 0, 0, 1, 2, 0, 1}));
        QVERIFY(search_group_counts({}).empty());
    }
};

QTEST_APPLESS_MAIN(BookSearchTest)
#include "tst_book_search.moc"

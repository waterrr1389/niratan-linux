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
};

QTEST_APPLESS_MAIN(BookSearchTest)
#include "tst_book_search.moc"

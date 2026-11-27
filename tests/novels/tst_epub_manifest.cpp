// SPDX-License-Identifier: GPL-3.0-or-later
// EPUBKit document.manifest.items and Linux's existing first-id resolution.

#include "fakes/ttu_converter_fixture.h"
#include "services/novels/epub_book.h"

#include <QTemporaryDir>
#include <QTest>

class EpubManifestTest : public QObject {
    Q_OBJECT

   private slots:
    void exposes_package_relative_and_container_paths_in_document_order() {
        using namespace niratan::novels;
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto path = root.filePath("fixture.epub").toStdString();
        QVERIFY(ttu_converter_fixture::make_epub(path));
        const auto book = EpubBook::open(path);
        QVERIFY(book);
        const std::vector<EpubManifestItem> expected{
            {"css", "OEBPS/style.css", "style.css", "text/css", ""},
            {"pic", "OEBPS/Images/pic.png", "Images/pic.png", "image/png", ""},
            {"one", "OEBPS/Text/one.xhtml", "Text/one.xhtml", "application/xhtml+xml", ""},
            {"two", "OEBPS/Text/two.xhtml", "Text/two.xhtml", "application/xhtml+xml", ""},
            {"nav", "OEBPS/nav.xhtml", "nav.xhtml", "application/xhtml+xml", "nav"},
            {"three", "OEBPS/Text/three.xhtml", "Text/three.xhtml", "application/xhtml+xml", ""},
            {"extra", "OEBPS/other.css", "other.css", "text/css", ""},
            {"jpeg", "OEBPS/Images/gone.jpg", "Images/gone.jpg", "image/jpeg", ""},
            {"webp", "OEBPS/Images/no.webp", "Images/no.webp", "image/webp", ""},
        };
        QCOMPARE(book->manifest(), expected);
        QCOMPARE(book->read(book->manifest().at(1).href), std::optional{std::string("PNG")});
    }
};

QTEST_APPLESS_MAIN(EpubManifestTest)
#include "tst_epub_manifest.moc"

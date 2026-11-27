// SPDX-License-Identifier: GPL-3.0-or-later
//
// Runs the reader's unchanged selection.js in Chromium, where the clicked
// text lives. C++ does not reconstruct its sentence.

#include "services/reader/reader_shell.h"

#include <QEventLoop>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSignalSpy>
#include <QTimer>
#include <QWebEnginePage>
#include <QTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include "../viewmodels/data_root.h"

namespace {

QVariant evaluate(QWebEnginePage& page, const QString& script) {
    QVariant result;
    QEventLoop loop;
    page.runJavaScript(script, [guard = QPointer(&loop), &result](const QVariant& value) {
        if (guard) {
            result = value;
            guard->quit();
        }
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    return result;
}

}  // namespace

class ReaderSelectionTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void get_sentence_data();
    void get_sentence();
    void lookup_and_sentence_are_different_spans_data();
    void lookup_and_sentence_are_different_spans();

   private:
    QWebEnginePage page_;
};

void ReaderSelectionTest::initTestCase() {
    const auto scripts = niratan::reader::load_reader_scripts();
    QVERIFY(!scripts.selection.empty());
    QSignalSpy loaded(&page_, &QWebEnginePage::loadFinished);
    page_.setHtml("<!doctype html><html><body></body></html>");
    QVERIFY(loaded.wait(15000));
    QVERIFY(loaded.first().value(0).toBool());
    const auto loaded_selection = evaluate(
        page_, QString::fromStdString(scripts.selection) + "\ntypeof hoshiSelection.getSentence");
    QCOMPARE(loaded_selection.toString(), QString("function"));
}

// B2: selection.js findParagraph, createWalker and getSentence.
// B3, B4: selection.js sentenceDelimiters, trailingSentenceChars and getSentence.
// B5: selection.js getSentence and brackets.
void ReaderSelectionTest::get_sentence_data() {
    QTest::addColumn<QString>("html");
    QTest::addColumn<int>("offset");
    QTest::addColumn<QString>("language");
    QTest::addColumn<QString>("expected");

    QTest::newRow("B2-inline-and-furigana")
        << QString("<p>隣の段落</p><p>この<ruby><span id='hit'>漢</span><rp>（</rp>"
                   "<rt>かん。</rt><rp>）</rp></ruby><em>字</em>です。</p><p>別の段落</p>")
        << 0 << QString("ja") << QString("この漢字です。");
    QTest::newRow("B2-dictionary-containers")
        << QString("<div>Outer<ul class='kanji-entry'><li>List"
                   "<div class='glossary-content'>Glossary<span class='expr-tag'>Expression"
                   "<span class='dictionary-query-source' id='hit'>Query words</span>"
                   "end</span>end</div>end</li></ul>end</div>")
        << 1 << QString("en") << QString("Query words");
    QTest::newRow("B2-kanji-entry-list-item-is-the-closest-container")
        << QString("<ul class='kanji-entry'><li>Previous item</li>"
                   "<li>Read <span id='hit'>this</span> entry</li><li>Next item</li></ul>")
        << 1 << QString("en") << QString("Read this entry");
    QTest::newRow("B2-glossary-content-is-the-closest-container")
        << QString("<div>Previous gloss<div class='glossary-content'>Read <em id='hit'>this</em>"
                   " gloss</div>Next gloss</div>")
        << 1 << QString("en") << QString("Read this gloss");
    QTest::newRow("B2-body-fallback")
        << QString("<div>Before </div><div id='hit'>these</div><div> words</div>") << 1
        << QString("en") << QString("Before these words");
    QTest::newRow("B3-japanese-delimiter-and-trailing-characters")
        << QString("<p id='hit'>前。「文！？…‥」次。</p>") << 3 << QString("ja")
        << QString("「文！？…‥」");
    QTest::newRow("B3-trailing-characters-stay-in-the-delimiter-node")
        << QString("<p><span id='hit'>文。</span><em>…</em>次。</p>") << 0 << QString("ja")
        << QString("文。");
    QTest::newRow("B4-english-ascii-delimiters")
        << QString("<p id='hit'>First. Next words? Last!</p>") << 9 << QString("en")
        << QString("Next words?");
    QTest::newRow("B4-japanese-delimiters-in-english-mode")
        << QString("<p id='hit'>First！ Next words。 Last!</p>") << 9 << QString("en")
        << QString("Next words。");
    QTest::newRow("B4-comma-ellipsis-dash-and-space-do-not-end-sentence")
        << QString("<p id='hit'>語、語…語─ 語!</p>") << 4 << QString("ja")
        << QString("語、語…語─ 語!");
    QTest::newRow("B4-cr-and-lf-are-boundaries")
        << QString("<p id='hit'>Before&#13;Current\nAfter</p>") << 9 << QString("en")
        << QString("Current");
    QTest::newRow("B5-trims-before-and-after-unmatched-brackets")
        << QString("<p id='hit'>　「&nbsp;本文。』　</p>") << 3 << QString("ja")
        << QString("本文。");
    QTest::newRow("B5-removes-a-delimiter-after-an-unmatched-close")
        << QString("<p id='hit'>Keep this). Not this.</p>") << 1 << QString("en")
        << QString("Keep this");
}

// B2–B5: selection.js getSentence, exercised directly without selection hit testing.
void ReaderSelectionTest::get_sentence() {
    QFETCH(QString, html);
    QFETCH(int, offset);
    QFETCH(QString, language);
    QFETCH(QString, expected);

    const auto args =
        QJsonDocument(QJsonArray{html, offset, language}).toJson(QJsonDocument::Compact);
    const auto script = QStringLiteral(R"JS(
        ((html, offset, language) => {
            document.body.innerHTML = html;
            hoshiSelection.language = language;
            return hoshiSelection.getSentence(document.getElementById('hit').firstChild, offset);
        })(...%1)
    )JS");
    const auto result = evaluate(page_, script.arg(QString::fromUtf8(args)));
    QVERIFY(result.isValid());
    QCOMPARE(result.toString(), expected);
}

// B1, B6, B14, B15: selection.js selectTextAtPoint, findEnglishWordStart,
// isScanBoundaryAt, isEnglishScanBoundaryAt, getSentence and miningContextForSelection.
void ReaderSelectionTest::lookup_and_sentence_are_different_spans_data() {
    QTest::addColumn<QString>("html");
    QTest::addColumn<int>("offset");
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("scan_length");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("sentence");
    QTest::addColumn<QString>("context_sentence");

    QTest::newRow("B6-ja-starts-at-click-and-stops-at-scan-length")
        << QString("<p id='hit'>前。今日は学校に行きます。</p>") << 5 << QString("ja") << 2
        << QString("学校") << QString("今日は学校に行きます。")
        << QString("今日は学校に行きます。");
    QTest::newRow("B6-en-starts-at-word-start-and-stops-at-boundary")
        << QString("<p id='hit'>Prior. We decide—today.</p>") << 13 << QString("en") << 16
        << QString("decide") << QString("We decide—today.") << QString("We decide—today.");
    QTest::newRow("B15-context-sheet-list-does-not-define-base-sentence")
        << QString("<p>Outer <span class='expr-tag' id='hit'>word</span> continues.</p>") << 1
        << QString("en") << 3 << QString("wor") << QString("word")
        << QString("Outer word continues.");
}

// B1, B6, B14, B15: selection.js selectTextAtPoint and miningContextForSelection.
void ReaderSelectionTest::lookup_and_sentence_are_different_spans() {
    QFETCH(QString, html);
    QFETCH(int, offset);
    QFETCH(QString, language);
    QFETCH(int, scan_length);
    QFETCH(QString, text);
    QFETCH(QString, sentence);
    QFETCH(QString, context_sentence);

    const auto args = QJsonDocument(QJsonArray{html, offset, language, scan_length})
                          .toJson(QJsonDocument::Compact);
    // The headless page has no view. Supply the hit and let the real script
    // choose the scan window, sentence, and context-sheet list.
    const auto script = QStringLiteral(R"JS(
        ((html, offset, language, scanLength) => {
            hoshiSelection.clearSelection();
            document.body.innerHTML = html;
            hoshiSelection.language = language;
            const node = document.getElementById('hit').firstChild;
            const getCharacterAtPoint = hoshiSelection.getCharacterAtPoint;
            hoshiSelection.getCharacterAtPoint = () => ({node, offset});
            try {
                let payload;
                const text = hoshiSelection.selectTextAtPoint(
                    0, 0, scanLength, true, selection => { payload = selection; }
                );
                return JSON.stringify({text, payload});
            } finally {
                hoshiSelection.getCharacterAtPoint = getCharacterAtPoint;
            }
        })(...%1)
    )JS");
    const auto result = evaluate(page_, script.arg(QString::fromUtf8(args)));
    const auto selected = QJsonDocument::fromJson(result.toString().toUtf8()).object();
    QCOMPARE(selected.value("text").toString(), text);
    const auto payload = selected.value("payload").toObject();
    QCOMPARE(payload.value("text").toString(), text);
    QCOMPARE(payload.value("sentence").toString(), sentence);
    QVERIFY(payload.value("normalizedOffset").isNull());
    QVERIFY(payload.value("rect").toObject().value("width").isDouble());
    const auto context = payload.value("miningContext").toObject();
    const int index = context.value("currentIndex").toInt(-1);
    const auto sentences = context.value("sentences").toArray();
    QVERIFY(index >= 0 && index < sentences.size());
    QCOMPARE(sentences.at(index).toObject().value("text").toString(), context_sentence);
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    niratan::test::TempDataRoot data_root;
    if (!data_root.isValid()) {
        return 1;
    }
    QGuiApplication app(argc, argv);
    ReaderSelectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_reader_selection.moc"

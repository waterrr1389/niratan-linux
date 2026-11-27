// SPDX-License-Identifier: GPL-3.0-or-later
//
// Runs the document services/reader/reader_shell generates in Chromium and
// inspects the rendered illustration. NativeReaderView.injectReader promotes
// a large non-gaiji img to block-img, and img.block-img sizes the image by
// its own ratio, so a publisher rule such as width/height 100% cannot squash
// it. Every svg also loses preserveAspectRatio="none".

#include "services/reader/reader_shell.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTimer>
#include <QUrl>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QTest>

#include "../viewmodels/data_root.h"

namespace {

QVariant evaluate(QWebEnginePage& page, const QString& script) {
    QVariant result;
    QEventLoop loop;
    page.runJavaScript(script, [&loop, &result](const QVariant& value) {
        result = value;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    return result;
}

bool wait_for(QWebEnginePage& page, const QString& expression, int timeout_ms) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout_ms) {
        if (evaluate(page, expression).toBool()) {
            return true;
        }
        QTest::qWait(50);
    }
    return false;
}

// An SVG image keeps its intrinsic size in Chromium, so no file is needed.
QString image_uri(int width, int height) {
    const QByteArray svg =
        QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\">"
                       "<rect width=\"%1\" height=\"%2\" fill=\"#888888\"/></svg>")
            .arg(width)
            .arg(height)
            .toUtf8();
    return QStringLiteral("data:image/svg+xml;base64,") + QString::fromLatin1(svg.toBase64());
}

std::string chapter_html() {
    const QString uri = image_uri(600, 300);
    return QStringLiteral(R"HTML(
<!doctype html><html><head><style>
img { width: 100%; height: 100%; }
</style></head><body>
<img id="illustration" src="%1">
<img id="gaiji" class="gaiji" src="%1">
<svg id="art" preserveAspectRatio="none" viewBox="0 0 600 300">
<image href="%1" width="600" height="300"/>
</svg>
</body></html>
)HTML")
        .arg(uri)
        .toStdString();
}

niratan::reader::ShellInput shell_input() {
    niratan::reader::ShellInput shell;
    shell.appearance.vertical_writing = false;
    shell.appearance.continuous = true;
    shell.page = niratan::reader::PageSize{1024, 768};
    shell.colors = niratan::reader::Colors{"#111111", "#FAFAFA", "#222222", "#EEEEEE"};
    shell.restore_token = QStringLiteral("token-images").toStdString();
    shell.initial_restore_expression =
        QStringLiteral("window.hoshiReader.restoreProgress(0)").toStdString();
    return shell;
}

QJsonObject image_state(QWebEnginePage& page) {
    return QJsonDocument::fromJson(evaluate(page, QStringLiteral(R"JS(
        (function() {
            var image = document.getElementById('illustration');
            var rect = image.getBoundingClientRect();
            return JSON.stringify({
                hasClass: image.classList.contains('block-img'),
                width: rect.width,
                height: rect.height,
                naturalWidth: image.naturalWidth,
                naturalHeight: image.naturalHeight,
                gaijiClass: document.getElementById('gaiji').classList.contains('block-img'),
                preserve: document.getElementById('art').getAttribute('preserveAspectRatio')
            });
        })()
    )JS"))
                                       .toString()
                                       .toUtf8())
        .object();
}

}  // namespace

class ReaderImagesTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void a_large_illustration_is_promoted_and_keeps_its_ratio();
    void a_gaiji_is_not_promoted();
    void preserve_aspect_ratio_none_is_removed_from_svg();

   private:
    QWebEngineView view_;
    QWebChannel channel_{view_.page()};
    QObject bridge_;
};

void ReaderImagesTest::initTestCase() {
    view_.resize(1024, 768);
    view_.show();
    channel_.registerObject(QStringLiteral("readerBridge"), &bridge_);
    view_.page()->setWebChannel(&channel_);
    const auto scripts = niratan::reader::load_reader_scripts();
    QVERIFY(!scripts.reader.empty());
    QSignalSpy loaded(view_.page(), &QWebEnginePage::loadFinished);
    view_.setHtml(QString::fromStdString(
                      niratan::reader::reader_document(chapter_html(), shell_input(), scripts)),
                  QUrl(QStringLiteral("https://reader.test/chapter.xhtml")));
    QVERIFY(loaded.wait(15000));
    QVERIFY(loaded.last().value(0).toBool());
    QVERIFY(wait_for(*view_.page(), QStringLiteral("document.readyState === 'complete'"), 5000));
    QVERIFY(wait_for(*view_.page(),
                     QStringLiteral("document.getElementById('illustration')"
                                    ".classList.contains('block-img')"),
                     5000));
}

void ReaderImagesTest::a_large_illustration_is_promoted_and_keeps_its_ratio() {
    const QJsonObject state = image_state(*view_.page());
    QVERIFY(state.value("hasClass").toBool());
    QCOMPARE(state.value("naturalWidth").toInt(), 600);
    QCOMPARE(state.value("naturalHeight").toInt(), 300);
    const double width = state.value("width").toDouble();
    const double height = state.value("height").toDouble();
    QVERIFY2(height > 0, qPrintable(QJsonDocument(state).toJson(QJsonDocument::Compact)));
    QVERIFY2(qAbs(width / height - 2.0) < 0.1,
             qPrintable(
                 QStringLiteral("rendered %1x%2\n%3")
                     .arg(width)
                     .arg(height)
                     .arg(QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact)))));
}

void ReaderImagesTest::a_gaiji_is_not_promoted() {
    QVERIFY(!image_state(*view_.page()).value("gaijiClass").toBool());
}

void ReaderImagesTest::preserve_aspect_ratio_none_is_removed_from_svg() {
    QVERIFY(image_state(*view_.page()).value("preserve").isNull());
}

int main(int argc, char* argv[]) {
    niratan::test::TempDataRoot data_root;
    if (!data_root.isValid()) {
        return 1;
    }
    QApplication app(argc, argv);
    ReaderImagesTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_reader_images.moc"

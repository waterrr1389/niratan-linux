// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bookshelf's multi-file import progress, following
// BookshelfViewModel.importBooks: while several files import in order the page
// shows "Importing k / n...", where k is the next 1-based file, and the label
// clears when the batch ends. A single file shows no progress. The duplicate
// first file is skipped silently, as processImport reports it.

#include "models/books/book.h"
#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"
#include "viewmodels/library/library_view_model.h"

#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"

#include "../flows/epub_fixture.h"
#include "../viewmodels/data_root.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTest>
#include <QUrl>
#include <QtQml/qqmlextensionplugin.h>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <memory>

Q_IMPORT_QML_PLUGIN(NiratanPlugin)
Q_IMPORT_QML_PLUGIN(NiratanStylePlugin)

namespace {

QString write_book(const QString& root, const QString& name, const char* title) {
    const QString path = root + QLatin1Char('/') + name + QStringLiteral(".epub");
    if (!niratan::test::write_epub(path.toStdString(), title, "一", "二")) {
        return {};
    }
    return path;
}

}  // namespace

class LibraryImportProgressTest : public QObject {
    Q_OBJECT

   private slots:
    void batch_progress_shows_the_next_file_and_clears();
};

// B9: three-file drop (first is an existing title), then a single file.
void LibraryImportProgressTest::batch_progress_shows_the_next_file_and_clears() {
    using niratan::books::BookStorage;
    using niratan::viewmodels::LibraryViewModel;
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    const QString same = write_book(root.path(), QStringLiteral("Same"), "Same");
    const QString second = write_book(root.path(), QStringLiteral("Second"), "Second");
    const QString third = write_book(root.path(), QStringLiteral("Third"), "Third");
    const QString single = write_book(root.path(), QStringLiteral("Single"), "Single");
    QVERIFY(!same.isEmpty() && !second.isEmpty() && !third.isEmpty() && !single.isEmpty());
    const BookStorage storage(niratan::storage::data_root());
    QVERIFY(storage.import_book(same.toStdString()).book.has_value());

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1100, 680);
    const QUrl url(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/LibraryPage.qml"));
    QQmlComponent component(&engine, url);
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.beginCreate(engine.rootContext()));
    QVERIFY(object);
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page);
    page->setParentItem(window.contentItem());
    page->setWidth(window.width());
    page->setHeight(window.height());
    component.completeCreate();
    auto* model = page->findChild<LibraryViewModel*>();
    QVERIFY(model);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QQuickItem* label = page->findChild<QQuickItem*>(QStringLiteral("libraryImportProgress"));
    QVERIFY(label);
    QStringList progresses;
    QString observed;
    bool observed_visible = false;
    QObject::connect(model, &LibraryViewModel::importProgressChanged, model, [&] {
        const QString progress = model->import_progress();
        progresses.append(progress);
        if (progress == QStringLiteral("Importing 2 / 3...")) {
            observed = progress;
            observed_visible = label->isVisible() && label->property("text").toString() == progress;
        }
    });

    QVERIFY(model->importDroppedEPUBs(
        {QUrl::fromLocalFile(same), QUrl::fromLocalFile(second), QUrl::fromLocalFile(third)}));
    QCOMPARE(observed, QStringLiteral("Importing 2 / 3..."));
    QVERIFY(observed_visible);
    QVERIFY(model->import_progress().isEmpty());
    QVERIFY(!label->isVisible());

    progresses.clear();
    QVERIFY(model->importDroppedEPUBs({QUrl::fromLocalFile(single)}));
    QVERIFY(progresses.isEmpty());
    QVERIFY(!label->isVisible());
}

int main(int argc, char* argv[]) {
    niratan::ui::register_popup_url_schemes();
    niratan::ui::register_reader_url_schemes();
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    LibraryImportProgressTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_library_import_progress.moc"

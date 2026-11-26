// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Settings/DictionaryView.swift (dictionaryReorderHandle,
// dictionaryReorderGesture, updateDictionaryDrag, reorderDictionary): each
// dictionary row leads with a reorder handle, and dragging it onto another
// row moves the dragged dictionary to that row's place while the pointer
// moves, through DictionaryReorder.destinationOffset.

#include "services/dictionary/dictionary_store.h"
#include "services/storage/app_paths.h"

#include "../dictionary/yomitan_fixture.h"
#include "data_root.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QPointer>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

#include <clocale>
#include <filesystem>
#include <memory>

Q_IMPORT_QML_PLUGIN(NiratanPlugin)
Q_IMPORT_QML_PLUGIN(NiratanStylePlugin)

class DictionaryReorderTest : public QObject {
    Q_OBJECT

   private slots:
    void dragging_a_handle_onto_a_row_moves_the_dictionary_there();
};

namespace {

bool import_dictionary(const QString& folder, const QString& title) {
    const auto zip = std::filesystem::path(folder.toStdString()) / (title.toStdString() + ".zip");
    niratan::test::write_stored_zip(
        zip,
        {{QStringLiteral("index.json"), niratan::test::minimal_index(title)},
         {QStringLiteral("term_bank_1.json"), niratan::test::minimal_term_bank()}});
    return niratan::dictionary::DictionaryStore(niratan::storage::dictionaries_root())
        .import_zip(zip)
        .ok;
}

QStringList titles(QObject* library) {
    QStringList result;
    for (const QVariant& row : library->property("rows").toList()) {
        result.append(row.toMap().value(QStringLiteral("title")).toString());
    }
    return result;
}

void collect_named(QQuickItem* item, const QString& name, QList<QQuickItem*>& found) {
    if (item->objectName() == name && item->isVisible()) {
        found.append(item);
    }
    for (QQuickItem* child : item->childItems()) {
        collect_named(child, name, found);
    }
}

}  // namespace

void DictionaryReorderTest::dragging_a_handle_onto_a_row_moves_the_dictionary_there() {
    niratan::test::TempDataRoot root;
    QVERIFY(root.isValid());
    for (const QString& title :
         {QStringLiteral("Alpha"), QStringLiteral("Beta"), QStringLiteral("Gamma")}) {
        QVERIFY(import_dictionary(root.path(), title));
    }

    QQuickWindow window;
    window.resize(960, 640);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlEngine engine;
    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/DictionarySettingsPage.qml")));
    std::unique_ptr<QObject> object(component.beginCreate(engine.rootContext()));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    page->setParentItem(window.contentItem());
    page->setSize(window.size());
    component.completeCreate();

    QObject* library = nullptr;
    for (QObject* child : page->findChildren<QObject*>()) {
        if (child->metaObject()->indexOfProperty("rows") >= 0 &&
            child->metaObject()->indexOfMethod("move(QString,QString)") >= 0) {
            library = child;
        }
    }
    QVERIFY2(library != nullptr, "the page has no dictionary library");
    QTRY_COMPARE(titles(library).size(), 3);
    const QStringList before = titles(library);

    // Every move rebuilds the rows, and a new row sits at the list's top until
    // the column lays it out. A drag starts only once the handles are the
    // current rows' and stand one below another, so a slow machine cannot
    // press where a row used to be.
    QList<QQuickItem*> handles;
    // Guarded copies taken while the rows are alive: a rebuilt row deletes
    // its handle, which a plain pointer would not notice.
    QList<QPointer<QQuickItem>> guarded;
    const auto settle_handles = [&handles, &guarded, page] {
        const QList<QPointer<QQuickItem>> old = guarded;
        const auto laid_out = [&] {
            for (const QPointer<QQuickItem>& item : old) {
                if (handles.contains(item.data())) {
                    return false;
                }
            }
            for (qsizetype i = 1; i < handles.size(); ++i) {
                if (handles.at(i)->mapToScene(QPointF(0, 0)).y() <=
                    handles.at(i - 1)->mapToScene(QPointF(0, 0)).y()) {
                    return false;
                }
            }
            return handles.size() == 3;
        };
        QTRY_VERIFY((handles.clear(),
                     collect_named(page, QStringLiteral("dictionaryReorderHandle"), handles),
                     laid_out()));
        guarded = QList<QPointer<QQuickItem>>(handles.cbegin(), handles.cend());
    };
    settle_handles();
    QVERIFY(!QTest::currentTestFailed());

    // Bring the list into view, as a user scrolls to it.
    auto* flick = qvariant_cast<QQuickItem*>(page->property("flickable"));
    QVERIFY(flick != nullptr);
    const qreal list_top = handles.first()->mapToItem(flick, QPointF(0, 0)).y();
    flick->setProperty("contentY",
                       flick->property("contentY").toReal() + list_top - window.height() / 4);
    QTRY_VERIFY(!flick->property("moving").toBool());

    const auto centre = [](QQuickItem* item) {
        return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
    };
    const QPoint from = centre(handles.first());
    const QPoint to = centre(handles.last());
    QVERIFY2(QRect(QPoint(0, 0), window.size()).contains(to), "the last row is not in view");

    // A press, then moves through each point, then a release at the last.
    const auto drag = [&window](QPoint press, const QList<QPoint>& path) {
        QTest::mousePress(&window, Qt::LeftButton, {}, press);
        QPoint last = press;
        for (const QPoint& point : path) {
            for (int step = 1; step <= 10; ++step) {
                QTest::mouseMove(&window, last + (point - last) * step / 10);
            }
            last = point;
        }
        QTest::mouseRelease(&window, Qt::LeftButton, {}, last);
        QTest::qWait(20);
    };

    drag(from, {to});
    const QStringList expected{before.at(1), before.at(2), before.at(0)};
    QTRY_COMPARE(titles(library), expected);
    settle_handles();
    QVERIFY(!QTest::currentTestFailed());

    // Back up: the last row onto the first restores the order.
    drag(centre(handles.last()), {centre(handles.first())});
    QTRY_COMPARE(titles(library), before);
    settle_handles();
    QVERIFY(!QTest::currentTestFailed());

    // A slip across a row edge, shorter than the drag distance, moves nothing.
    const QRectF first_handle =
        handles.first()->mapRectToScene(QRectF(QPointF(0, 0), handles.first()->size()));
    const QPoint edge(qRound(first_handle.center().x()), qRound(first_handle.bottom()) - 1);
    drag(edge, {edge + QPoint(0, 3)});
    QCOMPARE(titles(library), before);

    // Leaving the list sideways and going down outside it moves nothing.
    const qreal list_right = page->width() - 1;
    drag(from, {QPoint(qRound(list_right), from.y()), QPoint(qRound(list_right), to.y())});
    QCOMPARE(titles(library), before);

    // A press away from the handles still reaches the row's controls.
    QList<QQuickItem*> switches;
    collect_named(page, QStringLiteral("dictionaryEnabledSwitch"), switches);
    QCOMPARE(switches.size(), 3);
    const auto enabled = [library] {
        return library->property("rows")
            .toList()
            .constFirst()
            .toMap()
            .value(QStringLiteral("enabled"))
            .toBool();
    };
    const bool was_enabled = enabled();
    QTest::mouseClick(&window, Qt::LeftButton, {}, centre(switches.first()));
    QTRY_COMPARE(enabled(), !was_enabled);
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    DictionaryReorderTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_dictionary_reorder.moc"

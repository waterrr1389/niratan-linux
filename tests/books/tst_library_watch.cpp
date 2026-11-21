// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests the directory observation used by StatisticsViewModel.

#include "services/books/library_watch.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>

using niratan::books::LibraryWatch;

namespace {

std::filesystem::path root_of(const QTemporaryDir& dir) {
    return std::filesystem::path(dir.path().toStdString());
}

}  // namespace

class LibraryWatchTest : public QObject {
    Q_OBJECT

   private slots:
    void creatingBooksDirectory_emitsChanged() {
        QTemporaryDir dir;
        LibraryWatch watch(root_of(dir));
        QSignalSpy spy(&watch, &LibraryWatch::changed);

        QVERIFY(std::filesystem::create_directory(root_of(dir) / "Books"));
        QTRY_VERIFY(spy.count() > 0);

        spy.clear();
        QVERIFY(std::filesystem::create_directory(root_of(dir) / "Books" / "book"));
        QTRY_VERIFY(spy.count() > 0);
    }

    void creatingBookDirectory_emitsChanged() {
        QTemporaryDir dir;
        QVERIFY(std::filesystem::create_directory(root_of(dir) / "Books"));
        LibraryWatch watch(root_of(dir));
        QSignalSpy spy(&watch, &LibraryWatch::changed);

        QVERIFY(std::filesystem::create_directory(root_of(dir) / "Books" / "book"));
        QTRY_VERIFY(spy.count() > 0);
    }

    void emptyDataRoot_doesNotCrash() { LibraryWatch watch(std::filesystem::path{}); }
};

QTEST_GUILESS_MAIN(LibraryWatchTest)
#include "tst_library_watch.moc"

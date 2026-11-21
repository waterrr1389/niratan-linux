// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows VideoMiningHistoryStore.swift (init and loadItems), with the Linux
// empty-root contract and Core/UserConfig.swift's videoMiningHistoryLimit.

#include "services/player/mining_history_store.h"

#include <QTemporaryDir>
#include <QtTest>

#include <filesystem>

using niratan::player::MiningHistoryStore;

class TestMiningHistoryStore : public QObject {
    Q_OBJECT

   private slots:
    void empty_store_does_not_crash() {
        MiningHistoryStore store({});
        QVERIFY(store.empty());
        QVERIFY(!store.clear());
        QVERIFY(!store.add({}, 25));
        QVERIFY(!store.remove("missing"));
        QCOMPARE(store.load_snapshot(), MiningHistoryStore::Snapshot{});
    }

    void snapshot_returns_empty_for_missing_directory() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = std::filesystem::path(directory.path().toStdString()) / "missing";
        MiningHistoryStore store(path);
        QVERIFY(!store.empty());
        const auto snapshot = store.load_snapshot();
        QVERIFY(snapshot.items.empty());
        QCOMPARE(snapshot.limit, 25);
        QVERIFY(!std::filesystem::exists(path));
    }
};

QTEST_APPLESS_MAIN(TestMiningHistoryStore)
#include "tst_mining_history_store.moc"

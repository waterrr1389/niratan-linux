// SPDX-License-Identifier: GPL-3.0-or-later
//
// The library directory watch replaces the inline QFileSystemWatcher in
// StatisticsViewModel. It has no upstream Swift source because Swift uses
// in-memory observation for the corresponding library changes.
//
// The watch observes <data root>/Books, or the nearest existing directory
// when Books does not exist yet. An empty data root watches nothing.

#pragma once

#include <QFileSystemWatcher>
#include <QObject>

#include <filesystem>

namespace niratan::books {

class LibraryWatch : public QObject {
    Q_OBJECT

   public:
    static LibraryWatch at_data_root(QObject* parent = nullptr);

    explicit LibraryWatch(std::filesystem::path data_root, QObject* parent = nullptr);

   signals:
    void changed();

   private:
    void update_watch();

    std::filesystem::path data_root_;
    QFileSystemWatcher watcher_;
};

}  // namespace niratan::books

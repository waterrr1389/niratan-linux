// SPDX-License-Identifier: GPL-3.0-or-later

#include "library_watch.h"

#include <QFileInfo>
#include <QString>
#include <QStringList>

#include <utility>

#include "services/storage/app_roots.h"

namespace niratan::books {

LibraryWatch LibraryWatch::at_data_root(QObject* parent) {
    return LibraryWatch(storage::data_root(), parent);
}

LibraryWatch::LibraryWatch(std::filesystem::path data_root, QObject* parent)
    : QObject(parent), data_root_(std::move(data_root)) {
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this] {
        update_watch();
        emit changed();
    });
    update_watch();
}

void LibraryWatch::update_watch() {
    if (data_root_.empty()) {
        return;
    }

    QString watched;
    auto directory = data_root_ / "Books";
    while (!directory.empty()) {
        const auto name = QString::fromStdString(directory.string());
        if (QFileInfo(name).isDir()) {
            watched = name;
            break;
        }
        const auto parent = directory.parent_path();
        if (parent == directory) {
            break;
        }
        directory = parent;
    }

    const auto previous = watcher_.directories();
    if (!watched.isEmpty() && !previous.contains(watched)) {
        watcher_.addPath(watched);
    }
    for (const auto& path : previous) {
        if (path != watched) {
            watcher_.removePath(path);
        }
    }
}

}  // namespace niratan::books

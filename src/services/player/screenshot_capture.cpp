// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/player/screenshot_capture.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace niratan::player {

QString temporary_screenshot_path() {
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return QDir(directory).filePath(QStringLiteral("niratan-screenshot-%1.png")
                                        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

bool save_screenshot_capture(const QString& sourcePath, const QString& destinationPath) {
    if (destinationPath.isEmpty()) {
        return QFile::remove(sourcePath);
    }

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = source.readAll();
    source.close();

    QSaveFile destination(destinationPath);
    if (!destination.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (destination.write(bytes) != bytes.size() || !destination.commit()) {
        return false;
    }
    QFile::remove(sourcePath);
    return true;
}

}  // namespace niratan::player

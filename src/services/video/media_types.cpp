// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/video/media_types.h"

#include <QCollator>
#include <QFileInfo>
#include <QLocale>

namespace niratan::video {
namespace {

QStringList media_extensions() {
    return {QStringLiteral("mkv"),  QStringLiteral("webm"), QStringLiteral("avi"),
            QStringLiteral("m4v"),  QStringLiteral("mp4"),  QStringLiteral("mov"),
            QStringLiteral("qt"),   QStringLiteral("mpg"),  QStringLiteral("mpeg"),
            QStringLiteral("ts"),   QStringLiteral("m2ts"), QStringLiteral("mts"),
            QStringLiteral("3gp"),  QStringLiteral("ogv"),  QStringLiteral("wmv"),
            QStringLiteral("asf"),  QStringLiteral("flv"),  QStringLiteral("m4b"),
            QStringLiteral("m4a"),  QStringLiteral("mp3"),  QStringLiteral("flac"),
            QStringLiteral("opus"), QStringLiteral("ogg"),  QStringLiteral("oga"),
            QStringLiteral("weba"), QStringLiteral("wav"),  QStringLiteral("aac"),
            QStringLiteral("aiff"), QStringLiteral("aif"),  QStringLiteral("ape"),
            QStringLiteral("wv")};
}

}  // namespace

QStringList supported_extensions() {
    return media_extensions();
}

bool is_media_file(const QString& path) {
    const QString name = QFileInfo(path).fileName();
    const qsizetype dot = name.lastIndexOf(u'.');
    if (dot <= 0 || dot == name.size() - 1) {
        return false;
    }
    return media_extensions().contains(name.mid(dot + 1).toLower());
}

int localized_standard_compare(const QString& left, const QString& right) {
    // QCollator keeps the locale rules and is a little costly to build, so it
    // lives once per thread; the system locale is the one Foundation uses.
    thread_local const QCollator collator = [] {
        QCollator value{QLocale()};
        value.setCaseSensitivity(Qt::CaseInsensitive);
        value.setNumericMode(true);
        return value;
    }();
    return collator.compare(left, right);
}

}  // namespace niratan::video

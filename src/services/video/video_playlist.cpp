// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/video/video_playlist.h"

#include "services/video/media_types.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>

namespace niratan::video {
namespace {

// VideoPlaylist.swift supportedExtensions, in its order. This is narrower
// than the library scan set: the inspector lists exactly what Swift plays.
QStringList playlist_extensions() {
    return {QStringLiteral("3gp"),  QStringLiteral("aac"),  QStringLiteral("avi"),
            QStringLiteral("flac"), QStringLiteral("flv"),  QStringLiteral("m4a"),
            QStringLiteral("m4b"),  QStringLiteral("m4v"),  QStringLiteral("mkv"),
            QStringLiteral("mov"),  QStringLiteral("mp3"),  QStringLiteral("mp4"),
            QStringLiteral("mpeg"), QStringLiteral("mpg"),  QStringLiteral("ogg"),
            QStringLiteral("ogv"),  QStringLiteral("opus"), QStringLiteral("wav"),
            QStringLiteral("webm"), QStringLiteral("wmv")};
}

// URL.pathExtension: the text after the final dot, empty for dotless and
// leading-dot names.
QString path_extension(const QUrl& url) {
    const QString name = url.fileName();
    const qsizetype dot = name.lastIndexOf(u'.');
    if (dot <= 0 || dot == name.size() - 1) {
        return {};
    }
    return name.mid(dot + 1).toLower();
}

// URL.standardizedFileURL comparison input: a cleaned absolute local path,
// leaving a non-local URL as its text.
QString standardized_path(const QUrl& url) {
    if (!url.isLocalFile()) {
        return url.toString();
    }
    return QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath());
}

}  // namespace

QList<QUrl> playlist_directory_urls(const QString& directoryPath) {
    QList<QUrl> urls;
    const QDir directory(directoryPath);
    // QDir::Files skips hidden entries; the listing order stays unspecified,
    // as FileManager's does.
    const QFileInfoList entries =
        directory.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::NoSort);
    urls.reserve(entries.size());
    for (const QFileInfo& entry : entries) {
        urls.append(QUrl::fromLocalFile(entry.absoluteFilePath()));
    }
    return urls;
}

VideoPlaylist make_video_playlist(const QList<QUrl>& urls, const QUrl& currentUrl) {
    VideoPlaylist playlist;
    const QStringList extensions = playlist_extensions();
    for (const QUrl& url : urls) {
        if (extensions.contains(path_extension(url))) {
            playlist.items.append(url);
        }
    }
    std::stable_sort(playlist.items.begin(), playlist.items.end(),
                     [](const QUrl& left, const QUrl& right) {
                         return localized_standard_compare(left.fileName(), right.fileName()) < 0;
                     });
    if (!currentUrl.isEmpty()) {
        const QString current = standardized_path(currentUrl);
        for (qsizetype index = 0; index < playlist.items.size(); ++index) {
            if (standardized_path(playlist.items.at(index)) == current) {
                playlist.currentIndex = static_cast<int>(index);
                break;
            }
        }
    }
    return playlist;
}

}  // namespace niratan::video

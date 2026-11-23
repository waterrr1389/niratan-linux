// SPDX-License-Identifier: GPL-3.0-or-later
//
// The folder listing the video inspector shows as its episode list, following
// Features/Video/VideoPlaylist.swift (`supportedExtensions`, `init(urls:
// currentURL:)`) and VideoPlayerViewModel.swift's `playlistURLs(around:)`.
//
// `playlist_directory_urls` reads one directory and returns its candidate
// files: regular files only, hidden entries skipped (decision in the module
// task; Swift filters by extension alone). It returns every candidate, not
// only videos, so the caller can sort and filter with
// `make_video_playlist`.
//
// `make_video_playlist` is pure: it keeps files whose lower-cased extension
// is in `VideoPlaylist.supportedExtensions`, orders them by file name with
// Foundation's `localizedStandardCompare` (natural, case-insensitive), and
// reports the position of the current file, or -1 when it is not listed.
// Comparison of the current file uses the standardized absolute path, so two
// spellings of the same file match.
//
// The listing itself is not a stored value; nothing here touches the
// playback history. A caller that opens an episode reports the URL to the
// page, which owns the file change.
//
// The extension set is VideoPlaylist.swift's, not the wider library scan set
// in media_types.h: the inspector lists exactly what Swift would play next.

#pragma once

#include <QList>
#include <QString>
#include <QUrl>

namespace niratan::video {

struct VideoPlaylist {
    QList<QUrl> items;
    int currentIndex = -1;  // Swift's `currentIndex`: -1 when absent

    bool operator==(const VideoPlaylist&) const = default;
};

// Directory entries in unspecified order; the reader does not sort.
QList<QUrl> playlist_directory_urls(const QString& directoryPath);

VideoPlaylist make_video_playlist(const QList<QUrl>& urls, const QUrl& currentUrl);

}  // namespace niratan::video

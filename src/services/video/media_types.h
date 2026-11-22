// SPDX-License-Identifier: GPL-3.0-or-later
//
// The file types the video library scans, following
// Features/Video/VideoMediaTypes.swift `supportedExtensions` and
// `isMediaFile`, and the name ordering shared by the library's catalog and
// rows.
//
// An extension is compared lowercased and only when a stem precedes the dot:
// "Episode.MKV" matches, "notes.txt" and "episode.srt" do not, and a hidden
// file such as ".mp4" has no suffix and therefore no match either.
// `supported_extensions` keeps Swift's order.
//
// `localized_standard_compare` is the port of Foundation's
// `localizedStandardCompare` (decision in the module task): a QCollator with
// the system locale, case-insensitive, numeric mode on. It returns a negative
// value, zero or a positive value like `QString::compare`; callers keep
// Swift's tie breakers because the collator can report equality for names
// that differ in case or punctuation only.

#pragma once

#include <QString>
#include <QStringList>

namespace niratan::video {

QStringList supported_extensions();
bool is_media_file(const QString& path);
int localized_standard_compare(const QString& left, const QString& right);

}  // namespace niratan::video

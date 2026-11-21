// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows ImageHandler in Features/Popup/PopupWebView.swift: dictionary and
// path are query values decoded once, and both identify an indexed blob, not
// a filesystem path. A missing blob is distinct from a malformed request.
// The Linux popup also serves audio formats used by structured definitions.

#pragma once

#include <QByteArray>
#include <QUrl>

#include <filesystem>
#include <memory>
#include <vector>

#include "dictionary_store.h"

namespace niratan::dictionary {

class DictionaryQueryService;

enum class MediaStatus { Invalid, Missing, Found };

struct MediaResponse {
    MediaStatus status = MediaStatus::Invalid;
    QByteArray mime_type;
    QByteArray bytes;
};

class DictionaryMediaService {
   public:
    explicit DictionaryMediaService(std::filesystem::path root);
    ~DictionaryMediaService();

    MediaResponse get(const QUrl& url);

   private:
    DictionaryStore store_;
    std::vector<std::filesystem::path> directories_;
    std::unique_ptr<DictionaryQueryService> query_;
};

}  // namespace niratan::dictionary

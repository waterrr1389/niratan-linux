// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows Core/AnkiManager.swift (addNote, the word audio URLSession request).
// Mining accepts http, https and file sources. Failed fetches supply no bytes;
// HTTP response bodies are data even when the status indicates an error.

#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QUrl>

#include <functional>
#include <optional>

namespace niratan::audio {

class WebAudioDownload {
   public:
    using Callback = std::function<void(std::optional<QByteArray>)>;

    // Completes on the manager's thread, possibly inline for invalid URLs.
    // Destroying the service cancels outstanding requests.
    void get(const QUrl& url, Callback callback);

   private:
    QNetworkAccessManager network_;
};

}  // namespace niratan::audio

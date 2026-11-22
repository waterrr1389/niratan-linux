// SPDX-License-Identifier: GPL-3.0-or-later
//
// The shared implementation of MiningViewModel: the AnkiConnect transport
// wrapper, the backend shared by every host (saved words, active settings and
// media cache, like AnkiManager.shared), and the per-host State. Split out so
// the capture and sasayaki splits reach the same backend and document state
// without widening mining_view_model.h.
//
// Follows PopupWebView.swift (Coordinator.userContentController's shared
// coordinator and per-host context), Features/Popup/PopupView.swift
// (buildContent and mineEntry) and Core/AnkiManager.swift (the shared
// AnkiManager configuration and saved words).

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "services/anki/anki_config.h"
#include "services/anki/anki_connect.h"
#include "services/anki/duplicate_lookup.h"
#include "services/anki/mining.h"
#include "services/audio/web_audio_download.h"
#include "services/dictionary/dictionary_media.h"
#include "services/dictionary/term_dictionaries.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"
#include "viewmodels/anki/mining_view_model.h"
#include "viewmodels/anki/video_mining_capture.h"

namespace niratan::viewmodels {
namespace detail {

constexpr int toast_lifetime_ms = 2200;

// New requests can move to a different transport without losing callbacks
// already pending on the previous client.
class MiningConnection : public anki::AnkiConnectClient {
   public:
    using AnkiConnectClient::AnkiConnectClient;

    void send(std::string action, std::optional<QJsonObject> params, Callback callback) {
        ++pending_;
        request(std::move(action), std::move(params),
                [this, callback = std::move(callback)](anki::AnkiConnectResult result) {
                    --pending_;
                    if (retired_ && pending_ == 0) {
                        deleteLater();
                    }
                    if (callback) {
                        callback(std::move(result));
                    }
                });
    }

    void retire() {
        retired_ = true;
        if (pending_ == 0) {
            deleteLater();
        }
    }

   private:
    std::size_t pending_ = 0;
    bool retired_ = false;
};

// The two hosts share the saved words, active Anki settings and media cache,
// like AnkiManager.shared. Toasts and document context belong to each host.
class MiningBackend {
   public:
    explicit MiningBackend(profiles::ProfileRepository profile_repository)
        : repository(std::move(profile_repository)) {
        load_settings();
    }

    void load_settings() {
        // The settings page persists every edit, so read the current profile
        // without saving a stale outgoing snapshot.
        config = anki::AnkiConfigStore(storage::data_root(), repository);
        config.load();
        const auto& transport = config.settings().transport;
        const auto url = transport && transport->url ? *transport->url : std::string{};
        const auto api_key = transport ? transport->api_key : std::nullopt;
        if (!client || connection.url != url || connection.api_key != api_key) {
            if (client) {
                client->retire();
            }
            connection.url = url;
            connection.api_key = api_key;
            client = new MiningConnection(connection, &clients);
        }
    }

    std::string repository_profile_id() {
        if (auto current = profiles::ProfileRepository::at_data_root()) {
            repository = std::move(*current);
        }
        return repository.active_profile().id;
    }

    void refresh_settings() {
        repository_profile_id();
        load_settings();
    }

    // The active profile's ordered term list, reloaded from disk on each call.
    std::vector<anki::TermDictionary> load_term_dictionaries() {
        return dictionary::active_term_dictionaries(repository);
    }

    profiles::ProfileRepository repository;
    anki::AnkiConfigStore config{storage::data_root(), repository};
    anki::DuplicateLookupService duplicates{storage::data_root()};
    dictionary::DictionaryMediaService dictionary_media{storage::dictionaries_root()};
    anki::AnkiRequestFunction request = [this](auto action, auto params, auto callback) {
        client->send(std::move(action), std::move(params), std::move(callback));
    };
    anki::MiningService mining{
        duplicates,
        {.request = request,
         .download_audio =
             [this](QUrl url, anki::MiningDependencies::DownloadCallback callback) {
                 audio_download.get(url, std::move(callback));
             },
         .process_image = [](anki::MiningImage image, bool, anki::ImageCompressionFormat,
                             double) { return image; },
         .dictionary_media =
             [this](std::string_view dictionary, std::string_view path) {
                 QUrl url;
                 url.setScheme("dict");
                 QUrlQuery query;
                 const auto encoded = [](std::string_view text) {
                     return QString::fromLatin1(QUrl::toPercentEncoding(
                         QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()))));
                 };
                 query.addQueryItem("dictionary", encoded(dictionary));
                 query.addQueryItem("path", encoded(path));
                 url.setQuery(query);
                 return dictionary_media.get(url).bytes;
             },
         // No cover or Sasayaki audio is installed on a mining media server.
         .clear_media = [] {},
         .repository_profile_id = [this] { return repository_profile_id(); },
         .anki_profile_id = [this] { return std::string(config.active_profile_id()); },
         .activate_profile =
             [this](std::string_view id) {
                 if (config.active_profile_id() != id) {
                     load_settings();
                 }
             },
         .settings = [this]() -> const anki::AnkiSettings& { return config.settings(); }}};

    // Dropping the last host cancels pending callbacks before their seams die.
    audio::WebAudioDownload audio_download;
    QObject clients;
    anki::AnkiConnectConfig connection;
    MiningConnection* client = nullptr;
};

std::shared_ptr<MiningBackend> shared_backend();

}  // namespace detail

struct MiningViewModel::State {
    struct VideoContext {
        QString path;
        QString title;
        int audio_stream_index = -1;
        player::MiningSelection selection;
        double subtitle_delay = 0;
        double duration = 0;
    };

    explicit State(MiningViewModel& owner) {
        toast_timer.setSingleShot(true);
        toast_timer.setTimerType(Qt::PreciseTimer);
        QObject::connect(&toast_timer, &QTimer::timeout, &owner, [this, &owner] {
            toast.clear();
            emit owner.toastChanged();
        });
    }

    void show_toast(MiningViewModel& owner, QVariantMap payload) {
        toast = std::move(payload);
        toast_timer.start(detail::toast_lifetime_ms);
        emit owner.toastChanged();
    }

    const std::shared_ptr<detail::MiningBackend> backend = detail::shared_backend();
    anki::MiningContext context;
    std::vector<anki::TermDictionary> dictionaries;
    QVariantMap toast;
    QTimer toast_timer;
    std::optional<VideoContext> video;
    std::unique_ptr<VideoMiningCapture> capture;
    QString ffmpeg;
};

}  // namespace niratan::viewmodels

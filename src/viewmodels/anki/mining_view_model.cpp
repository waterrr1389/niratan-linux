// SPDX-License-Identifier: GPL-3.0-or-later

#include "viewmodels/anki/mining_view_model.h"

#include <QJSValue>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <utility>

#include "services/anki/anki_config.h"
#include "services/anki/anki_connect.h"
#include "services/anki/anki_requests.h"
#include "services/anki/duplicate_lookup.h"
#include "services/anki/mining.h"
#include "services/audio/web_audio_download.h"
#include "services/dictionary/dictionary_media.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

namespace niratan::viewmodels {
namespace {

constexpr int toast_lifetime_ms = 2200;

QVariant message_body(const QVariant& value) {
    if (value.metaType() == QMetaType::fromType<QJSValue>()) {
        return value.value<QJSValue>().toVariant();
    }
    return value;
}

std::optional<QVariantMap> string_map(const QVariant& body) {
    const auto value = message_body(body);
    if (value.typeId() != QMetaType::QVariantMap && value.typeId() != QMetaType::QVariantHash) {
        return std::nullopt;
    }
    auto map = value.toMap();
    if (!std::ranges::all_of(
            map, [](const QVariant& item) { return item.typeId() == QMetaType::QString; })) {
        return std::nullopt;
    }
    return map;
}

std::optional<qint64> decimal_id(const QString& text) {
    const auto digits = text.startsWith(u'+') || text.startsWith(u'-') ? QStringView(text).mid(1)
                                                                       : QStringView(text);
    if (digits.empty() ||
        !std::ranges::all_of(digits, [](QChar digit) { return digit >= u'0' && digit <= u'9'; })) {
        return std::nullopt;
    }
    bool ok = false;
    const auto id = text.toLongLong(&ok);
    return ok ? std::optional{id} : std::nullopt;
}

std::optional<QStringList> string_array(const QVariant& body) {
    const auto value = message_body(body);
    if (value.typeId() == QMetaType::QStringList) {
        return value.toStringList();
    }
    if (value.typeId() != QMetaType::QVariantList) {
        return std::nullopt;
    }
    QStringList strings;
    for (const auto& item : value.toList()) {
        if (item.typeId() != QMetaType::QString) {
            return std::nullopt;
        }
        strings.append(item.toString());
    }
    return strings;
}

// The two hosts share the saved words, active Anki settings and media cache,
// like AnkiManager.shared. Toasts and document context belong to each host.
class MiningBackend {
   public:
    explicit MiningBackend(profiles::ProfileRepository profile_repository)
        : repository(std::move(profile_repository)) {
        config.load();
        const auto& transport = config.settings().transport;
        anki::AnkiConnectConfig connection;
        if (transport) {
            connection.url = transport->url.value_or(std::string{});
            connection.api_key = transport->api_key;
        } else {
            connection.url.clear();
        }
        client.emplace(std::move(connection));
    }

    std::string repository_profile_id() {
        if (auto current = profiles::ProfileRepository::at_data_root()) {
            repository = std::move(*current);
        }
        return repository.active_profile().id;
    }

    void refresh_settings() { config.activate_profile(repository_profile_id()); }

    profiles::ProfileRepository repository;
    anki::AnkiConfigStore config{storage::data_root(), repository};
    anki::DuplicateLookupService duplicates{storage::data_root()};
    dictionary::DictionaryMediaService dictionary_media{storage::dictionaries_root()};
    anki::AnkiRequestFunction request = [this](auto action, auto params, auto callback) {
        client->request(std::move(action), std::move(params), std::move(callback));
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
         .activate_profile = [this](std::string_view id) { config.activate_profile(id); },
         .settings = [this]() -> const anki::AnkiSettings& { return config.settings(); }}};

    // Dropping the last host cancels pending callbacks before their seams die.
    audio::WebAudioDownload audio_download;
    std::optional<anki::AnkiConnectClient> client;
};

std::shared_ptr<MiningBackend> shared_backend() {
    static std::weak_ptr<MiningBackend> shared;
    if (auto backend = shared.lock()) {
        return backend;
    }
    if (auto repository = profiles::ProfileRepository::at_data_root()) {
        auto backend = std::make_shared<MiningBackend>(std::move(*repository));
        shared = backend;
        return backend;
    }
    return {};
}

}  // namespace

struct MiningViewModel::State {
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
        toast_timer.start(toast_lifetime_ms);
        emit owner.toastChanged();
    }

    const std::shared_ptr<MiningBackend> backend = shared_backend();
    anki::MiningContext context;
    std::vector<anki::TermDictionary> dictionaries;
    QVariantMap toast;
    QTimer toast_timer;
};

MiningViewModel::MiningViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>(*this)) {}

MiningViewModel::~MiningViewModel() = default;

QString MiningViewModel::toast_status() const {
    return state_->toast.value("status").toString();
}

QString MiningViewModel::toast_message() const {
    return state_->toast.value("message").toString();
}

QString MiningViewModel::toast_note_id() const {
    return state_->toast.value("noteID").toString();
}

void MiningViewModel::setContext(const QString& sentence, const QString& documentTitle,
                                 const QString& coverUrl, const QString& coverPath) {
    state_->context.sentence = sentence.toStdString();
    state_->context.document_title =
        documentTitle.isEmpty() ? std::nullopt : std::optional{documentTitle.toStdString()};
    state_->context.cover_url = coverUrl.toStdString();
    state_->context.cover_path = coverPath.toStdString();
}

void MiningViewModel::set_term_dictionaries(std::vector<anki::TermDictionary> dictionaries) {
    state_->dictionaries = std::move(dictionaries);
}

QVariantMap MiningViewModel::popupMiningOptions() {
    const anki::AnkiSettings defaults;
    const auto& backend = state_->backend;
    if (backend) {
        backend->refresh_settings();
    }
    const auto& settings = backend ? backend->config.settings() : defaults;
    const bool needs_audio = std::ranges::any_of(
        settings.field_mappings, [](const auto& field) { return field.second == "{audio}"; });
    return {{"needsAudio", needs_audio},
            {"allowDupes", settings.allow_dupes},
            {"useAnkiConnect", true},
            {"embedMedia", settings.embed_media},
            {"compactGlossariesAnki", settings.compact_glossaries}};
}

void MiningViewModel::mineEntry(int requestId, const QVariant& content) {
    const auto body = string_map(content);
    if (!body) {
        emit replyReady(requestId, {});
        return;
    }
    const auto finish = [host = QPointer(this), requestId](anki::AnkiMiningResult result) {
        const auto payload = result.web_payload().toVariantMap();
        if (host) {
            host->state_->show_toast(*host, payload);
        }
        if (host) {
            emit host->replyReady(requestId, payload);
        }
    };
    const auto backend = state_->backend;
    if (!backend) {
        finish({anki::AnkiMiningStatus::failed, tr("Unable to add card.").toStdString(),
                std::nullopt});
        return;
    }
    anki::MiningAttempt attempt{.content = {},
                                .context = state_->context,
                                .dictionaries = state_->dictionaries,
                                .profile_id = backend->repository_profile_id(),
                                .preflight_already_passed = true};
    for (auto entry = body->cbegin(); entry != body->cend(); ++entry) {
        attempt.content.emplace(entry.key().toStdString(), entry.value().toString().toStdString());
    }
    auto preflight_content = attempt.content;
    backend->mining.preflight(std::move(preflight_content),
                              [backend = backend.get(), attempt = std::move(attempt),
                               finish](std::optional<anki::AnkiMiningResult> verdict) mutable {
                                  if (verdict) {
                                      finish(std::move(*verdict));
                                  } else {
                                      backend->mining.mine(std::move(attempt), finish);
                                  }
                              });
}

void MiningViewModel::duplicateCheck(int requestId, const QVariant& word) {
    const auto value = message_body(word);
    if (value.typeId() != QMetaType::QString) {
        emit replyReady(requestId, {});
        return;
    }

    const auto finish = [host = QPointer(this), requestId](anki::DuplicateLookupResult result) {
        if (!host) {
            return;
        }
        QStringList ids;
        for (const auto id : result.note_ids) {
            ids.append(QString::number(id));
        }
        emit host->replyReady(requestId,
                              QVariantMap{{"isDuplicate", result.is_duplicate}, {"noteIDs", ids}});
    };
    if (const auto backend = state_->backend) {
        backend->refresh_settings();
        backend->duplicates.lookup(value.toString().toStdString(), backend->config.settings(),
                                   backend->request, finish);
    } else {
        finish({});
    }
}

void MiningViewModel::openAnkiNote(const QVariant& noteIds) {
    const auto strings = string_array(noteIds);
    if (!strings || !state_->backend) {
        return;
    }
    std::vector<std::int64_t> ids;
    for (const auto& string : *strings) {
        if (const auto id = decimal_id(string)) {
            ids.push_back(*id);
        }
    }
    if (auto request = anki::gui_browse_request(ids)) {
        state_->backend->request(std::move(request->action), std::move(request->params), {});
    }
}

void MiningViewModel::miningFeedback(const QVariant& result) {
    const auto body = string_map(result);
    if (!body) {
        return;
    }
    const auto status = body->value("status").toString();
    if (status != "added" && status != "duplicate" && status != "failed" && status != "pending") {
        return;
    }
    QVariantMap toast{{"status", status}, {"message", body->value("message").toString()}};
    if (const auto id = decimal_id(body->value("noteID").toString())) {
        toast.insert("noteID", QString::number(*id));
    }
    state_->show_toast(*this, std::move(toast));
}

void MiningViewModel::show_anki_note_button(int entry_index, qint64 note_id) {
    emit ankiNoteButtonRequested(entry_index, QString::number(note_id));
}

}  // namespace niratan::viewmodels

// SPDX-License-Identifier: GPL-3.0-or-later

#include "viewmodels/anki/mining_view_model.h"

#include "viewmodels/anki/mining_view_model_internal.h"

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
#include "services/anki/mining_gate.h"
#include "services/anki/popup_options.h"
#include "services/audio/web_audio_download.h"
#include "services/dictionary/dictionary_media.h"
#include "services/dictionary/term_dictionaries.h"
#include "services/profiles/profile_repository.h"
#include "viewmodels/anki/video_mining_capture.h"
#include "viewmodels/anki/video_mining_capture_start.h"

namespace niratan::viewmodels {
namespace {

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

}  // namespace

namespace detail {

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

}  // namespace detail

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
    state_->video.reset();
    state_->capture.reset();
    state_->context.sentence = sentence.toStdString();
    state_->context.document_title =
        documentTitle.isEmpty() ? std::nullopt : std::optional{documentTitle.toStdString()};
    state_->context.cover_url = coverUrl.toStdString();
    state_->context.cover_path = coverPath.toStdString();
}

void MiningViewModel::set_term_dictionaries(std::vector<anki::TermDictionary> dictionaries) {
    state_->dictionaries = std::move(dictionaries);
}

void MiningViewModel::setVideoContext(const QVariant& video) {
    // A pending capture keeps running: it copied its request when it started,
    // and the next cue or a closing popup must not abandon the add.
    const auto value = message_body(video);
    if (value.typeId() != QMetaType::QVariantMap) {
        state_->video.reset();
        return;
    }
    const auto map = value.toMap();
    State::VideoContext context;
    context.path = map.value("path").toString();
    context.title = map.value("title").toString();
    context.audio_stream_index = map.value("audioStreamIndex", -1).toInt();
    context.selection.sentence = map.value("cueText").toString();
    context.selection.cue_text = context.selection.sentence;
    context.selection.cue_start = map.value("cueStart").toDouble();
    context.selection.cue_end = map.value("cueEnd").toDouble();
    if (map.contains("previousCueText")) {
        context.selection.previous_cue_text = map.value("previousCueText").toString();
    }
    if (map.contains("nextCueText")) {
        context.selection.next_cue_text = map.value("nextCueText").toString();
    }
    context.subtitle_delay = map.value("subtitleDelay").toDouble();
    context.duration = map.value("duration").toDouble();
    state_->video = std::move(context);
}

void MiningViewModel::clearVideoContext() {
    state_->video.reset();
}

void MiningViewModel::screenshotFinished(bool ok) {
    if (state_->capture) {
        state_->capture->screenshot_finished(ok);
    }
}

void MiningViewModel::set_ffmpeg_path(QString ffmpeg) {
    state_->ffmpeg = std::move(ffmpeg);
}

QVariantMap MiningViewModel::popupMiningOptions() {
    const anki::AnkiSettings defaults;
    const auto& backend = state_->backend;
    if (backend) {
        backend->refresh_settings();
    }
    const auto& settings = backend ? backend->config.settings() : defaults;
    state_->dictionaries =
        backend ? backend->load_term_dictionaries() : std::vector<anki::TermDictionary>{};
    const anki::PopupOptions options = anki::popup_options(settings);
    return {{"needsAudio", options.needs_audio},
            {"allowDupes", options.allow_dupes},
            {"useAnkiConnect", options.use_anki_connect},
            {"embedMedia", options.embed_media},
            {"compactGlossariesAnki", options.compact_glossaries}};
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
            emit host->replyReady(requestId, payload);
        }
    };
    const auto backend = state_->backend;
    if (!backend) {
        finish({anki::AnkiMiningStatus::failed, tr("Unable to add card.").toStdString(),
                std::nullopt});
        return;
    }
    backend->refresh_settings();
    state_->dictionaries = backend->load_term_dictionaries();
    anki::MiningAttempt attempt{.content = {},
                                .context = state_->context,
                                .dictionaries = state_->dictionaries,
                                .profile_id = backend->repository_profile_id(),
                                .preflight_already_passed = true};
    for (auto entry = body->cbegin(); entry != body->cend(); ++entry) {
        attempt.content.emplace(entry.key().toStdString(), entry.value().toString().toStdString());
    }
    auto preflight_content = attempt.content;
    auto host = QPointer(this);
    backend->mining.preflight(
        std::move(preflight_content),
        [host, backend = backend.get(), attempt = std::move(attempt),
         finish](std::optional<anki::AnkiMiningResult> verdict) mutable {
            if (verdict) {
                finish(std::move(*verdict));
                return;
            }
            if (!host) {
                return;
            }
            if (host->state_->video) {
                host->start_video_capture(std::move(attempt), finish);
                return;
            }
            const auto& settings = backend->config.settings();
            auto pending = std::make_shared<anki::MiningAttempt>(std::move(attempt));
            auto add = [backend, pending, finish](QByteArray data, std::string extension) mutable {
                if (!data.isEmpty()) {
                    pending->context.sasayaki_audio_data = std::move(data);
                    pending->context.sasayaki_audio_format = std::move(extension);
                }
                backend->mining.mine(std::move(*pending), finish);
            };
            if (anki::needs_sasayaki_audio(settings.field_mappings) &&
                host->request_cue_audio(QString::fromStdString(pending->context.sentence),
                                        static_cast<int>(settings.audio_compression_format),
                                        settings.audio_compression_bitrate_kbps, std::move(add))) {
                return;
            }
            backend->mining.mine(std::move(*pending), finish);
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
        state_->backend->refresh_settings();
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

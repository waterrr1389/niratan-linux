// SPDX-License-Identifier: GPL-3.0-or-later

#include "mining.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryFile>

#include <memory>
#include <utility>

#include "services/anki/anki_requests.h"
#include "services/anki/media_naming.h"

namespace niratan::anki {

namespace mining_detail {

using MediaDirectory = std::optional<std::filesystem::path>;
using MediaDirectories = std::map<std::pair<std::string, std::string>, MediaDirectory>;
using NoteReady = std::function<void(QJsonObject)>;

namespace {

void send(const MiningDependencies& deps, Request request, AnkiConnectClient::Callback callback) {
    deps.request(std::move(request.action), std::move(request.params), std::move(callback));
}

QJsonDocument json_content(const Strings& content, std::string_view key) {
    const auto found = content.find(key);
    return found == content.end()
        ? QJsonDocument{}
        : QJsonDocument::fromJson(QByteArray::fromStdString(found->second));
}

Strings single_glossaries(const Strings& content) {
    const QJsonObject glossaries = json_content(content, "singleGlossaries").object();
    Strings decoded;
    for (auto it = glossaries.begin(); it != glossaries.end(); ++it) {
        if (!it.value().isString()) {
            return {};
        }
        decoded.emplace(it.key().toStdString(), it.value().toString().toStdString());
    }
    return decoded;
}

struct DictionaryMedia {
    std::string dictionary;
    std::string path;
    QString filename;
};

std::vector<DictionaryMedia> dictionary_items(const Strings& content) {
    std::vector<DictionaryMedia> items;
    for (const auto& value : json_content(content, "dictionaryMedia").array()) {
        const auto item = value.toObject();
        const auto dictionary = item.value("dictionary");
        const auto path = item.value("path");
        const auto filename = item.value("filename");
        if (!dictionary.isString() || !path.isString() || !filename.isString()) {
            return {};
        }
        items.push_back({dictionary.toString().toStdString(), path.toString().toStdString(),
                         filename.toString()});
    }
    return items;
}

std::optional<std::string> write_direct_media(const MediaDirectory& directory,
                                              const std::string& filename,
                                              const QByteArray& bytes) {
    if (!directory) {
        return std::nullopt;
    }
    const auto safe = safe_media_filename(filename);
    const auto destination = *directory / safe;
    if (QFileInfo(destination).exists()) {
        return safe;
    }
    QTemporaryFile temporary(*directory / ("." + safe + ".XXXXXX.tmp"));
    if (!temporary.open() || temporary.write(bytes) != bytes.size() || !temporary.flush() ||
        !temporary.rename(destination)) {
        return std::nullopt;
    }
    temporary.setAutoRemove(false);
    return safe;
}

std::optional<QByteArray> read_media(const std::string& path) {
    QFile file(QString::fromStdString(path));
    if (!QFileInfo(file).isFile() || !file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QByteArray data = file.readAll();
    return file.error() == QFileDevice::NoError ? std::optional(std::move(data)) : std::nullopt;
}

std::string path_extension(const std::string& path, std::string_view fallback) {
    const auto ext = std::filesystem::path(path).extension().string();
    return safe_media_extension(ext.empty() ? std::string_view{} : std::string_view(ext).substr(1),
                                fallback);
}

std::string media_filename(std::string_view prefix, const QByteArray& data,
                           std::string_view extension) {
    return std::string(prefix) +
        QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().toStdString() + "." +
        std::string(extension);
}

QJsonObject attachment(const std::string& filename, const QByteArray& bytes,
                       const QJsonArray& fields) {
    return {{"filename", QString::fromStdString(filename)},
            {"data", QString::fromLatin1(bytes.toBase64())},
            {"fields", fields}};
}

QJsonObject note_options(const AnkiSettings& config, std::string_view deck) {
    const auto transport = config.transport.value_or(AnkiTransportConfig{});
    const auto options = duplicate_options(transport.duplicate_scope, deck, config.allow_dupes,
                                           transport.check_all_models);
    QJsonObject json{{"allowDuplicate", options.allow_duplicate},
                     {"duplicateScope", QString::fromStdString(options.scope)}};
    if (const auto& scope = options.scope_options) {
        QJsonObject detail;
        if (scope->deck_name) {
            detail.insert("deckName", QString::fromStdString(*scope->deck_name));
        }
        if (scope->check_children) {
            detail.insert("checkChildren", true);
        }
        if (scope->check_all_models) {
            detail.insert("checkAllModels", true);
        }
        json.insert("duplicateScopeOptions", detail);
    }
    return json;
}

class NoteBuilder : public std::enable_shared_from_this<NoteBuilder> {
   public:
    NoteBuilder(const MiningDependencies& deps, MiningAttempt attempt, const AnkiSettings& config,
                NoteReady ready)
        : deps_(deps), attempt_(std::move(attempt)), config_(config), ready_(std::move(ready)),
          dictionary_items_(dictionary_items(attempt_.content)) {
        const auto glossaries = single_glossaries(attempt_.content);
        for (const auto& [key, mapping] : config_.field_mappings) {
            const auto field = QString::fromStdString(key);
            if (mapping == "{audio}") {
                web_audio_.append(field);
            } else if (mapping == "{sasayaki-audio}") {
                (attempt_.context.video ? video_audio_ : sasayaki_audio_).append(field);
            } else if (mapping == "{video-audio-clip}") {
                video_audio_.append(field);
            } else if (mapping == "{book-cover}") {
                (attempt_.context.video ? video_frame_ : cover_).append(field);
            } else if (mapping == "{video-screenshot}") {
                video_frame_.append(field);
            } else {
                const auto text = render_field(mapping, attempt_.context, attempt_.content,
                                               glossaries, attempt_.dictionaries);
                fields_.insert(field, QString::fromStdString(text));
            }
        }
    }

    bool needs_media_directory() const {
        return !sasayaki_audio_.isEmpty() || !video_audio_.isEmpty() || !cover_.isEmpty() ||
            !video_frame_.isEmpty() || !dictionary_items_.empty();
    }

    void with_media_directory(MediaDirectory directory) {
        directory_ = std::move(directory);
        if (!web_audio_.isEmpty()) {
            if (const auto audio = attempt_.content.find("audio");
                audio != attempt_.content.end()) {
                const QUrl url(QString::fromStdString(audio->second));
                const auto scheme = url.scheme();
                if (url.isValid() && (scheme == "http" || scheme == "https" || scheme == "file")) {
                    deps_.download_audio(
                        url, [self = shared_from_this()](std::optional<QByteArray> data) {
                            if (data) {
                                self->audio_.append(
                                    attachment(media_filename("hoshi_audio_", *data, "mp3"), *data,
                                               self->web_audio_));
                            }
                            self->append_local_media();
                        });
                    return;
                }
            }
        }
        append_local_media();
    }

   private:
    void set_media_fields(const QJsonArray& names, const std::string& markup) {
        for (const auto& name : names) {
            fields_.insert(name.toString(), QString::fromStdString(markup));
        }
    }

    void append_video_media(const QJsonArray& fields, const std::optional<std::string>& filename,
                            const std::optional<std::string>& path, bool audio) {
        if (fields.isEmpty()) {
            return;
        }
        if (directory_ && filename) {
            set_media_fields(
                fields, audio ? direct_audio_markup(*filename) : direct_image_markup(*filename));
        } else if (path) {
            if (const auto data = read_media(*path)) {
                // AnkiAudioCompressionFormat.fileExtension in Models/Anki.swift.
                const auto audio_ext =
                    config_.audio_compression_format == AudioCompressionFormat::aac ? "m4a" : "mp3";
                const auto ext = path_extension(*path, audio ? audio_ext : "png");
                const auto name =
                    media_filename(audio ? "hoshi_video_audio_" : "hoshi_video_frame_", *data, ext);
                (audio ? audio_ : pictures_).append(attachment(name, *data, fields));
            }
        }
    }

    void append_cover() {
        const auto& path = attempt_.context.cover_path;
        if (cover_.isEmpty() || path.empty()) {
            return;
        }
        if (auto data = read_media(path)) {
            const auto image = deps_.process_image(
                {std::move(*data), path_extension(path, "png")}, config_.compress_images,
                config_.image_compression_format, config_.image_compression_quality);
            const auto name = media_filename("hoshi_cover_", image.bytes, image.extension);
            if (const auto direct = write_direct_media(directory_, name, image.bytes)) {
                set_media_fields(cover_, direct_image_markup(*direct));
            } else {
                pictures_.append(attachment(name, image.bytes, cover_));
            }
        }
    }

    // AnkiManager.addNoteAnkiConnect: a non-video context's Sasayaki clip is
    // named from its bytes and the context's extension, written directly when
    // a media directory is usable and uploaded otherwise. Video keeps its
    // video-audio bucket, so its sasayaki fields are never here.
    void append_sasayaki_audio() {
        if (sasayaki_audio_.isEmpty() || !attempt_.context.sasayaki_audio_data.has_value()) {
            return;
        }
        const QByteArray& data = *attempt_.context.sasayaki_audio_data;
        const std::string_view extension = attempt_.context.sasayaki_audio_format.empty()
            ? std::string_view("m4a")
            : std::string_view(attempt_.context.sasayaki_audio_format);
        const auto name = media_filename("hoshi_sasayaki_", data, extension);
        if (const auto direct = write_direct_media(directory_, name, data)) {
            set_media_fields(sasayaki_audio_, direct_audio_markup(*direct));
        } else {
            audio_.append(attachment(name, data, sasayaki_audio_));
        }
    }

    void append_local_media() {
        append_sasayaki_audio();
        const auto& video = attempt_.context.video;
        if (video) {
            append_video_media(video_audio_, video->audio_clip_filename, video->audio_clip_path,
                               true);
        }
        append_cover();
        if (video) {
            append_video_media(video_frame_, video->screenshot_filename, video->screenshot_path,
                               false);
        }
        append_dictionary_media();
    }

    void append_dictionary_media() {
        while (dictionary_index_ < dictionary_items_.size()) {
            const auto& item = dictionary_items_[dictionary_index_++];
            const auto bytes = deps_.dictionary_media(item.dictionary, item.path);
            const auto name =
                media_filename("hoshi_dict_", bytes, path_extension(item.path, "bin"));
            const auto direct = write_direct_media(directory_, name, bytes);
            const auto replacement = QString::fromStdString(direct ? *direct : name);
            if (!item.filename.isEmpty()) {
                // Literal replacement, as Foundation's replacingOccurrences
                // matches the filename.
                for (auto it = fields_.begin(); it != fields_.end(); ++it) {
                    it.value() = it.value().toString().replace(item.filename, replacement);
                }
            }
            if (direct) {
                continue;
            }
            // Resume after each upload. Inline replies let this loop continue
            // instead of recursing once for every dictionary item.
            sending_upload_ = waiting_for_upload_ = true;
            send(deps_, store_media_file_request(name, bytes),
                 [self = shared_from_this()](AnkiConnectResult) {
                     self->waiting_for_upload_ = false;
                     if (!self->sending_upload_) {
                         self->append_dictionary_media();
                     }
                 });
            sending_upload_ = false;
            if (waiting_for_upload_) {
                return;
            }
        }
        finish();
    }

    void finish() {
        QJsonObject note{{"deckName", QString::fromStdString(*config_.selected_deck)},
                         {"modelName", QString::fromStdString(*config_.selected_note_type)},
                         {"options", note_options(config_, *config_.selected_deck)},
                         {"fields", fields_}};
        if (!audio_.isEmpty()) {
            note.insert("audio", audio_);
        }
        if (!pictures_.isEmpty()) {
            note.insert("picture", pictures_);
        }
        const auto tags = QString::fromStdString(config_.tags).split(' ', Qt::SkipEmptyParts);
        if (!tags.isEmpty()) {
            note.insert("tags", QJsonArray::fromStringList(tags));
        }
        ready_(std::move(note));
    }

    const MiningDependencies& deps_;
    const MiningAttempt attempt_;
    const AnkiSettings config_;
    NoteReady ready_;
    const std::vector<DictionaryMedia> dictionary_items_;
    MediaDirectory directory_;
    QJsonObject fields_;
    QJsonArray web_audio_;
    QJsonArray sasayaki_audio_;
    QJsonArray video_audio_;
    QJsonArray cover_;
    QJsonArray video_frame_;
    QJsonArray audio_;
    QJsonArray pictures_;
    std::size_t dictionary_index_ = 0;
    bool sending_upload_ = false;
    bool waiting_for_upload_ = false;
};

}  // namespace

void resolve_media_directory(const MiningDependencies& deps, MediaDirectories& cache,
                             std::function<void(MediaDirectory)> callback) {
    const auto transport = deps.settings().transport.value_or(AnkiTransportConfig{});
    auto url = transport.url.value_or("");
    if (QString::fromStdString(url).trimmed().isEmpty()) {
        url = *AnkiTransportConfig{}.url;
    }
    const auto key = std::pair{url, transport.api_key.value_or("")};
    if (const auto found = cache.find(key); found != cache.end()) {
        callback(found->second);
        return;
    }
    send(deps, media_dir_path_request(),
         [&cache, key, callback = std::move(callback)](AnkiConnectResult reply) {
             MediaDirectory directory;
             if (const auto path = parse_media_dir_path_result(reply)) {
                 const QFileInfo info(QString::fromStdString(*path));
                 if (info.isDir() && info.isWritable()) {
                     directory = info.filesystemAbsoluteFilePath();
                 }
             }
             cache.insert_or_assign(key, directory);
             callback(std::move(directory));
         });
}

void build_card(const MiningDependencies& deps, MediaDirectories& cache, MiningAttempt attempt,
                const AnkiSettings& settings, NoteReady ready) {
    auto builder =
        std::make_shared<NoteBuilder>(deps, std::move(attempt), settings, std::move(ready));
    if (builder->needs_media_directory()) {
        resolve_media_directory(deps, cache, [builder](MediaDirectory directory) {
            builder->with_media_directory(std::move(directory));
        });
    } else {
        builder->with_media_directory(std::nullopt);
    }
}

}  // namespace mining_detail

}  // namespace niratan::anki

// SPDX-License-Identifier: GPL-3.0-or-later

#include "cue_audio.h"

#include <QDir>
#include <QFile>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#include <QProcess>

#include "services/reader/character_normalizer.h"

namespace niratan::sasayaki {

namespace {

constexpr double rate_epsilon = 0.001;

QString fixed_time(double value) {
    return QString::number(value, 'f', 6);
}

QString audio_extension(anki::AudioCompressionFormat format) {
    return format == anki::AudioCompressionFormat::aac ? QStringLiteral("m4a")
                                                       : QStringLiteral("mp3");
}

QString audio_codec(anki::AudioCompressionFormat format) {
    return format == anki::AudioCompressionFormat::aac ? QStringLiteral("aac")
                                                       : QStringLiteral("libmp3lame");
}

std::string format_raw(anki::AudioCompressionFormat format) {
    return format == anki::AudioCompressionFormat::aac ? "aac" : "mp3";
}

CueAudioRange clipped_range(double start, double end) {
    const double clipped_start = std::max(0.0, start);
    return {clipped_start, std::max(clipped_start, end)};
}

QString rate_text(double rate) {
    return QString::number(rate, 'g', 10);
}

QString atempo(double rate) {
    if (rate <= 2.0) {
        return QStringLiteral("atempo=") + rate_text(rate);
    }
    return QStringLiteral("atempo=2,atempo=") + rate_text(rate / 2.0);
}

QString temporary_stem() {
    return QDir::temp().filePath(QStringLiteral("sasayaki-audio-") +
                                 QUuid::createUuid().toString(QUuid::WithoutBraces));
}

}  // namespace

CueAudioRange expand_cue(const std::vector<SasayakiMatch>& matches, const SasayakiMatch& cue,
                         std::string_view sentence) {
    std::vector<const SasayakiMatch*> chapter;
    for (const auto& match : matches) {
        if (match.chapter_index == cue.chapter_index) {
            chapter.push_back(&match);
        }
    }
    const auto found =
        std::ranges::find_if(chapter, [&cue](const auto* match) { return match->id == cue.id; });
    if (found == chapter.end()) {
        return {cue.start_time, cue.end_time};
    }

    const auto index = static_cast<std::size_t>(found - chapter.begin());
    const std::string filtered_sentence = reader::filtered_text(sentence);
    const auto contained = [&filtered_sentence](const SasayakiMatch& match) {
        return filtered_sentence.find(reader::filtered_text(match.text)) != std::string::npos;
    };
    std::size_t start = index;
    std::size_t end = index;
    while (start > 0 && contained(*chapter[start - 1])) {
        --start;
    }
    while (end + 1 < chapter.size() && contained(*chapter[end + 1])) {
        ++end;
    }
    return {chapter[start]->start_time, chapter[end]->end_time};
}

CueAudioRange cue_clip_range(const CueAudioRange& range, double delay) {
    return clipped_range(range.start + delay, range.end + delay);
}

std::string cue_audio_cache_key(const CueAudioRequest& request) {
    const CueAudioRange range = clipped_range(request.start, request.end);
    return request.source_path.toStdString() + "\n" +
        std::to_string(std::llround(range.start * 1000.0)) + "\n" +
        std::to_string(std::llround(range.end * 1000.0)) + "\n" + format_raw(request.format) +
        "\n" + std::to_string(request.bitrate_kbps) + "\n" +
        std::to_string(std::llround(request.rate * 100.0));
}

QStringList cue_audio_arguments(const CueAudioRequest& request) {
    const CueAudioRange range = clipped_range(request.start, request.end);
    const int bitrate = std::clamp(request.bitrate_kbps, 32, 192);
    QStringList arguments{"-nostdin", "-y",
                          "-i",       request.source_path,
                          "-ss",      fixed_time(range.start),
                          "-t",       fixed_time(range.end - range.start)};
    if (std::abs(request.rate - 1.0) >= rate_epsilon) {
        arguments << "-filter:a" << atempo(request.rate);
    }
    arguments << "-vn" << "-ac" << "1" << "-c:a" << audio_codec(request.format) << "-b:a"
              << (QString::number(bitrate) + "k")
              << (request.output_stem + "." + audio_extension(request.format));
    return arguments;
}

CueAudioExporter::CueAudioExporter(QString ffmpeg, QObject* parent)
    : QObject(parent), ffmpeg_(std::move(ffmpeg)) {}

CueAudioExporter::~CueAudioExporter() {
    if (process_) {
        process_->disconnect(this);
        process_->kill();
        process_->waitForFinished(1000);
    }
}

void CueAudioExporter::start(const CueAudioRequest& request) {
    pending_.push_back(request);
    process_next();
}

void CueAudioExporter::clear_cache() {
    cache_.clear();
}

void CueAudioExporter::process_next() {
    while (!process_ && !pending_.empty()) {
        CueAudioRequest request = std::move(pending_.front());
        pending_.pop_front();
        cache_key_ = cue_audio_cache_key(request);
        if (const auto cached = cache_.find(cache_key_); cached != cache_.end()) {
            emit finished(cached->second);
            continue;
        }

        if (request.output_stem.isEmpty()) {
            request.output_stem = temporary_stem();
        }
        const CueAudioRange range = clipped_range(request.start, request.end);
        output_path_ = request.output_stem + "." + audio_extension(request.format);
        if (!(range.end > range.start)) {
            emit finished({});
            continue;
        }

        QFile::remove(output_path_);
        completed_ = false;
        process_ = new QProcess(this);
        connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                fail();
            }
        });
        connect(process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int code, QProcess::ExitStatus status) {
                    if (status != QProcess::NormalExit || code != 0) {
                        fail();
                        return;
                    }
                    QFile output(output_path_);
                    QByteArray data;
                    if (output.open(QIODevice::ReadOnly)) {
                        data = output.readAll();
                    }
                    output.close();
                    QFile::remove(output_path_);
                    if (data.isEmpty()) {
                        fail();
                        return;
                    }
                    complete(data);
                });
        const QString program = ffmpeg_.isEmpty() ? QStringLiteral("ffmpeg") : ffmpeg_;
        process_->start(program, cue_audio_arguments(request));
        return;
    }
}

void CueAudioExporter::complete(const QByteArray& data) {
    if (completed_) {
        return;
    }
    completed_ = true;
    if (process_) {
        process_->deleteLater();
        process_ = nullptr;
    }
    if (!data.isEmpty()) {
        cache_[cache_key_] = data;
    }
    emit finished(data);
    process_next();
}

void CueAudioExporter::fail() {
    QFile::remove(output_path_);
    complete({});
}

}  // namespace niratan::sasayaki

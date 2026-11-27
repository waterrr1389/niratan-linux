// SPDX-License-Identifier: GPL-3.0-or-later

#include "transcript_extractor.h"

#include "services/storage/app_roots.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <filesystem>
#include <system_error>
#include <utility>

namespace niratan::player {

std::optional<QByteArray> read_subtitle_document(const QString& path, qint64 limit) {
    QFile file(path);
    if (!QFileInfo(file).isFile() || !file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray bytes = file.read(limit + 1);
    if (file.error() != QFile::NoError || bytes.size() > limit) {
        return std::nullopt;
    }
    return bytes;
}

QString transcript_cache_file(const QString& cache_directory, const QString& video_path,
                              int track_id) {
    const auto hash = QCryptographicHash::hash(video_path.toUtf8(), QCryptographicHash::Sha256);
    return QDir(cache_directory).filePath(
        QStringLiteral("%1-%2.srt").arg(QString::fromLatin1(hash.toHex())).arg(track_id));
}

QString TranscriptExtractor::default_cache_directory() {
    const auto root = storage::data_root();
    return root.empty() ? QString{}
                        : QString::fromStdString((root / "Cache/video-subtitles").string());
}

TranscriptExtractor::TranscriptExtractor(QString cache_directory, QString ffmpeg, QObject* parent)
    : QObject(parent), cache_directory_(std::move(cache_directory)), ffmpeg_(std::move(ffmpeg)) {}

TranscriptExtractor::~TranscriptExtractor() {
    if (process_ != nullptr) {
        process_->disconnect(this);
        process_->kill();
    }
}

void TranscriptExtractor::start(const QString& video_path, int track_id, int stream_index) {
    if (running()) {
        return;
    }
    const auto fail = [this](const QString& error) { emit finished({}, error); };
    if (cache_directory_.isEmpty() || !QDir().mkpath(cache_directory_)) {
        fail(tr("Could not create the embedded transcript cache."));
        return;
    }
    const QString cache = transcript_cache_file(cache_directory_, video_path, track_id);
    const auto modified = QFileInfo(video_path).lastModified();
    if (modified.isValid() && QFileInfo(cache).lastModified() == modified) {
        read_transcript(cache);
        return;
    }
    const QString ffmpeg =
        ffmpeg_.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("ffmpeg")) : ffmpeg_;
    if (ffmpeg.isEmpty()) {
        fail(tr("ffmpeg was not found on PATH."));
        return;
    }

    auto* process = new QProcess(this);
    auto* staging = new QTemporaryFile(cache + QStringLiteral(".XXXXXX.srt"), process);
    if (!staging->open()) {
        delete process;
        fail(tr("Could not create a temporary transcript file."));
        return;
    }
    staging->close();
    connect(process, &QProcess::errorOccurred, this, [this, fail](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            stop();
            fail(tr("Could not start ffmpeg to extract the transcript."));
        }
    });
    connect(process, &QProcess::finished, this,
            [this, fail, staging, cache, video_path, modified](int code,
                                                               QProcess::ExitStatus status) {
        const QString staged = staging->fileName();
        stop();
        if (status != QProcess::NormalExit || code != 0) {
            fail(tr("ffmpeg could not extract this subtitle track as text."));
            return;
        }
        complete(staged, cache, video_path, modified);
    });
    process_ = process;
    emit runningChanged();
    process->setStandardOutputFile(QProcess::nullDevice());
    process->setStandardErrorFile(QProcess::nullDevice());
    process->start(ffmpeg, {QStringLiteral("-nostdin"), QStringLiteral("-y"), QStringLiteral("-i"),
                            video_path, QStringLiteral("-map"),
                            QStringLiteral("0:%1").arg(stream_index), QStringLiteral("-c:s"),
                            QStringLiteral("srt"), staging->fileName()});
}

void TranscriptExtractor::cancel() {
    if (auto* process = std::exchange(process_, nullptr)) {
        process->disconnect(this);
        process->kill();
        process->deleteLater();
        emit runningChanged();
    }
}

void TranscriptExtractor::stop() {
    if (auto* process = std::exchange(process_, nullptr)) {
        process->deleteLater();
        emit runningChanged();
    }
}

void TranscriptExtractor::complete(const QString& staging, const QString& cache,
                                   const QString& video_path, const QDateTime& modified) {
    if (QFileInfo(video_path).lastModified() != modified) {
        emit finished({}, tr("The video changed while extracting its transcript. Please try again."));
        return;
    }
    QFile output(staging);
    if (modified.isValid() && output.open(QIODevice::ReadWrite) &&
        output.setFileTime(modified, QFileDevice::FileModificationTime)) {
        output.close();
        std::error_code error;
        std::filesystem::rename(staging.toStdString(), cache.toStdString(), error);
        if (!error) {
            read_transcript(cache);
            return;
        }
    }
    read_transcript(staging);
}

void TranscriptExtractor::read_transcript(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit finished({}, tr("Could not read the extracted transcript."));
        return;
    }
    const auto bytes = read_subtitle_document(path);
    if (!bytes) {
        emit finished({}, tr("Could not read the extracted transcript, or it is too large."));
        return;
    }
    if (bytes->isEmpty()) {
        emit finished({}, tr("No readable cues were found in the embedded subtitle track."));
        return;
    }
    emit finished(*bytes, {});
}

}  // namespace niratan::player

// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_clip.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QString>

#include <algorithm>
#include <utility>

namespace niratan::player {

namespace {

QString capture_error() {
    return QCoreApplication::translate("AnkiMining", "Unable to capture the subtitle audio clip.");
}

QString fixed_time(double value) {
    return QString::number(value, 'f', 6);
}

QString audio_extension(anki::AudioCompressionFormat format) {
    return format == anki::AudioCompressionFormat::aac ? QStringLiteral("m4a")
                                                       : QStringLiteral("mp3");
}

}  // namespace

QString audio_clip_output_path(const QString& output_stem, anki::AudioCompressionFormat format) {
    return output_stem + "." + audio_extension(format);
}

QStringList audio_clip_arguments(const AudioClipRequest& request) {
    const double start = std::max(0.0, request.start);
    const int bitrate = std::min(192, std::max(32, request.bitrate_kbps));
    QStringList arguments{"-nostdin", "-y",
                          "-i",       request.source_path,
                          "-ss",      fixed_time(start),
                          "-t",       fixed_time(request.end - start)};
    if (request.audio_stream_index >= 0) {
        arguments << "-map" << QStringLiteral("0:%1").arg(request.audio_stream_index);
    }
    arguments << "-vn" << "-ac" << "1" << "-c:a"
              << (request.format == anki::AudioCompressionFormat::aac ? "aac" : "libmp3lame")
              << "-b:a" << (QString::number(bitrate) + "k")
              << audio_clip_output_path(request.output_stem, request.format);
    return arguments;
}

AudioClipExporter::AudioClipExporter(QString ffmpeg, QObject* parent)
    : QObject(parent), ffmpeg_(std::move(ffmpeg)) {}

AudioClipExporter::~AudioClipExporter() {
    if (process_) {
        process_->disconnect(this);
        process_->kill();
        process_->waitForFinished(1000);
    }
}

void AudioClipExporter::start(AudioClipRequest request) {
    if (process_) {
        return;
    }
    output_path_ = audio_clip_output_path(request.output_stem, request.format);
    completed_ = false;
    if (!(request.end > request.start)) {
        emit finished({}, capture_error());
        return;
    }
    QFile::remove(output_path_);
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
                const QFileInfo output(output_path_);
                if (!output.isFile() || output.size() <= 0) {
                    fail();
                    return;
                }
                complete(output_path_, {});
            });
    const QString program = ffmpeg_.isEmpty() ? QStringLiteral("ffmpeg") : ffmpeg_;
    process_->start(program, audio_clip_arguments(request));
}

void AudioClipExporter::complete(const QString& path, const QString& error) {
    if (completed_) {
        return;
    }
    completed_ = true;
    if (process_) {
        process_->deleteLater();
        process_ = nullptr;
    }
    emit finished(path, error);
}

void AudioClipExporter::fail() {
    QFile::remove(output_path_);
    complete({}, capture_error());
}

}  // namespace niratan::player

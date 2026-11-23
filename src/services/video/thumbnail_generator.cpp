// SPDX-License-Identifier: GPL-3.0-or-later
//
// The ffmpeg poster generator. Follows Features/Video/Playback/HSMpvClient.mm
// (`HSMpvRenderThumbnailPNGData`, `HSMpvPNGDataByLimitingMaximumDimension`)
// with the ffmpeg command from Decision D2 as stated in thumbnail_store.h.

#include "services/video/thumbnail_store.h"

#include <QProcess>

#include <utility>

namespace niratan::video {
namespace {

constexpr int kDefaultCaptureSeconds = 5;

}  // namespace

QStringList thumbnail_generator_arguments(const QString& source_path, int maximum_dimension,
                                          int start_seconds) {
    const QString scale = QStringLiteral("scale='min(%1,iw)':'min(%1,ih)':"
                                         "force_original_aspect_ratio=decrease")
                              .arg(maximum_dimension);
    return {QStringLiteral("-nostdin"),  QStringLiteral("-y"),
            QStringLiteral("-ss"),       QString::number(start_seconds),
            QStringLiteral("-i"),        source_path,
            QStringLiteral("-frames:v"), QStringLiteral("1"),
            QStringLiteral("-vf"),       scale,
            QStringLiteral("-f"),        QStringLiteral("image2pipe"),
            QStringLiteral("-c:v"),      QStringLiteral("png"),
            QStringLiteral("-")};
}

FfmpegThumbnailGenerator::FfmpegThumbnailGenerator(QString executable)
    : executable_(std::move(executable)) {}

QByteArray FfmpegThumbnailGenerator::run(const QString& path, int maximum_dimension,
                                         int start_seconds,
                                         const ThumbnailCancelled& is_cancelled) const {
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    const QString program = executable_.isEmpty() ? QStringLiteral("ffmpeg") : executable_;
    process.start(program, thumbnail_generator_arguments(path, maximum_dimension, start_seconds));
    if (!process.waitForStarted()) {
        return {};
    }
    while (!process.waitForFinished(20)) {
        if (is_cancelled && is_cancelled()) {
            process.kill();
            process.waitForFinished(1000);
            return {};
        }
    }
    if (is_cancelled && is_cancelled()) {
        return {};
    }
    return process.readAllStandardOutput();
}

QByteArray FfmpegThumbnailGenerator::generate(const QString& path, int maximum_dimension,
                                              const ThumbnailCancelled& is_cancelled) {
    if (is_cancelled && is_cancelled()) {
        return {};
    }
    QByteArray bytes = run(path, maximum_dimension, kDefaultCaptureSeconds, is_cancelled);
    if (!bytes.isEmpty() || (is_cancelled && is_cancelled())) {
        return bytes;
    }
    // A file shorter than the capture time yields no frame; the first frame
    // is the fallback (Decision D2).
    bytes = run(path, maximum_dimension, 0, is_cancelled);
    return bytes;
}

}  // namespace niratan::video

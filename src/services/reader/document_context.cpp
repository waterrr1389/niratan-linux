// SPDX-License-Identifier: GPL-3.0-or-later

#include "document_context.h"

#include <QFile>

#include "services/audio/local_audio.h"

namespace niratan::reader {

std::string document_title(std::string_view epub_title, const books::BookMetadata& book) {
    return epub_title.empty() ? book.display_title() : std::string(epub_title);
}

std::optional<std::filesystem::path> document_cover_path(const books::BookStorage& storage,
                                                         const books::BookMetadata& book) {
    return storage.cover_path(book);
}

std::string document_cover_url(const std::filesystem::path& cover_path) {
    if (cover_path.empty()) {
        return {};
    }
    std::string extension = cover_path.extension().string();
    if (extension.starts_with('.')) {
        extension.erase(0, 1);
    }
    return "http://localhost:" + std::to_string(audio::kLocalAudioPort) + "/cover/cover." +
        extension;
}

std::optional<QByteArray> read_document_cover(const std::filesystem::path& cover_path) {
    QFile file(QString::fromStdString(cover_path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return std::nullopt;
    }
    return bytes;
}

}  // namespace niratan::reader

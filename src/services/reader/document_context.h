// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reader mining metadata from NativeReaderView.swift (NativeReaderModel.title,
// coverURL), EPUBDocument.swift (title), Book.swift (BookMetadata.displayTitle),
// and Extensions.swift (BookMetadata.coverURL). A nonempty EPUB title wins over
// the library display title. Linux uses the display title for both missing and
// explicitly empty EPUB titles.
//
// A cover reference starting with '/' is absolute; all others are relative to
// the app directory. Path resolution does not require a readable file.
// LocalFileServer.swift (setCover, port) and AnkiManager.swift (handlebarToValue,
// addNoteAnkiConnect) use the same file for the served cover and the attachment.
// The URL formatter uses the path extension as-is, including an empty extension.
// By default the reader serves both covers and gallery images on port
// 18765. If it cannot bind there, the gallery uses an ephemeral port, the
// cover URL is empty, and the attachment path is still available.

#pragma once

#include <QByteArray>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "services/books/book_storage.h"

namespace niratan::reader {

std::string document_title(std::string_view epub_title, const books::BookMetadata& book);

std::optional<std::filesystem::path> document_cover_path(const books::BookStorage& storage,
                                                         const books::BookMetadata& book);

// An empty path has no URL. Formatting does not read the cover or start a server.
std::string document_cover_url(const std::filesystem::path& cover_path, quint16 port);

// A read failure supplies no attachment bytes but does not change the URL.
std::optional<QByteArray> read_document_cover(const std::filesystem::path& cover_path);

}  // namespace niratan::reader

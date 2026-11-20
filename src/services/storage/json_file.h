// SPDX-License-Identifier: GPL-3.0-or-later
//
// The JSON sidecar file store, following SPEC.md 3.5 and Niratan-win's
// Services/Novels/NiratanJsonFileStore.cs.
//
// read_json_file separates a missing file from a damaged one: a file that
// cannot be read or parsed is invalid and carries the reason, so a caller can
// keep the original bytes and warn instead of treating the file as empty.
// Nothing is normalized on read.
//
// write_json_file writes through QSaveFile: the bytes go to a temporary file
// in the target directory and replace the target only on commit, so a half
// written sidecar is never visible at the target path. Before writing it
// refuses to replace an existing file that is not valid JSON, which keeps a
// damaged sidecar from being overwritten by a normalization pass; the caller
// must fix or remove the file explicitly. Parent directories are created.
// The document is written indented, with the same JSON member names as the
// document it receives.

#pragma once

#include <QJsonDocument>

#include <filesystem>
#include <string>

namespace niratan::storage {

enum class JsonReadStatus { missing, success, invalid };

struct JsonReadResult {
    JsonReadStatus status = JsonReadStatus::missing;
    QJsonDocument document;
    // Empty unless status is invalid.
    std::string error;
};

JsonReadResult read_json_file(const std::filesystem::path& path);

// False on any failure, including a damaged target and an unwritable path.
// error, when given, receives a reason. The target is left unchanged.
bool write_json_file(const std::filesystem::path& path, const QJsonDocument& document,
                     std::string* error = nullptr);

}  // namespace niratan::storage

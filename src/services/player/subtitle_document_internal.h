// SPDX-License-Identifier: GPL-3.0-or-later
//
// Helpers shared by the SRT/WebVTT parser and the ASS/SSA parser behind
// subtitle_document.h. This is not a public header: only subtitle_document.cpp
// and subtitle_ass.cpp include it.

#pragma once

#include "subtitle_document.h"

#include <QByteArray>
#include <QString>
#include <QStringView>

#include <optional>

namespace niratan::player::detail {

// Unicode White_Space, the union of Foundation's .whitespaces and .newlines
// that Swift trims with, and also Character.isWhitespace used for timing
// tokens.
bool is_whitespace(QChar character);

// Foundation's .whitespaces: Zs plus tab, which is one line trim in the
// WebVTT cleaning step.
bool is_line_whitespace(QChar character);

QString trimmed(QStringView text, bool (*is_space)(QChar));
QString trim_all(QStringView text);

// Strict UTF-8, then UTF-16 when the bytes start with a UTF-16 BOM, then UTF-8
// with U+FFFD for invalid sequences. Every U+FEFF is removed after a
// successful UTF-8 or UTF-16 decode.
QString decode_document(const QByteArray& data);

// CRLF then bare CR become LF.
QString normalize_crlf(QString text);
// normalize_crlf followed by collapsing each run of two or more LF to two.
QString normalize_blocks(QString text);

// Comma to dot, two or three colon fields parsed as doubles, the result
// rounded to milliseconds and validated as finite, non-negative and below
// Int64::max.
std::optional<double> parse_timestamp(QStringView raw);

// Parses CRLF-normalised ASS/SSA text into cues. Returns no_valid_cues when
// the text yields no cue; warnings are always empty.
SubtitleParseResult parse_ass_document(const QString& text, const QUrl& source,
                                       SubtitleFormat format);

}  // namespace niratan::player::detail

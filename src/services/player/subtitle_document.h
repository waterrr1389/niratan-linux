// SPDX-License-Identifier: GPL-3.0-or-later
//
// Subtitle text documents and their cue lookup, following Models/Subtitle.swift
// (SubtitleFormat, SubtitleCue, SubtitleDocument) and
// Features/Video/Subtitles/SubtitleParser.swift (parse, format(for:), decode,
// parseBlock, cleanedWebVTTText, parseTimestamp, timestampMilliseconds,
// SubtitleParserError.unsupportedFormat, .noValidCues) plus
// Features/Video/Subtitles/SubtitleCueStore.swift.
//
// SRT, WebVTT, ASS and SSA text documents are parsed here; the ASS render
// plan stays out of scope, so callers get plain cue text and timing only.
//
// Parsing:
//   - The format is the hint when one is given, otherwise the source URL's
//     extension lowercased: "srt" is SRT, "vtt" is WebVTT, "ass" is ASS and
//     "ssa" is SSA. Any other extension is unsupported_format, including a
//     name without a dot. The source URL is stored as given.
//   - Decoding tries strict UTF-8 first, then UTF-16 when the bytes start with
//     a UTF-16 BOM (FF FE little endian, FE FF big endian), and otherwise
//     decodes as UTF-8 with U+FFFD for invalid sequences. After a successful
//     UTF-8 or UTF-16 decode every U+FEFF is removed, which is what Swift's
//     replacingOccurrences(of: "\u{feff}") does, so a byte-order mark never
//     reaches cue text. BOM-less UTF-16 and other encodings are not
//     recognised; Swift's String(data:encoding:.utf16) may accept some of
//     them, and this port intentionally does not.
//   - CRLF becomes LF, then bare CR becomes LF. SRT and WebVTT then collapse
//     each run of two or more LF to exactly two and split on LF LF; ASS/SSA
//     split the normalised text on LF without collapsing, because Swift
//     dispatches to its ASS parser before the block normalisation.
//   - Per block: leading blank lines are dropped; in WebVTT one leading line
//     starting with "WEBVTT" is dropped; the first line containing "-->" is
//     the timing line, which must split into exactly two "-->" parts. The end
//     timestamp is the first whitespace-delimited token of the second part,
//     so WebVTT settings such as "position:50%" do not affect it. The cue text
//     is every later line joined with LF and trimmed of whitespace and
//     newlines; an empty text, a missing timing line, a malformed timestamp or
//     end < start makes the block invalid.
//   - A timestamp is whitespace-trimmed, commas become dots, and it splits on
//     ":" into minutes:seconds or hours:minutes:seconds (empty fields are
//     dropped, as Swift's split does). Each field parses as a decimal double;
//     the resulting time must be finite and non-negative once rounded to
//     milliseconds, and that rounded value must also be finite, non-negative
//     and below Int64::max.
//   - The cue id is the trimmed line just before the timing line when it is
//     non-empty, otherwise the zero-based block index printed in decimal.
//   - SRT text is trimmed but left otherwise untouched: tags and entities stay
//     literal. WebVTT text loses same-line <...> tags, then "&nbsp;", "&lrm;",
//     "&rlm;", "&lt;", "&gt;", "&quot;", "&apos;" and "&amp;" are replaced in
//     that order; each remaining line is trimmed of whitespace, empty lines
//     are dropped, and the whole is trimmed again.
//   - ASS/SSA: every line is trimmed of whitespace and newlines and empty
//     lines are skipped. A line that starts with "[" and ends with "]" sets
//     the current section to its lowercased text; only lines while the section
//     is exactly "[events]" are read, and a later [Events] header is read
//     again. There an anchored, case-insensitive "Format:" line replaces the
//     dialogue field names only when its comma parts, trimmed and lowercased,
//     contain start, end and text; the defaults are Swift's Layer (Marked for
//     SSA), Start, End, Style, Name, MarginL, MarginR, MarginV, Effect and
//     Text. An anchored, case-insensitive "Dialogue:" line (no space before
//     the colon) has its remainder trimmed and split on commas without
//     trimming: surplus pieces join into the text field and later fields shift
//     by the surplus, while missing pieces are empty. "Comment:" lines are
//     recognised and never produce cues; other lines are ignored.
//   - ASS cue text sanitizes its Text value: a "{" with a later "}" drops that
//     block including the braces, while a "{" without one stays literal.
//     Inside a block, parts split on "\" and trimmed are checked for a leading
//     p or P; a digit run right after it sets the drawing scale (a missing or
//     non-numeric run leaves the scale, and it never goes below 0). At a
//     non-zero scale every character outside override blocks is dropped. Then
//     "\N" and "\n" become a newline, "\h" a space, and the text is trimmed of
//     whitespace and newlines. Empty text produces no cue.
//   - An ASS cue keeps start and end as parsed doubles, needs both timestamps
//     and end >= start, and has the id "ass-" plus the 1-based line number of
//     the Dialogue line in the normalised source. ASS warnings are always
//     empty; a document with no cue is no_valid_cues like the other formats.
//     No render plan, style alignment, effects split or display-time dedupe is
//     produced.
//   - Cues sort by start time, then end time. Swift's sort leaves equal start
//     and end unordered; this port keeps document order for such ties and no
//     caller may rely on it.
//   - A block that fails to parse and is not empty, exactly "WEBVTT" or
//     starting with "NOTE" (case-sensitive) adds "Skipped subtitle block N."
//     with N being the block index plus one. A document with no valid cue is
//     no_valid_cues and yields no document.
//
// The cue store splits the sorted cues into long cues (> 30 s) and timed cues,
// tracking the longest timed duration. cues_at is inclusive at both ends; a
// long cue is returned whenever it covers the time, and a timed cue is found
// by walking back from the first timed cue that starts after the time. slice_at
// adds the cues ending at the greatest end strictly before the time
// (last_shown) and those starting at the least start strictly after it
// (next_to_show). The playback variants subtract the subtitle delay. The delay
// alignment returns playback minus the adjacent cue's start: the last of
// last_shown for previous, the first of next_to_show for next.

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>

namespace niratan::player {

enum class SubtitleFormat { srt, web_vtt, ass, ssa };

struct SubtitleCue {
    QString id;
    double start_time = 0;
    double end_time = 0;
    QString text;
    bool operator==(const SubtitleCue&) const = default;
};

struct SubtitleDocument {
    QUrl source;
    SubtitleFormat format = SubtitleFormat::srt;
    QList<SubtitleCue> cues;
    QStringList warnings;
    bool operator==(const SubtitleDocument&) const = default;
};

enum class SubtitleParseError { none, unsupported_format, no_valid_cues };

struct SubtitleParseResult {
    std::optional<SubtitleDocument> document;
    SubtitleParseError error = SubtitleParseError::none;
    bool operator==(const SubtitleParseResult&) const = default;
};

SubtitleParseResult
parse_subtitle_document(const QByteArray& data, const QUrl& source,
                        std::optional<SubtitleFormat> format_hint = std::nullopt);

struct SubtitleCueSlice {
    QList<SubtitleCue> showing;
    QList<SubtitleCue> last_shown;
    QList<SubtitleCue> next_to_show;
    bool operator==(const SubtitleCueSlice&) const = default;
};

enum class SubtitleOffsetDirection { previous, next };

class SubtitleCueStore {
   public:
    explicit SubtitleCueStore(SubtitleDocument document);

    QList<SubtitleCue> cues_at(double time) const;
    QList<SubtitleCue> cues_at_playback(double playback_time, double subtitle_delay) const;
    SubtitleCueSlice slice_at(double time) const;
    SubtitleCueSlice slice_at_playback(double playback_time, double subtitle_delay) const;
    std::optional<double> delay_aligning_adjacent_cue(double playback_time, double subtitle_delay,
                                                      SubtitleOffsetDirection direction) const;

   private:
    SubtitleDocument document_;
    QList<SubtitleCue> timed_cues_;
    QList<SubtitleCue> long_cues_;
    QList<SubtitleCue> cues_by_start_;
    QList<SubtitleCue> cues_by_end_;
    double max_timed_cue_duration_ = 0;
};

}  // namespace niratan::player

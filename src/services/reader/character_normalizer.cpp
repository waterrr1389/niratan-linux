// SPDX-License-Identifier: GPL-3.0-or-later

#include "character_normalizer.h"

#include <QChar>
#include <QRegularExpression>
#include <QString>

namespace niratan::reader {
namespace {

QString from_utf8(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::string to_utf8(const QString &text) {
    return text.toUtf8().toStdString();
}

// Characters the reader counts, following the explicit ranges in
// Util/ReaderCharacterNormalizer.swift. The Unicode properties are matched by
// range so the result does not depend on the Unicode tables of the regex engine.
bool is_countable(char32_t code_point) {
    return (code_point >= 0x0030 && code_point <= 0x0039)  // 0-9
        || (code_point >= 0x0041 && code_point <= 0x005A)  // A-Z
        || (code_point >= 0x0061 && code_point <= 0x007A)  // a-z
        || code_point == 0x25CB                             // ○
        || code_point == 0x25EF                             // ◯
        || (code_point >= 0x3005 && code_point <= 0x3007)  // 々-〇
        || code_point == 0x303B                             // 〻
        || (code_point >= 0x3041 && code_point <= 0x3096)  // ぁ-ゖ
        || (code_point >= 0x309D && code_point <= 0x309E)  // ゝ-ゞ
        || (code_point >= 0x30A1 && code_point <= 0x30FA)  // ァ-ヺ
        || code_point == 0x30FC                             // ー
        || (code_point >= 0xFF10 && code_point <= 0xFF19)  // ０-９
        || (code_point >= 0xFF21 && code_point <= 0xFF3A)  // Ａ-Ｚ
        || (code_point >= 0xFF41 && code_point <= 0xFF5A)  // ａ-ｚ
        || (code_point >= 0xFF66 && code_point <= 0xFF9D)  // ｦ-ﾝ
        || (code_point >= 0x2E80 && code_point <= 0x2EFF)  // \p{Radical}
        || (code_point >= 0x2F00 && code_point <= 0x2FDF)  // \p{Radical}
        || (code_point >= 0x3400 && code_point <= 0x4DBF)  // \p{Unified_Ideograph}
        || (code_point >= 0x4E00 && code_point <= 0x9FFF)  // \p{Unified_Ideograph}
        || (code_point >= 0xF900 && code_point <= 0xFAFF);  // \p{Unified_Ideograph}
}

QString decoded_references(const QString &text) {
    static const QRegularExpression reference_regex(QStringLiteral(
        "&#(?:([0-9]+)|[xX]([0-9A-Fa-f]+));"));

    QString result;
    result.reserve(text.size());

    qsizetype copied = 0;
    QRegularExpressionMatchIterator matches = reference_regex.globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        result.append(text.mid(copied, match.capturedStart() - copied));
        copied = match.capturedEnd();

        bool valid = false;
        qulonglong value = 0;
        if (match.capturedStart(1) >= 0) {
            value = match.captured(1).toULongLong(&valid, 10);
        } else if (match.capturedStart(2) >= 0) {
            value = match.captured(2).toULongLong(&valid, 16);
        }

        const bool scalar = valid && value <= 0x10FFFF
            && !(value >= 0xD800 && value <= 0xDFFF);
        if (!scalar) {
            continue;
        }

        if (value <= 0xFFFF) {
            result.append(QChar(static_cast<ushort>(value)));
        } else {
            result.append(QChar(QChar::highSurrogate(static_cast<char32_t>(value))));
            result.append(QChar(QChar::lowSurrogate(static_cast<char32_t>(value))));
        }
    }
    result.append(text.mid(copied));

    // Chained replacements in the Swift order, so an escaped reference stays
    // escaped rather than decoding twice.
    result.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    result.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    result.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    result.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    result.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    result.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return result;
}

QString filtered(const QString &markup) {
    static const QRegularExpression body_regex(QStringLiteral("<body.*?</body>"),
                                               QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression rt_regex(QStringLiteral("<rt[^>]*>.*?</rt>"),
                                             QRegularExpression::DotMatchesEverythingOption);
    // Swift uses the same pattern without the case-insensitive option, so an
    // upper-case <SCRIPT> block stays in the count.
    static const QRegularExpression script_style_regex(
        QStringLiteral("<(script|style)[^>]*>.*?</\\1>"),
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression tag_regex(QStringLiteral("<[^>]+>"));

    QString text = markup;
    const QRegularExpressionMatch body = body_regex.match(text);
    if (body.hasMatch()) {
        text = body.captured(0);
    }
    text.remove(rt_regex);
    text.remove(script_style_regex);
    text.remove(tag_regex);
    text = decoded_references(text);

    QString kept;
    kept.reserve(text.size());
    for (qsizetype index = 0; index < text.size();) {
        const QChar current = text.at(index);
        char32_t code_point = current.unicode();
        qsizetype length = 1;
        if (current.isHighSurrogate() && index + 1 < text.size()
            && text.at(index + 1).isLowSurrogate()) {
            code_point = QChar::surrogateToUcs4(current, text.at(index + 1));
            length = 2;
        }
        if (is_countable(code_point)) {
            kept.append(current);
            if (length == 2) {
                kept.append(text.at(index + 1));
            }
        }
        index += length;
    }
    return kept;
}

}  // namespace

std::string filtered_text(std::string_view markup) {
    return to_utf8(filtered(from_utf8(markup)));
}

int readable_character_count(std::string_view markup) {
    return static_cast<int>(filtered(from_utf8(markup)).toUcs4().size());
}

std::string decoded_character_references(std::string_view text) {
    return to_utf8(decoded_references(from_utf8(text)));
}

}  // namespace niratan::reader

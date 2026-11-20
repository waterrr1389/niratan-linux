// SPDX-License-Identifier: GPL-3.0-or-later

#include "content_language.h"

#include <QChar>
#include <QString>
#include <QStringView>

#include <algorithm>
#include <optional>
#include <string_view>

namespace niratan::profiles {

std::string_view language_id(ContentLanguage language) {
    switch (language) {
        case ContentLanguage::japanese:
            return "ja";
        case ContentLanguage::english:
            return "en";
    }
    return {};
}

std::optional<ContentLanguage> normalize_language(std::string_view language) {
    // Invalid UTF-8 decodes to U+FFFD, which no tag below matches, so the
    // result stays nullopt instead of being rejected explicitly.
    const QString tag = QString::fromUtf8(language.data(), static_cast<qsizetype>(language.size()))
                            .trimmed()
                            .toLower();

    qsizetype begin = 0;
    while (begin < tag.size() && (tag.at(begin) == u'-' || tag.at(begin) == u'_')) {
        ++begin;
    }
    if (begin == tag.size()) {
        return std::nullopt;
    }

    qsizetype end = begin;
    while (end < tag.size() && tag.at(end) != u'-' && tag.at(end) != u'_') {
        ++end;
    }

    const QStringView primary = QStringView(tag).mid(begin, end - begin);
    if (primary == QStringView(u"ja")) {
        return ContentLanguage::japanese;
    }
    if (primary == QStringView(u"en")) {
        return ContentLanguage::english;
    }
    return std::nullopt;
}

int display_count(ContentLanguage language, int raw_characters) {
    const int count = std::max(0, raw_characters);
    switch (language) {
        case ContentLanguage::japanese:
            return count;
        case ContentLanguage::english: {
            // Ceiling division without the intermediate overflow of count + 4.
            const int whole = count / 5;
            return count % 5 == 0 ? whole : whole + 1;
        }
    }
    return 0;
}

int raw_characters(ContentLanguage language, int display_count) {
    const int count = std::max(0, display_count);
    switch (language) {
        case ContentLanguage::japanese:
            return count;
        case ContentLanguage::english:
            return count * 5;
    }
    return 0;
}

}  // namespace niratan::profiles

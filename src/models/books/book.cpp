// SPDX-License-Identifier: GPL-3.0-or-later

#include "book.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace niratan::books {
namespace {

QString text(const std::string& value) {
    return QString::fromStdString(value);
}

// Synthesized Codable decodes an optional property from an absent key and from
// a null alike, and rejects a value of any other type. The helpers return false
// only for the last case, and leave the field cleared for the first.
bool optional_string(const QJsonObject& object, QLatin1StringView key,
                     std::optional<std::string>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    if (!value.isString()) {
        return false;
    }
    *out = value.toString().toStdString();
    return true;
}

bool required_string(const QJsonObject& object, QLatin1StringView key, std::string* out) {
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return false;
    }
    *out = value.toString().toStdString();
    return true;
}

// An integer property decodes only from a number without a fractional part that
// fits the property's range; anything else fails instead of saturating.
std::optional<int> integer_value(const QJsonValue& value) {
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    constexpr double kUpperBound = 2147483648.0;  // 2^31, one past INT_MAX
    if (!std::isfinite(number) || std::trunc(number) != number || number < -kUpperBound ||
        number >= kUpperBound) {
        return std::nullopt;
    }
    return static_cast<int>(number);
}

std::optional<std::int64_t> integer64_value(const QJsonValue& value) {
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    constexpr double kUpperBound = 9223372036854775808.0;  // 2^63, one past INT64_MAX
    if (!std::isfinite(number) || std::trunc(number) != number || number < -kUpperBound ||
        number >= kUpperBound) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(number);
}

bool required_integer(const QJsonObject& object, QLatin1StringView key, int* out) {
    const std::optional<int> number = integer_value(object.value(key));
    if (!number) {
        return false;
    }
    *out = *number;
    return true;
}

bool optional_integer(const QJsonObject& object, QLatin1StringView key,
                      std::optional<int>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    const std::optional<int> number = integer_value(value);
    if (!number) {
        return false;
    }
    *out = number;
    return true;
}

bool optional_integer64(const QJsonObject& object, QLatin1StringView key,
                        std::optional<std::int64_t>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    const std::optional<std::int64_t> number = integer64_value(value);
    if (!number) {
        return false;
    }
    *out = number;
    return true;
}

bool required_number(const QJsonObject& object, QLatin1StringView key, double* out) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }
    *out = value.toDouble();
    return true;
}

bool optional_number(const QJsonObject& object, QLatin1StringView key,
                     std::optional<double>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    *out = value.toDouble();
    return true;
}

// A dictionary property decodes from a JSON object whose every value is a
// number, and rejects the document when any value is of another type.
bool optional_int_map(const QJsonObject& object, QLatin1StringView key,
                      std::optional<std::map<std::string, int>>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    if (!value.isObject()) {
        return false;
    }

    std::map<std::string, int> map;
    const QJsonObject source = value.toObject();
    for (auto entry = source.begin(); entry != source.end(); ++entry) {
        const std::optional<int> number = integer_value(entry.value());
        if (!number) {
            return false;
        }
        map.emplace(entry.key().toStdString(), *number);
    }
    *out = std::move(map);
    return true;
}

bool optional_images(const QJsonObject& object, QLatin1StringView key,
                     std::optional<std::vector<std::string>>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    if (!value.isArray()) {
        return false;
    }

    std::vector<std::string> images;
    const QJsonArray array = value.toArray();
    for (const QJsonValue& entry : array) {
        if (!entry.isString()) {
            return false;
        }
        images.push_back(entry.toString().toStdString());
    }
    *out = std::move(images);
    return true;
}

void insert_optional_string(QJsonObject& object, QLatin1StringView key,
                            const std::optional<std::string>& value) {
    if (!value) {
        return;
    }
    object.insert(key, text(*value));
}

void insert_optional_integer(QJsonObject& object, QLatin1StringView key,
                             const std::optional<int>& value) {
    if (!value) {
        return;
    }
    object.insert(key, *value);
}

void insert_optional_integer64(QJsonObject& object, QLatin1StringView key,
                               const std::optional<std::int64_t>& value) {
    if (!value) {
        return;
    }
    object.insert(key, static_cast<qint64>(*value));
}

void insert_optional_number(QJsonObject& object, QLatin1StringView key,
                            const std::optional<double>& value) {
    if (!value) {
        return;
    }
    object.insert(key, *value);
}

void insert_int_map(QJsonObject& object, QLatin1StringView key,
                    const std::optional<std::map<std::string, int>>& value) {
    if (!value) {
        return;
    }

    QJsonObject map;
    for (const auto& [name, number] : *value) {
        map.insert(text(name), number);
    }
    object.insert(key, map);
}

void insert_optional_images(QJsonObject& object, QLatin1StringView key,
                            const std::optional<std::vector<std::string>>& value) {
    if (!value) {
        return;
    }

    QJsonArray array;
    for (const std::string& image : *value) {
        array.append(text(image));
    }
    object.insert(key, array);
}

// QUuid::fromString reports a malformed string and the all-zero identifier as
// the same null value, while Foundation parses the latter. Text spelling the
// null UUID is recognized here; braces and hyphens are formatting only.
bool is_spelled_null_uuid(std::string_view text) {
    std::size_t digits = 0;
    for (const char character : text) {
        if (character == '0') {
            ++digits;
            continue;
        }
        if (character != '{' && character != '}' && character != '-') {
            return false;
        }
    }
    return digits == 32;
}

std::optional<ChapterInfo> chapter_info_from_json(const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();

    ChapterInfo chapter;
    if (!required_integer(object, QLatin1StringView("currentTotal"), &chapter.current_total) ||
        !required_integer(object, QLatin1StringView("chapterCount"), &chapter.chapter_count) ||
        !optional_integer(object, QLatin1StringView("spineIndex"), &chapter.spine_index) ||
        !optional_int_map(object, QLatin1StringView("fragmentOffsets"),
                          &chapter.fragment_offsets)) {
        return std::nullopt;
    }
    return chapter;
}

QJsonObject chapter_info_to_json(const ChapterInfo& chapter) {
    QJsonObject object;
    insert_optional_integer(object, QLatin1StringView("spineIndex"), chapter.spine_index);
    object.insert(QLatin1StringView("currentTotal"), chapter.current_total);
    object.insert(QLatin1StringView("chapterCount"), chapter.chapter_count);
    insert_int_map(object, QLatin1StringView("fragmentOffsets"), chapter.fragment_offsets);
    return object;
}

}  // namespace

std::optional<int> reorder_destination(int source_index, int target_index) {
    if (source_index == target_index) {
        return std::nullopt;
    }
    return target_index > source_index ? target_index + 1 : target_index;
}

std::string reorder_payload(const std::string& book_id) {
    return "hoshi-book:" + book_id;
}

std::optional<std::string> reorder_book_id(std::string_view payload) {
    constexpr std::string_view kPrefix = "hoshi-book:";
    if (!payload.starts_with(kPrefix)) {
        return std::nullopt;
    }

    const std::string_view identifier = payload.substr(kPrefix.size());
    if (identifier.empty()) {
        return std::nullopt;
    }
    const QString identifier_text =
        QString::fromUtf8(identifier.data(), static_cast<qsizetype>(identifier.size()));
    // QUuid::fromString accepts the brace and hyphen forms Foundation does, but
    // reports a malformed string and the all-zero identifier as the same null
    // value; Foundation parses the latter, so text spelling it is kept.
    const QUuid parsed = QUuid::fromString(identifier_text);
    if (parsed.isNull() && !is_spelled_null_uuid(identifier)) {
        return std::nullopt;
    }
    return std::string(identifier);
}

std::string BookMetadata::display_title() const {
    return renamed_title.value_or(title);
}

bool is_highlight_color(std::string_view color) {
    return color == "yellow" || color == "green" || color == "blue" || color == "pink" ||
           color == "purple";
}

QJsonDocument highlights_to_json(const std::vector<Highlight>& highlights) {
    QJsonArray result;
    for (const auto& entry : highlights) {
        result.append(QJsonObject{
            {QStringLiteral("id"), QString::fromStdString(entry.id)},
            {QStringLiteral("character"), entry.character},
            {QStringLiteral("offset"), entry.offset},
            {QStringLiteral("text"), QString::fromStdString(entry.text)},
            {QStringLiteral("color"), QString::fromStdString(entry.color)},
            {QStringLiteral("createdAt"), entry.created_at},
        });
    }
    return QJsonDocument(result);
}

std::optional<std::vector<Highlight>> highlights_from_json(const QJsonDocument& document) {
    if (!document.isArray()) {
        return std::nullopt;
    }
    std::vector<Highlight> result;
    for (const auto& value : document.array()) {
        if (!value.isObject()) {
            return std::nullopt;
        }
        const auto object = value.toObject();
        Highlight entry{};
        if (!required_string(object, QLatin1StringView("id"), &entry.id) || entry.id.empty() ||
            !required_string(object, QLatin1StringView("text"), &entry.text) ||
            !required_string(object, QLatin1StringView("color"), &entry.color) ||
            !is_highlight_color(entry.color) ||
            !required_number(object, QLatin1StringView("createdAt"), &entry.created_at)) {
            return std::nullopt;
        }
        entry.character = object.value(QStringLiteral("character")).toInt(-1);
        entry.offset = object.value(QStringLiteral("offset")).toInt(-1);
        if (entry.character < 0 || entry.offset < 0) {
            return std::nullopt;
        }
        result.push_back(std::move(entry));
    }
    return result;
}

std::vector<Highlight> highlights_in_range(const std::vector<Highlight>& highlights, int start,
                                           int end) {
    std::vector<Highlight> result;
    for (const auto& entry : highlights) {
        if (entry.character >= start && entry.character < end) {
            result.push_back(entry);
        }
    }
    std::ranges::stable_sort(result, {}, &Highlight::character);
    return result;
}

std::optional<std::pair<int, double>> BookInfo::resolve_character_position(int characters) const {
    if (character_count <= 0) {
        return std::nullopt;
    }

    const int clamped = std::max(0, std::min(characters, character_count - 1));
    const ChapterInfo* match = nullptr;
    for (const auto& entry : chapter_info) {
        const ChapterInfo& chapter = entry.second;
        if (!chapter.spine_index || chapter.chapter_count <= 0) {
            continue;
        }
        // Widened so that a large currentTotal plus chapterCount cannot wrap.
        const auto start = static_cast<std::int64_t>(chapter.current_total);
        const auto end = start + static_cast<std::int64_t>(chapter.chapter_count);
        if (clamped < start || clamped >= end) {
            continue;
        }
        // Swift iterates a dictionary, so overlapping ranges resolve
        // arbitrarily; the lowest start wins here for a stable result.
        if (match == nullptr || chapter.current_total < match->current_total) {
            match = &chapter;
        }
    }
    if (match == nullptr) {
        return std::nullopt;
    }

    return std::make_pair(
        *match->spine_index,
        static_cast<double>(clamped - match->current_total) /
            static_cast<double>(match->chapter_count));
}

std::optional<BookMetadata> book_metadata_from_json(const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();

    BookMetadata book;
    if (!required_string(object, QLatin1StringView("id"), &book.id) ||
        !required_string(object, QLatin1StringView("title"), &book.title) ||
        !required_string(object, QLatin1StringView("folder"), &book.folder) ||
        !required_number(object, QLatin1StringView("lastAccess"), &book.last_access) ||
        !optional_string(object, QLatin1StringView("epub"), &book.epub) ||
        !optional_string(object, QLatin1StringView("cover"), &book.cover) ||
        !optional_string(object, QLatin1StringView("renamedTitle"), &book.renamed_title) ||
        !optional_string(object, QLatin1StringView("profileId"), &book.profile_id) ||
        !optional_string(object, QLatin1StringView("bookLanguage"), &book.book_language) ||
        !optional_string(object, QLatin1StringView("externalSourceID"),
                         &book.external_source_id) ||
        !optional_string(object, QLatin1StringView("externalISBN"), &book.external_isbn) ||
        !optional_string(object, QLatin1StringView("author"), &book.author) ||
        !optional_integer(object, QLatin1StringView("characterCount"), &book.character_count) ||
        !optional_integer64(object, QLatin1StringView("modified"), &book.modified)) {
        return std::nullopt;
    }
    return book;
}

QJsonDocument book_metadata_to_json(const BookMetadata& book) {
    QJsonObject object;
    object.insert(QLatin1StringView("id"), text(book.id));
    object.insert(QLatin1StringView("title"), text(book.title));
    insert_optional_string(object, QLatin1StringView("epub"), book.epub);
    insert_optional_string(object, QLatin1StringView("cover"), book.cover);
    object.insert(QLatin1StringView("folder"), text(book.folder));
    object.insert(QLatin1StringView("lastAccess"), book.last_access);
    insert_optional_string(object, QLatin1StringView("renamedTitle"), book.renamed_title);
    insert_optional_string(object, QLatin1StringView("profileId"), book.profile_id);
    insert_optional_string(object, QLatin1StringView("bookLanguage"), book.book_language);
    insert_optional_string(object, QLatin1StringView("externalSourceID"),
                           book.external_source_id);
    insert_optional_string(object, QLatin1StringView("externalISBN"), book.external_isbn);
    insert_optional_string(object, QLatin1StringView("author"), book.author);
    insert_optional_integer(object, QLatin1StringView("characterCount"), book.character_count);
    insert_optional_integer64(object, QLatin1StringView("modified"), book.modified);
    return QJsonDocument(object);
}

std::optional<Bookmark> bookmark_from_json(const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();

    Bookmark bookmark;
    if (!required_integer(object, QLatin1StringView("chapterIndex"), &bookmark.chapter_index) ||
        !required_number(object, QLatin1StringView("progress"), &bookmark.progress) ||
        !required_integer(object, QLatin1StringView("characterCount"),
                          &bookmark.character_count) ||
        !optional_number(object, QLatin1StringView("lastModified"), &bookmark.last_modified)) {
        return std::nullopt;
    }
    return bookmark;
}

QJsonDocument bookmark_to_json(const Bookmark& bookmark) {
    QJsonObject object;
    object.insert(QLatin1StringView("chapterIndex"), bookmark.chapter_index);
    object.insert(QLatin1StringView("progress"), bookmark.progress);
    object.insert(QLatin1StringView("characterCount"), bookmark.character_count);
    insert_optional_number(object, QLatin1StringView("lastModified"), bookmark.last_modified);
    return QJsonDocument(object);
}

std::optional<BookInfo> book_info_from_json(const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();

    BookInfo info;
    if (!required_integer(object, QLatin1StringView("characterCount"), &info.character_count)) {
        return std::nullopt;
    }

    const QJsonValue chapters = object.value(QLatin1StringView("chapterInfo"));
    if (!chapters.isObject()) {
        return std::nullopt;
    }
    const QJsonObject chapter_object = chapters.toObject();
    for (auto entry = chapter_object.begin(); entry != chapter_object.end(); ++entry) {
        std::optional<ChapterInfo> chapter = chapter_info_from_json(entry.value());
        if (!chapter) {
            return std::nullopt;
        }
        info.chapter_info.emplace(entry.key().toStdString(), std::move(*chapter));
    }

    if (!optional_images(object, QLatin1StringView("images"), &info.images) ||
        !optional_int_map(object, QLatin1StringView("imagePositions"), &info.image_positions)) {
        return std::nullopt;
    }
    return info;
}

QJsonDocument book_info_to_json(const BookInfo& info) {
    QJsonObject chapters;
    for (const auto& [path, chapter] : info.chapter_info) {
        chapters.insert(text(path), chapter_info_to_json(chapter));
    }

    QJsonObject object;
    object.insert(QLatin1StringView("characterCount"), info.character_count);
    object.insert(QLatin1StringView("chapterInfo"), chapters);
    insert_optional_images(object, QLatin1StringView("images"), info.images);
    insert_int_map(object, QLatin1StringView("imagePositions"), info.image_positions);
    return QJsonDocument(object);
}

std::optional<std::vector<BookShelf>> shelves_from_json(const QJsonDocument& document) {
    if (!document.isArray()) {
        return std::nullopt;
    }

    std::vector<BookShelf> shelves;
    const QJsonArray array = document.array();
    for (const QJsonValue& entry : array) {
        if (!entry.isObject()) {
            return std::nullopt;
        }
        const QJsonObject object = entry.toObject();

        BookShelf shelf;
        if (!required_string(object, QLatin1StringView("name"), &shelf.name)) {
            return std::nullopt;
        }
        const QJsonValue ids = object.value(QLatin1StringView("bookIds"));
        if (!ids.isArray()) {
            return std::nullopt;
        }
        const QJsonArray id_array = ids.toArray();
        for (const QJsonValue& id : id_array) {
            if (!id.isString()) {
                return std::nullopt;
            }
            shelf.book_ids.push_back(id.toString().toStdString());
        }
        shelves.push_back(std::move(shelf));
    }
    return shelves;
}

QJsonDocument shelves_to_json(const std::vector<BookShelf>& shelves) {
    QJsonArray array;
    for (const BookShelf& shelf : shelves) {
        QJsonArray ids;
        for (const std::string& id : shelf.book_ids) {
            ids.append(text(id));
        }

        QJsonObject object;
        object.insert(QLatin1StringView("name"), text(shelf.name));
        object.insert(QLatin1StringView("bookIds"), ids);
        array.append(object);
    }
    return QJsonDocument(array);
}

std::optional<std::vector<std::string>> book_order_from_json(const QJsonDocument& document) {
    if (!document.isArray()) {
        return std::nullopt;
    }

    std::vector<std::string> order;
    const QJsonArray array = document.array();
    for (const QJsonValue& entry : array) {
        if (!entry.isString()) {
            return std::nullopt;
        }
        order.push_back(entry.toString().toStdString());
    }
    return order;
}

QJsonDocument book_order_to_json(const std::vector<std::string>& order) {
    QJsonArray array;
    for (const std::string& id : order) {
        array.append(text(id));
    }
    return QJsonDocument(array);
}

bool is_in_progress(double progress) {
    return progress > 0.0 && progress < 0.999;
}

}  // namespace niratan::books

// SPDX-License-Identifier: GPL-3.0-or-later

#include "audiobook_metadata.h"

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace niratan::audiobook {
namespace {

using niratan::sasayaki::SasayakiAudiobookChapter;
using niratan::sasayaki::SasayakiAudiobookMetadata;

// MP4MetadataReader.maxMetadataDataBytes: larger artwork is treated as absent
// rather than loaded.
constexpr quint64 kMaximumMetadataDataBytes = 20ULL * 1024ULL * 1024ULL;

// QFile wrapper with SequentialAccess semantics: every read is exact, and a
// short read is remembered as a failure of the whole parse, which is how the
// Swift reader's throw propagates out of parse().
class Mp4File {
   public:
    explicit Mp4File(const QString& path) : file_(path) {
        if (file_.open(QIODevice::ReadOnly)) {
            size_ = static_cast<quint64>(file_.size());
        }
    }

    bool is_open() const { return file_.isOpen(); }
    quint64 size() const { return size_; }
    bool failed() const { return failed_; }

    std::optional<QByteArray> read(quint64 offset, quint64 count) const {
        if (!file_.isOpen() || !file_.seek(static_cast<qint64>(offset))) {
            failed_ = true;
            return std::nullopt;
        }
        const QByteArray data = file_.read(static_cast<qint64>(count));
        if (static_cast<quint64>(data.size()) != count) {
            failed_ = true;
            return std::nullopt;
        }
        return data;
    }

    std::optional<quint8> read_u8(quint64 offset) const {
        const std::optional<QByteArray> data = read(offset, 1);
        if (!data.has_value()) {
            return std::nullopt;
        }
        return static_cast<quint8>((*data)[0]);
    }

    std::optional<quint32> read_u32(quint64 offset) const {
        const std::optional<QByteArray> data = read(offset, 4);
        if (!data.has_value()) {
            return std::nullopt;
        }
        quint32 value = 0;
        for (const char byte : *data) {
            value = static_cast<quint32>((value << 8) | static_cast<quint8>(byte));
        }
        return value;
    }

    std::optional<quint64> read_u64(quint64 offset) const {
        const std::optional<QByteArray> data = read(offset, 8);
        if (!data.has_value()) {
            return std::nullopt;
        }
        quint64 value = 0;
        for (const char byte : *data) {
            value = (value << 8) | static_cast<quint8>(byte);
        }
        return value;
    }

   private:
    mutable QFile file_;
    quint64 size_ = 0;
    mutable bool failed_ = false;
};

struct Mp4Box {
    QByteArray type;
    quint64 start = 0;
    quint64 header_size = 0;
    quint64 size = 0;

    quint64 content_start() const { return start + header_size; }
    quint64 end() const { return start + size; }
};

struct RawChapter {
    std::string title;
    double start_time = 0.0;
};

// MP4MetadataReader.readBox: size 0 runs to the parent end, size 1 reads the
// 64-bit largesize after the type, any other value is the size. A size below
// the header or past the parent end is malformed and is not a read failure.
std::optional<Mp4Box> read_box(const Mp4File& file, quint64 offset, quint64 parent_end) {
    if (offset + 8 > parent_end) {
        return std::nullopt;
    }
    const std::optional<quint32> short_size = file.read_u32(offset);
    if (!short_size.has_value()) {
        return std::nullopt;
    }
    const std::optional<QByteArray> type = file.read(offset + 4, 4);
    if (!type.has_value()) {
        return std::nullopt;
    }

    quint64 header_size = 8;
    quint64 size = 0;
    switch (*short_size) {
        case 0:
            size = parent_end - offset;
            break;
        case 1: {
            const std::optional<quint64> large_size = file.read_u64(offset + 8);
            if (!large_size.has_value()) {
                return std::nullopt;
            }
            header_size = 16;
            size = *large_size;
            break;
        }
        default:
            size = *short_size;
            break;
    }
    if (size < header_size || size > parent_end - offset) {
        return std::nullopt;
    }
    return Mp4Box{*type, offset, header_size, size};
}

std::optional<Mp4Box> child_box(const Mp4File& file, quint64 start, quint64 end,
                                const QByteArray& type) {
    quint64 offset = start;
    while (offset + 8 <= end) {
        const std::optional<Mp4Box> box = read_box(file, offset, end);
        if (!box.has_value()) {
            return std::nullopt;
        }
        if (box->type == type) {
            return box;
        }
        if (box->end() <= offset) {
            return std::nullopt;
        }
        offset = box->end();
    }
    return std::nullopt;
}

std::optional<Mp4Box> child_box(const Mp4File& file, const Mp4Box& parent, const QByteArray& type) {
    return child_box(file, parent.content_start(), parent.end(), type);
}

std::vector<Mp4Box> child_boxes(const Mp4File& file, quint64 start, quint64 end) {
    std::vector<Mp4Box> boxes;
    quint64 offset = start;
    while (offset + 8 <= end) {
        const std::optional<Mp4Box> box = read_box(file, offset, end);
        if (!box.has_value()) {
            break;
        }
        boxes.push_back(*box);
        if (box->end() <= offset) {
            break;
        }
        offset = box->end();
    }
    return boxes;
}

// MP4MetadataReader.normalizedText and SasayakiAudiobookMetadataLoader.normalized:
// decode UTF-8 with replacement and trim Foundation's whitespace and newlines,
// which is the same set QString::trimmed removes.
std::optional<std::string> normalized_text(const QByteArray& data) {
    const QString value = QString::fromUtf8(data).trimmed();
    if (value.isEmpty()) {
        return std::nullopt;
    }
    return value.toStdString();
}

// MP4MetadataReader.metadataData: the item's first data child only, after its
// 8-byte version and flags, with a payload in 1..20 MiB.
std::optional<QByteArray> metadata_data(const Mp4File& file, const Mp4Box& item) {
    const std::optional<Mp4Box> data_box = child_box(file, item, QByteArrayLiteral("data"));
    if (!data_box.has_value() || data_box->content_start() + 8 > data_box->end()) {
        return std::nullopt;
    }
    const quint64 byte_count = data_box->end() - data_box->content_start() - 8;
    if (byte_count == 0 || byte_count > kMaximumMetadataDataBytes) {
        return std::nullopt;
    }
    return file.read(data_box->content_start() + 8, byte_count);
}

SasayakiAudiobookMetadata read_metadata(const Mp4File& file) {
    const std::optional<Mp4Box> moov = child_box(file, 0, file.size(), QByteArrayLiteral("moov"));
    if (!moov.has_value()) {
        return {};
    }

    std::optional<Mp4Box> meta;
    const std::optional<Mp4Box> udta = child_box(file, *moov, QByteArrayLiteral("udta"));
    if (udta.has_value()) {
        meta = child_box(file, *udta, QByteArrayLiteral("meta"));
    }
    if (!meta.has_value()) {
        meta = child_box(file, *moov, QByteArrayLiteral("meta"));
    }
    if (!meta.has_value() || meta->content_start() + 4 > meta->end()) {
        return {};
    }
    const std::optional<Mp4Box> item_list =
        child_box(file, meta->content_start() + 4, meta->end(), QByteArrayLiteral("ilst"));
    if (!item_list.has_value()) {
        return {};
    }

    const QByteArray title_key = QByteArray("\xA9"
                                            "nam",
                                            4);
    const QByteArray artist_key = QByteArray("\xA9"
                                             "ART",
                                             4);
    std::optional<std::string> title;
    std::optional<std::string> artist;
    std::optional<std::string> album_artist;
    std::optional<QByteArray> artwork;
    for (const Mp4Box& item : child_boxes(file, item_list->content_start(), item_list->end())) {
        const std::optional<QByteArray> data = metadata_data(file, item);
        if (!data.has_value()) {
            continue;
        }
        if (item.type == title_key) {
            if (!title.has_value()) {
                title = normalized_text(*data);
            }
        } else if (item.type == artist_key) {
            if (!artist.has_value()) {
                artist = normalized_text(*data);
            }
        } else if (item.type == QByteArrayLiteral("aART")) {
            if (!album_artist.has_value()) {
                album_artist = normalized_text(*data);
            }
        } else if (item.type == QByteArrayLiteral("covr")) {
            if (!artwork.has_value()) {
                artwork = *data;
            }
        }
    }

    SasayakiAudiobookMetadata metadata;
    metadata.title = std::move(title);
    metadata.artist = artist.has_value() ? std::move(artist) : std::move(album_artist);
    metadata.artwork_data = std::move(artwork);
    return metadata;
}

// MP4ChapterReader.seconds: no duration without a positive timescale.
std::optional<double> seconds(quint64 duration, quint64 timescale) {
    if (timescale == 0) {
        return std::nullopt;
    }
    return static_cast<double>(duration) / static_cast<double>(timescale);
}

// MP4ChapterReader.readMovieDuration. The fixed offsets are read straight from
// the file handle, not bounded by the mvhd box, so only a read past the end of
// the file fails the parse; a box that is short but not last takes the bytes
// that follow it.
std::optional<double> read_movie_duration(const Mp4File& file, const Mp4Box& box) {
    const std::optional<quint8> version = file.read_u8(box.content_start());
    if (!version.has_value()) {
        return std::nullopt;
    }
    switch (*version) {
        case 0: {
            const std::optional<quint32> timescale = file.read_u32(box.content_start() + 12);
            if (!timescale.has_value()) {
                return std::nullopt;
            }
            const std::optional<quint32> duration = file.read_u32(box.content_start() + 16);
            if (!duration.has_value()) {
                return std::nullopt;
            }
            return seconds(*duration, *timescale);
        }
        case 1: {
            const std::optional<quint32> timescale = file.read_u32(box.content_start() + 20);
            if (!timescale.has_value()) {
                return std::nullopt;
            }
            const std::optional<quint64> duration = file.read_u64(box.content_start() + 24);
            if (!duration.has_value()) {
                return std::nullopt;
            }
            return seconds(*duration, *timescale);
        }
        default:
            return std::nullopt;
    }
}

// MP4ChapterReader.readChapterList: a count byte, then 100 ns starts with a
// one-byte title length. A truncated entry ends the loop and keeps the chapters
// before it.
std::vector<RawChapter> read_chapter_list(const Mp4File& file, const Mp4Box& box) {
    std::vector<RawChapter> chapters;
    if (box.content_start() + 9 > box.end()) {
        return chapters;
    }
    const std::optional<quint8> count = file.read_u8(box.content_start() + 8);
    if (!count.has_value()) {
        return chapters;
    }
    chapters.reserve(static_cast<std::size_t>(*count));
    quint64 offset = box.content_start() + 9;
    for (quint8 index = 0; index < *count; ++index) {
        if (offset + 9 > box.end()) {
            break;
        }
        const std::optional<quint64> start = file.read_u64(offset);
        if (!start.has_value()) {
            return chapters;
        }
        offset += 8;
        const std::optional<quint8> title_length = file.read_u8(offset);
        if (!title_length.has_value()) {
            return chapters;
        }
        offset += 1;
        if (offset + *title_length > box.end()) {
            break;
        }
        const std::optional<QByteArray> title_data = file.read(offset, *title_length);
        if (!title_data.has_value()) {
            return chapters;
        }
        offset += *title_length;
        const QString title = QString::fromUtf8(*title_data).trimmed();
        chapters.push_back(
            RawChapter{title.toStdString(), static_cast<double>(*start) / 10000000.0});
    }
    return chapters;
}

std::vector<SasayakiAudiobookChapter> read_chapters(const Mp4File& file) {
    const std::optional<Mp4Box> moov = child_box(file, 0, file.size(), QByteArrayLiteral("moov"));
    if (!moov.has_value()) {
        return {};
    }

    std::optional<double> movie_duration;
    const std::optional<Mp4Box> movie_header = child_box(file, *moov, QByteArrayLiteral("mvhd"));
    if (movie_header.has_value()) {
        movie_duration = read_movie_duration(file, *movie_header);
        if (file.failed()) {
            return {};
        }
    }

    std::optional<Mp4Box> chapter_box;
    const std::optional<Mp4Box> udta = child_box(file, *moov, QByteArrayLiteral("udta"));
    if (udta.has_value()) {
        chapter_box = child_box(file, *udta, QByteArrayLiteral("chpl"));
    }
    if (!chapter_box.has_value()) {
        chapter_box = child_box(file, *moov, QByteArrayLiteral("chpl"));
    }
    if (!chapter_box.has_value()) {
        return {};
    }

    std::vector<RawChapter> raw = read_chapter_list(file, *chapter_box);
    if (file.failed()) {
        return {};
    }
    // Swift's sorted result is unspecified for equal starts; the port keeps
    // the input order so the post-sort ids are stable.
    std::stable_sort(raw.begin(), raw.end(), [](const RawChapter& left, const RawChapter& right) {
        return left.start_time < right.start_time;
    });

    std::vector<SasayakiAudiobookChapter> chapters;
    chapters.reserve(raw.size());
    for (std::size_t index = 0; index < raw.size(); ++index) {
        SasayakiAudiobookChapter chapter;
        chapter.id = static_cast<int>(index);
        chapter.title =
            raw[index].title.empty() ? "Chapter " + std::to_string(index + 1) : raw[index].title;
        chapter.start_time = raw[index].start_time;
        if (index + 1 < raw.size()) {
            chapter.end_time = raw[index + 1].start_time;
        } else if (movie_duration.has_value() && *movie_duration >= raw[index].start_time) {
            chapter.end_time = *movie_duration;
        }
        chapters.push_back(std::move(chapter));
    }
    return chapters;
}

}  // namespace

niratan::sasayaki::SasayakiAudiobookMetadata read_audiobook_metadata(const QString& path) {
    Mp4File file(path);
    if (!file.is_open()) {
        return {};
    }
    SasayakiAudiobookMetadata metadata = read_metadata(file);
    if (file.failed()) {
        return {};
    }
    return metadata;
}

std::vector<niratan::sasayaki::SasayakiAudiobookChapter>
read_audiobook_chapters(const QString& path) {
    Mp4File file(path);
    if (!file.is_open()) {
        return {};
    }
    std::vector<SasayakiAudiobookChapter> chapters = read_chapters(file);
    if (file.failed()) {
        return {};
    }
    return chapters;
}

std::optional<int> current_audiobook_chapter_id(
    const std::vector<niratan::sasayaki::SasayakiAudiobookChapter>& chapters, double current_time) {
    if (chapters.empty()) {
        return std::nullopt;
    }
    const int last_id = chapters.back().id;
    for (auto it = chapters.rbegin(); it != chapters.rend(); ++it) {
        if (!(current_time >= it->start_time)) {
            continue;
        }
        if (it->end_time.has_value()) {
            const bool admits =
                it->id == last_id ? current_time <= *it->end_time : current_time < *it->end_time;
            if (!admits) {
                continue;
            }
        }
        return it->id;
    }
    return std::nullopt;
}

}  // namespace niratan::audiobook

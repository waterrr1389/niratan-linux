// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Sasayaki/SasayakiAudiobookMetadata.swift (MP4MetadataReader,
// MP4ChapterReader, the two parsers) and Models/Sasayaki.swift
// (SasayakiAudiobookChapter, SasayakiAudiobookMetadata) are the specification.
// The fixture of script/test_sasayaki_audiobook_metadata.swift is reproduced
// byte for byte in fixture_metadata() and fixture_chapters(); every other case
// names the Swift branch it exercises in its message.

#include <QByteArray>
#include <QChar>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QTest>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "models/sasayaki/sasayaki.h"
#include "services/audiobook/audiobook_metadata.h"

using niratan::audiobook::current_audiobook_chapter_id;
using niratan::audiobook::read_audiobook_chapters;
using niratan::audiobook::read_audiobook_metadata;
using niratan::sasayaki::SasayakiAudiobookChapter;
using niratan::sasayaki::SasayakiAudiobookMetadata;

namespace {

QByteArray be32(quint32 value) {
    QByteArray bytes(4, '\0');
    qToBigEndian(value, bytes.data());
    return bytes;
}

QByteArray be64(quint64 value) {
    QByteArray bytes(8, '\0');
    qToBigEndian(value, bytes.data());
    return bytes;
}

QByteArray box(const QByteArray& type, const QByteArray& payload) {
    return be32(static_cast<quint32>(payload.size()) + 8U) + type + payload;
}

// A box whose 32-bit size is 0: it runs to the end of its parent.
QByteArray open_ended_box(const QByteArray& type, const QByteArray& payload) {
    return be32(0) + type + payload;
}

// A box with the 64-bit largesize form (32-bit size 1, then the full size).
QByteArray large_box(const QByteArray& type, const QByteArray& payload) {
    return be32(1) + type + be64(static_cast<quint64>(payload.size()) + 16U) + payload;
}

// The raw four box-type bytes of ©nam and ©ART: isoLatin1 A9 then ASCII.
QByteArray type_name() {
    return QByteArray::fromHex("a96e616d");
}

QByteArray type_artist() {
    return QByteArray::fromHex("a9415254");
}

QByteArray data_box(const QByteArray& value) {
    return box(QByteArrayLiteral("data"), QByteArray(8, '\0') + value);
}

QByteArray metadata_item(const QByteArray& type, const QByteArray& value) {
    return box(type, data_box(value));
}

QByteArray meta_box(const QByteArray& items) {
    return box(QByteArrayLiteral("meta"),
               QByteArray(4, '\0') + box(QByteArrayLiteral("ilst"), items));
}

QByteArray mvhd_v0(quint32 timescale, quint32 duration) {
    return box(QByteArrayLiteral("mvhd"), QByteArray(12, '\0') + be32(timescale) + be32(duration));
}

QByteArray mvhd_v1(quint32 timescale, quint64 duration) {
    QByteArray payload(4, '\0');
    payload[0] = 1;
    return box(QByteArrayLiteral("mvhd"),
               payload + QByteArray(16, '\0') + be32(timescale) + be64(duration));
}

QByteArray mvhd_version(quint8 version) {
    QByteArray payload(32, '\0');
    payload[0] = static_cast<char>(version);
    return box(QByteArrayLiteral("mvhd"), payload);
}

QByteArray chpl_box(const std::vector<std::pair<QByteArray, quint64>>& entries) {
    QByteArray payload(8, '\0');
    payload.append(static_cast<char>(entries.size()));
    for (const auto& [title, start] : entries) {
        payload.append(be64(start));
        payload.append(static_cast<char>(title.size()));
        payload.append(title);
    }
    return box(QByteArrayLiteral("chpl"), payload);
}

QByteArray chapter_payload(quint8 count, const QByteArray& entries) {
    QByteArray payload(8, '\0');
    payload.append(static_cast<char>(count));
    payload.append(entries);
    return box(QByteArrayLiteral("chpl"), payload);
}

QByteArray audio_file(const QByteArray& moov_payload) {
    return box(QByteArrayLiteral("ftyp"), QByteArray(8, '\0')) +
        box(QByteArrayLiteral("moov"), moov_payload);
}

QString write_bytes(const QTemporaryDir& directory, const QByteArray& bytes,
                    const QString& name = QStringLiteral("fixture.m4b")) {
    const QString path = directory.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write(bytes);
    file.close();
    return path;
}

QString text_of(const std::optional<std::string>& value) {
    return value.has_value() ? QString::fromStdString(*value) : QString();
}

QString artwork_of(const SasayakiAudiobookMetadata& metadata) {
    return metadata.artwork_data.has_value() ? QString::fromLatin1(metadata.artwork_data->toHex())
                                             : QString();
}

SasayakiAudiobookChapter chapter(int id, double start, std::optional<double> end) {
    SasayakiAudiobookChapter value;
    value.id = id;
    value.title = QStringLiteral("Chapter").toStdString();
    value.start_time = start;
    value.end_time = end;
    return value;
}

}  // namespace

class AudiobookMetadataTest : public QObject {
    Q_OBJECT

   private slots:
    void fixture_metadata();
    void fixture_chapters();
    void metadata_absent();
    void metadata_item_list_location();
    void metadata_keys_and_first_valid_item();
    void metadata_text_normalization();
    void metadata_artwork();
    void metadata_data_limits();
    void box_sizes();
    void chapters_absent();
    void chapters_box_location();
    void chapter_list_truncation();
    void chapter_sort_and_titles();
    void chapter_end_times();
    void movie_duration_versions();
    void current_chapter();
};

// B1: the exact fixture and values of script/test_sasayaki_audiobook_metadata.swift.
void AudiobookMetadataTest::fixture_metadata() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QByteArray artwork = QByteArray::fromHex("89504e47");
    const QByteArray items = metadata_item(type_name(), QByteArrayLiteral("Fixture Audiobook")) +
        metadata_item(type_artist(), QByteArrayLiteral("Fixture Artist")) +
        metadata_item(QByteArrayLiteral("covr"), artwork);
    const QByteArray chapters = chpl_box(
        {{QByteArrayLiteral("Opening"), 0}, {QByteArrayLiteral("Chapter Two"), 300000000}});
    const QString path =
        write_bytes(directory,
                    audio_file(mvhd_v0(1000, 90000) +
                               box(QByteArrayLiteral("udta"), chapters + meta_box(items))));
    QVERIFY(!path.isEmpty());

    const SasayakiAudiobookMetadata metadata = read_audiobook_metadata(path);
    QCOMPARE(text_of(metadata.title), QStringLiteral("Fixture Audiobook"));
    QCOMPARE(text_of(metadata.artist), QStringLiteral("Fixture Artist"));
    QCOMPARE(artwork_of(metadata), QStringLiteral("89504e47"));
}

void AudiobookMetadataTest::fixture_chapters() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QByteArray chapters = chpl_box(
        {{QByteArrayLiteral("Opening"), 0}, {QByteArrayLiteral("Chapter Two"), 300000000}});
    const QString path = write_bytes(
        directory, audio_file(mvhd_v0(1000, 90000) + box(QByteArrayLiteral("udta"), chapters)));
    QVERIFY(!path.isEmpty());

    const std::vector<SasayakiAudiobookChapter> read = read_audiobook_chapters(path);
    QCOMPARE(static_cast<int>(read.size()), 2);
    if (read.size() != 2) {
        return;
    }
    QCOMPARE(read[0].id, 0);
    QCOMPARE(QString::fromStdString(read[0].title), QStringLiteral("Opening"));
    QCOMPARE(read[0].start_time, 0.0);
    QVERIFY(read[0].end_time.has_value());
    QCOMPARE(read[0].end_time.value_or(-1.0), 30.0);
    QCOMPARE(read[1].id, 1);
    QCOMPARE(QString::fromStdString(read[1].title), QStringLiteral("Chapter Two"));
    QCOMPARE(read[1].start_time, 30.0);
    QVERIFY(read[1].end_time.has_value());
    QCOMPARE(read[1].end_time.value_or(-1.0), 90.0);
}

// B2: parse never reports an error.
void AudiobookMetadataTest::metadata_absent() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const SasayakiAudiobookMetadata missing =
        read_audiobook_metadata(directory.filePath(QStringLiteral("absent.m4b")));
    QVERIFY(!missing.title.has_value());
    QVERIFY(!missing.artist.has_value());
    QVERIFY(!missing.artwork_data.has_value());

    const QString no_moov =
        write_bytes(directory, box(QByteArrayLiteral("ftyp"), QByteArray(8, '\0')),
                    QStringLiteral("no_moov.m4b"));
    const SasayakiAudiobookMetadata without_movie = read_audiobook_metadata(no_moov);
    QVERIFY(!without_movie.title.has_value());
    QVERIFY(!without_movie.artist.has_value());
    QVERIFY(!without_movie.artwork_data.has_value());

    const QString no_items =
        write_bytes(directory, audio_file(mvhd_v0(1000, 90000)), QStringLiteral("no_items.m4b"));
    const SasayakiAudiobookMetadata without_items = read_audiobook_metadata(no_items);
    QVERIFY(!without_items.title.has_value());
    QVERIFY(!without_items.artist.has_value());
    QVERIFY(!without_items.artwork_data.has_value());

    // A directory cannot be opened as a file.
    const SasayakiAudiobookMetadata directory_path = read_audiobook_metadata(directory.path());
    QVERIFY(!directory_path.title.has_value());
    QVERIFY(!directory_path.artist.has_value());
    QVERIFY(!directory_path.artwork_data.has_value());
}

// B3: moov/udta/meta/ilst preferred, then moov/meta/ilst.
void AudiobookMetadataTest::metadata_item_list_location() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString both = write_bytes(
        directory,
        audio_file(box(QByteArrayLiteral("udta"),
                       meta_box(metadata_item(type_name(), QByteArrayLiteral("From Udta")))) +
                   meta_box(metadata_item(type_name(), QByteArrayLiteral("From Moov")))),
        QStringLiteral("both.m4b"));
    QCOMPARE(text_of(read_audiobook_metadata(both).title), QStringLiteral("From Udta"));

    const QString no_udta_meta = write_bytes(
        directory,
        audio_file(box(QByteArrayLiteral("udta"), box(QByteArrayLiteral("free"), QByteArray())) +
                   meta_box(metadata_item(type_name(), QByteArrayLiteral("Direct")))),
        QStringLiteral("direct.m4b"));
    QCOMPARE(text_of(read_audiobook_metadata(no_udta_meta).title), QStringLiteral("Direct"));

    // meta with fewer than the four version-and-flags bytes has no item list.
    const QString short_meta =
        write_bytes(directory, audio_file(box(QByteArrayLiteral("meta"), QByteArray(2, '\0'))),
                    QStringLiteral("short_meta.m4b"));
    QVERIFY(!read_audiobook_metadata(short_meta).title.has_value());

    // A udta/meta without ilst returns empty; the direct moov/meta is not retried.
    const QString udta_meta_without_items = write_bytes(
        directory,
        audio_file(
            box(QByteArrayLiteral("udta"), box(QByteArrayLiteral("meta"), QByteArray(4, '\0'))) +
            meta_box(metadata_item(type_name(), QByteArrayLiteral("From Moov")))),
        QStringLiteral("udta_meta_without_items.m4b"));
    QVERIFY(!read_audiobook_metadata(udta_meta_without_items).title.has_value());
}

// B4: the raw four-byte item types, and the first non-empty value per key.
void AudiobookMetadataTest::metadata_keys_and_first_valid_item() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString album_artist =
        write_bytes(directory,
                    audio_file(meta_box(metadata_item(QByteArrayLiteral("aART"),
                                                      QByteArrayLiteral("Album Artist")))),
                    QStringLiteral("aart.m4b"));
    QCOMPARE(text_of(read_audiobook_metadata(album_artist).artist), QStringLiteral("Album Artist"));

    const QString both_artists =
        write_bytes(directory,
                    audio_file(meta_box(metadata_item(type_artist(), QByteArrayLiteral("Artist")) +
                                        metadata_item(QByteArrayLiteral("aART"),
                                                      QByteArrayLiteral("Album Artist")))),
                    QStringLiteral("both_artists.m4b"));
    QCOMPARE(text_of(read_audiobook_metadata(both_artists).artist), QStringLiteral("Artist"));

    const QString unknown_key = write_bytes(
        directory,
        audio_file(meta_box(metadata_item(QByteArrayLiteral("junk"), QByteArrayLiteral("Ignored")) +
                            metadata_item(type_name(), QByteArrayLiteral("Title")))),
        QStringLiteral("unknown.m4b"));
    const SasayakiAudiobookMetadata unknown = read_audiobook_metadata(unknown_key);
    QCOMPARE(text_of(unknown.title), QStringLiteral("Title"));
    QVERIFY(!unknown.artist.has_value());

    // An earlier item that normalizes to empty does not block a later one.
    const QString later_wins =
        write_bytes(directory,
                    audio_file(meta_box(metadata_item(type_name(), QByteArrayLiteral(" \t ")) +
                                        metadata_item(type_name(), QByteArrayLiteral("Later")))),
                    QStringLiteral("later.m4b"));
    QCOMPARE(text_of(read_audiobook_metadata(later_wins).title), QStringLiteral("Later"));

    // The first non-empty item of a key wins over a later non-empty one.
    const QString first_wins =
        write_bytes(directory,
                    audio_file(meta_box(metadata_item(type_name(), QByteArrayLiteral("First")) +
                                        metadata_item(type_name(), QByteArrayLiteral("Second")))),
                    QStringLiteral("first.m4b"));
    QCOMPARE(text_of(read_audiobook_metadata(first_wins).title), QStringLiteral("First"));

    // An empty ©ART leaves artist nil, so aART supplies the artist.
    const QString empty_artist_falls_back =
        write_bytes(directory,
                    audio_file(meta_box(metadata_item(type_artist(), QByteArrayLiteral(" \t ")) +
                                        metadata_item(QByteArrayLiteral("aART"),
                                                      QByteArrayLiteral("Album Artist")))),
                    QStringLiteral("empty_artist.m4b"));
    QCOMPARE(text_of(read_audiobook_metadata(empty_artist_falls_back).artist),
             QStringLiteral("Album Artist"));
}

// B5: UTF-8 replacement, the Foundation whitespace set, and empty becoming absent.
void AudiobookMetadataTest::metadata_text_normalization() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const std::vector<char32_t> whitespace = {
        0x0009, 0x0020, 0x00A0, 0x1680, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004,
        0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A, 0x202F, 0x205F, 0x3000,
        0x000A, 0x000B, 0x000C, 0x000D, 0x0085, 0x2028, 0x2029};
    for (const char32_t code : whitespace) {
        const QString trimmed = QString(QChar(code)) + QStringLiteral("X") + QChar(code);
        const QString path = write_bytes(
            directory, audio_file(meta_box(metadata_item(type_name(), trimmed.toUtf8()))),
            QStringLiteral("space_%1.m4b").arg(static_cast<uint>(code), 4, 16, QLatin1Char('0')));
        QCOMPARE(text_of(read_audiobook_metadata(path).title), QStringLiteral("X"));
    }

    const QString invalid = write_bytes(
        directory,
        audio_file(
            meta_box(metadata_item(type_name(), QByteArray("\xFF", 1) + QByteArrayLiteral("A")))),
        QStringLiteral("invalid.m4b"));
    const QString expected = QString(QChar(QChar::ReplacementCharacter)) + QStringLiteral("A");
    QCOMPARE(text_of(read_audiobook_metadata(invalid).title), expected);

    const QString interior = write_bytes(
        directory, audio_file(meta_box(metadata_item(type_name(), QByteArrayLiteral("A  B")))),
        QStringLiteral("interior.m4b"));
    QCOMPARE(text_of(read_audiobook_metadata(interior).title), QStringLiteral("A  B"));

    const QString empty = write_bytes(
        directory, audio_file(meta_box(metadata_item(type_name(), QByteArrayLiteral("   ")))),
        QStringLiteral("empty.m4b"));
    QVERIFY(!read_audiobook_metadata(empty).title.has_value());
}

// B6: covr is handed on unmodified and a second one is ignored.
void AudiobookMetadataTest::metadata_artwork() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QByteArray first = QByteArray::fromHex("89504e47");
    const QByteArray second = QByteArray::fromHex("ffd8ff");
    const QString path =
        write_bytes(directory,
                    audio_file(meta_box(metadata_item(QByteArrayLiteral("covr"), first) +
                                        metadata_item(QByteArrayLiteral("covr"), second))),
                    QStringLiteral("covers.m4b"));
    const SasayakiAudiobookMetadata metadata = read_audiobook_metadata(path);
    QVERIFY(metadata.artwork_data.has_value());
    QCOMPARE(*metadata.artwork_data, first);

    // Only the first data child is examined; an invalid one skips the whole item.
    const QString invalid_first_data =
        write_bytes(directory,
                    audio_file(meta_box(
                        box(QByteArrayLiteral("covr"),
                            data_box(QByteArray()) + data_box(QByteArray::fromHex("89504e47"))))),
                    QStringLiteral("invalid_first_data.m4b"));
    QVERIFY(!read_audiobook_metadata(invalid_first_data).artwork_data.has_value());
}

// B7: a data child is used only when its payload is 1..20 MiB.
void AudiobookMetadataTest::metadata_data_limits() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // No data child at all, then a valid artist: remaining items still parse.
    const QString no_data =
        write_bytes(directory,
                    audio_file(meta_box(box(type_name(), QByteArray()) +
                                        metadata_item(type_artist(), QByteArrayLiteral("A")))),
                    QStringLiteral("no_data.m4b"));
    const SasayakiAudiobookMetadata without_data = read_audiobook_metadata(no_data);
    QVERIFY(!without_data.title.has_value());
    QCOMPARE(text_of(without_data.artist), QStringLiteral("A"));

    // An empty payload is skipped.
    const QString empty_data =
        write_bytes(directory,
                    audio_file(meta_box(metadata_item(type_name(), QByteArray()) +
                                        metadata_item(type_artist(), QByteArrayLiteral("A")))),
                    QStringLiteral("empty_data.m4b"));
    const SasayakiAudiobookMetadata with_empty = read_audiobook_metadata(empty_data);
    QVERIFY(!with_empty.title.has_value());
    QCOMPARE(text_of(with_empty.artist), QStringLiteral("A"));

    const qsizetype maximum = 20 * 1024 * 1024;
    const QString at_limit = write_bytes(
        directory,
        audio_file(meta_box(metadata_item(QByteArrayLiteral("covr"), QByteArray(maximum, 'y')))),
        QStringLiteral("at_limit.m4b"));
    const SasayakiAudiobookMetadata accepted = read_audiobook_metadata(at_limit);
    QVERIFY(accepted.artwork_data.has_value());
    QCOMPARE(accepted.artwork_data->size(), maximum);

    const QString over_limit = write_bytes(
        directory,
        audio_file(meta_box(metadata_item(QByteArrayLiteral("covr"), QByteArray(maximum + 1, 'z')) +
                            metadata_item(type_artist(), QByteArrayLiteral("A")))),
        QStringLiteral("over_limit.m4b"));
    const SasayakiAudiobookMetadata rejected = read_audiobook_metadata(over_limit);
    QVERIFY(!rejected.artwork_data.has_value());
    QCOMPARE(text_of(rejected.artist), QStringLiteral("A"));
}

// B8: the three size forms, malformed boxes ending a parent scan, and a box at
// the parent end not being read.
void AudiobookMetadataTest::box_sizes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QByteArray moov_payload = mvhd_v0(1000, 90000) +
        box(QByteArrayLiteral("udta"),
            chpl_box({{QByteArrayLiteral("A"), 0}}) +
                meta_box(metadata_item(type_name(), QByteArrayLiteral("Open"))));

    const QString open_ended =
        write_bytes(directory,
                    box(QByteArrayLiteral("ftyp"), QByteArray(8, '\0')) +
                        open_ended_box(QByteArrayLiteral("moov"), moov_payload),
                    QStringLiteral("open_ended.m4b"));
    QCOMPARE(text_of(read_audiobook_metadata(open_ended).title), QStringLiteral("Open"));
    QCOMPARE(static_cast<int>(read_audiobook_chapters(open_ended).size()), 1);

    const QString large = write_bytes(directory,
                                      box(QByteArrayLiteral("ftyp"), QByteArray(8, '\0')) +
                                          large_box(QByteArrayLiteral("moov"), moov_payload),
                                      QStringLiteral("large.m4b"));
    QCOMPARE(text_of(read_audiobook_metadata(large).title), QStringLiteral("Open"));
    QCOMPARE(static_cast<int>(read_audiobook_chapters(large).size()), 1);

    // A size below the header ends the ilst scan and keeps the boxes before it.
    const QString malformed = write_bytes(
        directory,
        audio_file(meta_box(metadata_item(type_name(), QByteArrayLiteral("Kept")) + be32(4) +
                            QByteArrayLiteral("junk") +
                            metadata_item(type_artist(), QByteArrayLiteral("Dropped")))),
        QStringLiteral("malformed.m4b"));
    const SasayakiAudiobookMetadata kept = read_audiobook_metadata(malformed);
    QCOMPARE(text_of(kept.title), QStringLiteral("Kept"));
    QVERIFY(!kept.artist.has_value());

    // A size past the parent end also ends the scan.
    const QString past_end = write_bytes(
        directory,
        audio_file(meta_box(metadata_item(type_name(), QByteArrayLiteral("Kept")) + be32(9999) +
                            QByteArrayLiteral("wide") +
                            metadata_item(type_artist(), QByteArrayLiteral("Dropped")))),
        QStringLiteral("past_end.m4b"));
    const SasayakiAudiobookMetadata clipped = read_audiobook_metadata(past_end);
    QCOMPARE(text_of(clipped.title), QStringLiteral("Kept"));
    QVERIFY(!clipped.artist.has_value());

    // A malformed udta child makes the chpl search fall back to moov/chpl.
    const QString fall_back =
        write_bytes(directory,
                    audio_file(box(QByteArrayLiteral("udta"),
                                   be32(4) + QByteArrayLiteral("junk") +
                                       chpl_box({{QByteArrayLiteral("Udta"), 0}})) +
                               chpl_box({{QByteArrayLiteral("Direct"), 0}})),
                    QStringLiteral("fall_back.m4b"));
    const std::vector<SasayakiAudiobookChapter> fallback_chapters =
        read_audiobook_chapters(fall_back);
    QCOMPARE(static_cast<int>(fallback_chapters.size()), 1);
    if (!fallback_chapters.empty()) {
        QCOMPARE(QString::fromStdString(fallback_chapters[0].title), QStringLiteral("Direct"));
    }
}

// B9: the chapter parser never reports an error.
void AudiobookMetadataTest::chapters_absent() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QVERIFY(read_audiobook_chapters(directory.filePath(QStringLiteral("absent.m4b"))).empty());
    QVERIFY(read_audiobook_chapters(directory.path()).empty());

    const QString no_moov =
        write_bytes(directory, box(QByteArrayLiteral("ftyp"), QByteArray(8, '\0')),
                    QStringLiteral("no_moov.m4b"));
    QVERIFY(read_audiobook_chapters(no_moov).empty());

    const QString no_chpl =
        write_bytes(directory, audio_file(mvhd_v0(1000, 90000)), QStringLiteral("no_chpl.m4b"));
    QVERIFY(read_audiobook_chapters(no_chpl).empty());
}

// B10: moov/udta/chpl preferred, then moov/chpl.
void AudiobookMetadataTest::chapters_box_location() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString both = write_bytes(
        directory,
        audio_file(box(QByteArrayLiteral("udta"), chpl_box({{QByteArrayLiteral("From Udta"), 0}})) +
                   chpl_box({{QByteArrayLiteral("From Moov"), 0}})),
        QStringLiteral("both.m4b"));
    const std::vector<SasayakiAudiobookChapter> udta = read_audiobook_chapters(both);
    QCOMPARE(static_cast<int>(udta.size()), 1);
    if (!udta.empty()) {
        QCOMPARE(QString::fromStdString(udta[0].title), QStringLiteral("From Udta"));
    }

    const QString no_udta_chpl =
        write_bytes(directory,
                    audio_file(box(QByteArrayLiteral("udta"), mvhd_v0(1000, 90000)) +
                               chpl_box({{QByteArrayLiteral("Direct"), 0}})),
                    QStringLiteral("direct.m4b"));
    const std::vector<SasayakiAudiobookChapter> direct = read_audiobook_chapters(no_udta_chpl);
    QCOMPARE(static_cast<int>(direct.size()), 1);
    if (!direct.empty()) {
        QCOMPARE(QString::fromStdString(direct[0].title), QStringLiteral("Direct"));
    }
}

// B11: the chpl payload layout and every truncation branch.
void AudiobookMetadataTest::chapter_list_truncation() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString short_box =
        write_bytes(directory, audio_file(box(QByteArrayLiteral("chpl"), QByteArray(8, '\0'))),
                    QStringLiteral("short_box.m4b"));
    QVERIFY(read_audiobook_chapters(short_box).empty());

    const QString empty_count = write_bytes(directory, audio_file(chapter_payload(0, QByteArray())),
                                            QStringLiteral("empty_count.m4b"));
    QVERIFY(read_audiobook_chapters(empty_count).empty());

    // The count announces more entries than the box holds.
    const QString fewer_entries =
        write_bytes(directory, audio_file(chapter_payload(3, be64(0) + QByteArray(1, '\0'))),
                    QStringLiteral("fewer_entries.m4b"));
    const std::vector<SasayakiAudiobookChapter> announced = read_audiobook_chapters(fewer_entries);
    QCOMPARE(static_cast<int>(announced.size()), 1);

    // A truncated 9-byte fixed part keeps the entries before the cut.
    const QString cut_fixed = write_bytes(directory,
                                          audio_file(chapter_payload(2,
                                                                     be64(0) +
                                                                         QByteArray("\x01"
                                                                                    "A",
                                                                                    2) +
                                                                         be64(100))),
                                          QStringLiteral("cut_fixed.m4b"));
    const std::vector<SasayakiAudiobookChapter> fixed = read_audiobook_chapters(cut_fixed);
    QCOMPARE(static_cast<int>(fixed.size()), 1);
    if (!fixed.empty()) {
        QCOMPARE(QString::fromStdString(fixed[0].title), QStringLiteral("A"));
    }

    // A title longer than the remaining box keeps the entries before the cut.
    const QString cut_title =
        write_bytes(directory,
                    audio_file(chapter_payload(2,
                                               be64(0) +
                                                   QByteArray("\x01"
                                                              "A",
                                                              2) +
                                                   be64(100) + QByteArray(1, '\x05') +
                                                   QByteArrayLiteral("ab"))),
                    QStringLiteral("cut_title.m4b"));
    const std::vector<SasayakiAudiobookChapter> title = read_audiobook_chapters(cut_title);
    QCOMPARE(static_cast<int>(title.size()), 1);
    if (!title.empty()) {
        QCOMPARE(QString::fromStdString(title[0].title), QStringLiteral("A"));
    }

    // A title of length zero is empty and becomes "Chapter N".
    const QString zero_title =
        write_bytes(directory, audio_file(chapter_payload(1, be64(0) + QByteArray(1, '\0'))),
                    QStringLiteral("zero_title.m4b"));
    const std::vector<SasayakiAudiobookChapter> zero = read_audiobook_chapters(zero_title);
    QCOMPARE(static_cast<int>(zero.size()), 1);
    if (!zero.empty()) {
        QCOMPARE(QString::fromStdString(zero[0].title), QStringLiteral("Chapter 1"));
    }
}

// B12: stable ascending start order, post-sort ids and generated titles.
void AudiobookMetadataTest::chapter_sort_and_titles() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = write_bytes(
        directory,
        audio_file(mvhd_v0(1000, 90000) +
                   chpl_box({{QByteArray(), 200000000}, {QByteArrayLiteral("First"), 0}})),
        QStringLiteral("sorted.m4b"));
    const std::vector<SasayakiAudiobookChapter> chapters = read_audiobook_chapters(path);
    QCOMPARE(static_cast<int>(chapters.size()), 2);
    if (chapters.size() != 2) {
        return;
    }
    QCOMPARE(QString::fromStdString(chapters[0].title), QStringLiteral("First"));
    QCOMPARE(chapters[0].id, 0);
    QCOMPARE(chapters[0].start_time, 0.0);
    QCOMPARE(QString::fromStdString(chapters[1].title), QStringLiteral("Chapter 2"));
    QCOMPARE(chapters[1].id, 1);
    QCOMPARE(chapters[1].start_time, 20.0);

    // Chapter titles are trimmed with the same whitespace set as metadata.
    const QString trimmed_titles =
        write_bytes(directory,
                    audio_file(chpl_box({{QByteArrayLiteral(" \tSpaced \n"), 0},
                                         {QByteArrayLiteral("Second"), 100000000}})),
                    QStringLiteral("trimmed_titles.m4b"));
    const std::vector<SasayakiAudiobookChapter> spaced = read_audiobook_chapters(trimmed_titles);
    QCOMPARE(static_cast<int>(spaced.size()), 2);
    if (spaced.size() == 2) {
        QCOMPARE(QString::fromStdString(spaced[0].title), QStringLiteral("Spaced"));
        QCOMPARE(QString::fromStdString(spaced[1].title), QStringLiteral("Second"));
    }
}

// B13: an end is the next start, and only the last chapter takes the movie end.
void AudiobookMetadataTest::chapter_end_times() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString without_mvhd = write_bytes(
        directory,
        audio_file(chpl_box({{QByteArrayLiteral("A"), 0}, {QByteArrayLiteral("B"), 100000000}})),
        QStringLiteral("without_mvhd.m4b"));
    const std::vector<SasayakiAudiobookChapter> open_end = read_audiobook_chapters(without_mvhd);
    QCOMPARE(static_cast<int>(open_end.size()), 2);
    if (open_end.size() == 2) {
        QVERIFY(open_end[0].end_time.has_value());
        QCOMPARE(open_end[0].end_time.value_or(-1.0), 10.0);
        QVERIFY(!open_end[1].end_time.has_value());
    }

    // A movie duration earlier than the last start leaves the last end absent.
    const QString earlier = write_bytes(
        directory,
        audio_file(mvhd_v0(1000, 5000) +
                   chpl_box({{QByteArrayLiteral("A"), 0}, {QByteArrayLiteral("B"), 100000000}})),
        QStringLiteral("earlier.m4b"));
    const std::vector<SasayakiAudiobookChapter> early = read_audiobook_chapters(earlier);
    QCOMPARE(static_cast<int>(early.size()), 2);
    if (early.size() == 2) {
        QVERIFY(early[0].end_time.has_value());
        QCOMPARE(early[0].end_time.value_or(-1.0), 10.0);
        QVERIFY(!early[1].end_time.has_value());
    }

    // A duration equal to the last start is admitted.
    const QString equal = write_bytes(
        directory,
        audio_file(mvhd_v0(1000, 10000) + chpl_box({{QByteArrayLiteral("Only"), 100000000}})),
        QStringLiteral("equal.m4b"));
    const std::vector<SasayakiAudiobookChapter> same = read_audiobook_chapters(equal);
    QCOMPARE(static_cast<int>(same.size()), 1);
    if (!same.empty()) {
        QVERIFY(same[0].end_time.has_value());
        QCOMPARE(same[0].end_time.value_or(-1.0), 10.0);
    }
}

// B14: mvhd versions, a zero timescale, an unknown version and a truncated box.
void AudiobookMetadataTest::movie_duration_versions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString version_one = write_bytes(
        directory, audio_file(mvhd_v1(1000, 90000) + chpl_box({{QByteArrayLiteral("Only"), 0}})),
        QStringLiteral("v1.m4b"));
    const std::vector<SasayakiAudiobookChapter> v1 = read_audiobook_chapters(version_one);
    QCOMPARE(static_cast<int>(v1.size()), 1);
    if (!v1.empty()) {
        QVERIFY(v1[0].end_time.has_value());
        QCOMPARE(v1[0].end_time.value_or(-1.0), 90.0);
    }

    const QString unknown = write_bytes(
        directory, audio_file(mvhd_version(2) + chpl_box({{QByteArrayLiteral("Only"), 0}})),
        QStringLiteral("unknown.m4b"));
    const std::vector<SasayakiAudiobookChapter> v2 = read_audiobook_chapters(unknown);
    QCOMPARE(static_cast<int>(v2.size()), 1);
    if (!v2.empty()) {
        QVERIFY(!v2[0].end_time.has_value());
    }

    const QString zero_timescale = write_bytes(
        directory, audio_file(mvhd_v0(0, 90000) + chpl_box({{QByteArrayLiteral("Only"), 0}})),
        QStringLiteral("zero_timescale.m4b"));
    const std::vector<SasayakiAudiobookChapter> zero = read_audiobook_chapters(zero_timescale);
    QCOMPARE(static_cast<int>(zero.size()), 1);
    if (!zero.empty()) {
        QVERIFY(!zero[0].end_time.has_value());
    }

    // The mvhd is the last bytes of the file, so its truncated duration is a
    // read past the end: the whole chapter read fails.
    const QString truncated =
        write_bytes(directory,
                    audio_file(chpl_box({{QByteArrayLiteral("Only"), 0}}) +
                               box(QByteArrayLiteral("mvhd"), QByteArray(4, '\0'))),
                    QStringLiteral("truncated.m4b"));
    QVERIFY(read_audiobook_chapters(truncated).empty());
}

// B15: SasayakiPlayer.currentAudiobookChapterID.
void AudiobookMetadataTest::current_chapter() {
    std::vector<SasayakiAudiobookChapter> closed = {chapter(0, 0.0, 10.0), chapter(1, 10.0, 20.0),
                                                    chapter(2, 20.0, 30.0)};
    QVERIFY(!current_audiobook_chapter_id({}, 0.0).has_value());
    QCOMPARE(current_audiobook_chapter_id(closed, 0.0).value_or(-1), 0);
    QCOMPARE(current_audiobook_chapter_id(closed, 9.999).value_or(-1), 0);
    QCOMPARE(current_audiobook_chapter_id(closed, 10.0).value_or(-1), 1);
    QCOMPARE(current_audiobook_chapter_id(closed, 20.0).value_or(-1), 2);
    QCOMPARE(current_audiobook_chapter_id(closed, 30.0).value_or(-1), 2);
    QVERIFY(!current_audiobook_chapter_id(closed, 30.001).has_value());
    QVERIFY(!current_audiobook_chapter_id(closed, -1.0).has_value());

    std::vector<SasayakiAudiobookChapter> open = {chapter(0, 0.0, 10.0),
                                                  chapter(1, 10.0, std::nullopt)};
    QCOMPARE(current_audiobook_chapter_id(open, 100.0).value_or(-1), 1);
}

QTEST_APPLESS_MAIN(AudiobookMetadataTest)

#include "tst_audiobook_metadata.moc"

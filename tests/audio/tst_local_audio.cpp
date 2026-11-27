// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/audio/local_audio.h"

#include <sqlite3.h>

#include <QTest>

#include <array>
#include <string>
#include <string_view>

using niratan::audio::LocalAudioEntry;
using niratan::audio::audio_mime_type;
using niratan::audio::default_sources;
using niratan::audio::find_local_audio;
using niratan::audio::is_supported_audio_file;
using niratan::audio::kLocalAudioPath;
using niratan::audio::kLocalAudioPort;
using niratan::audio::kLocalAudioUrlTemplate;
using niratan::audio::katakana_to_hiragana;
using niratan::audio::local_audio_url;
using niratan::audio::percent_encode_path;

// Core/LocalFileServer.swift (getAudioSources, getAudio,
// mimeType(forAudioFile:), katakanaToHiragana, defaultSources,
// supportedAudioExtensions, localAudioPath) and Core/LocalAudioEndpoint.swift
// have no test script of their own, so the cases follow those functions
// directly. The query cases build a temporary entries table instead of an
// imported android.db.
class LocalAudioTest : public QObject {
    Q_OBJECT

   private slots:
    void constants_match_the_endpoint_and_sources();
    void katakana_scalars_become_hiragana();
    void katakana_conversion_leaves_other_scalars_alone();
    void path_bytes_are_percent_encoded_except_the_allowed_set();
    void mime_types_follow_the_extension_table();
    void local_audio_url_joins_the_source_and_the_encoded_file();
    void supported_extensions_are_recognized();
    void empty_reading_matches_only_the_expression();
    void empty_reading_prefers_the_default_source_order();
    void empty_reading_returns_the_unknown_source_last();
    void empty_reading_skips_unsupported_files();
    void non_empty_reading_prefers_the_exact_pair();
    void non_empty_reading_ranks_reading_before_expression();
    void non_empty_reading_breaks_ties_by_source_order();
    void katakana_reading_is_converted_before_the_lookup();
    void only_one_row_is_returned();
    void a_missing_table_returns_nothing();
};

namespace {

// A temporary database with the entries table of an imported android.db.
struct TestDatabase {
    QTemporaryDir directory;
    sqlite3* db = nullptr;

    TestDatabase() {
        if (!directory.isValid()) {
            return;
        }
        const QByteArray path = directory.filePath(QStringLiteral("android.db")).toUtf8();
        if (sqlite3_open(path.constData(), &db) != SQLITE_OK) {
            sqlite3_close(db);
            db = nullptr;
            return;
        }
        execute("CREATE TABLE entries (expression TEXT, reading TEXT, source TEXT, file TEXT)");
    }

    ~TestDatabase() {
        if (db != nullptr) {
            sqlite3_close(db);
        }
    }

    TestDatabase(const TestDatabase&) = delete;
    TestDatabase& operator=(const TestDatabase&) = delete;

    void execute(const char* sql) { sqlite3_exec(db, sql, nullptr, nullptr, nullptr); }

    void insert(std::string_view expression, std::string_view reading, std::string_view source,
                std::string_view file) {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(db, "INSERT INTO entries VALUES (?, ?, ?, ?)", -1, &statement,
                               nullptr) != SQLITE_OK) {
            return;
        }
        const std::array<std::string_view, 4> values{expression, reading, source, file};
        for (std::size_t index = 0; index < values.size(); ++index) {
            sqlite3_bind_text(statement, static_cast<int>(index + 1), values[index].data(),
                              static_cast<int>(values[index].size()), SQLITE_TRANSIENT);
        }
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }
};

void compare_entry(const std::optional<LocalAudioEntry>& entry, std::string_view source,
                   std::string_view file) {
    QVERIFY(entry.has_value());
    QCOMPARE(entry->source, std::string(source));
    QCOMPARE(entry->file, std::string(file));
}

}  // namespace

void LocalAudioTest::constants_match_the_endpoint_and_sources() {
    QCOMPARE(std::string(kLocalAudioPath), std::string("Audio/android.db"));
    QCOMPARE(kLocalAudioPort, static_cast<std::uint16_t>(18765));
    QCOMPARE(std::string(kLocalAudioUrlTemplate),
             std::string("http://localhost:18765/localaudio/get/?term={term}&reading={reading}"));

    const std::array<std::string, 10> expected{"nhk16",  "daijisen", "shinmeikai8", "jpod",
                                               "jpod_alternate", "taas", "ozk5", "forvo",
                                               "forvo_ext", "forvo_ext2"};
    QCOMPARE(default_sources().size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        QCOMPARE(std::string(default_sources()[index]), expected[index]);
    }
}

void LocalAudioTest::katakana_scalars_become_hiragana() {
    QCOMPARE(katakana_to_hiragana("\u30A1"), std::string("\u3041"));
    QCOMPARE(katakana_to_hiragana("\u30F6"), std::string("\u3096"));
    QCOMPARE(katakana_to_hiragana("\u30CD\u30B3"), std::string("\u306D\u3053"));
}

void LocalAudioTest::katakana_conversion_leaves_other_scalars_alone() {
    QCOMPARE(katakana_to_hiragana("\u30F7"), std::string("\u30F7"));
    QCOMPARE(katakana_to_hiragana("\u3042\u3093"), std::string("\u3042\u3093"));
    QCOMPARE(katakana_to_hiragana("abc XYZ 123"), std::string("abc XYZ 123"));
    QCOMPARE(katakana_to_hiragana("\u30CD\u3000\u30B3"), std::string("\u306D\u3000\u3053"));
    QCOMPARE(katakana_to_hiragana(std::string("\xFF\xFE", 2)), std::string("\xFF\xFE", 2));
    QCOMPARE(katakana_to_hiragana(std::string("\xE3\x30" "CD", 4)),
             std::string("\xE3\x30" "CD", 4));
}

void LocalAudioTest::path_bytes_are_percent_encoded_except_the_allowed_set() {
    QCOMPARE(percent_encode_path("a b.mp3"), std::string("a%20b.mp3"));
    QCOMPARE(percent_encode_path("\u8868\u7D19.mp3"), std::string("%E8%A1%A8%E7%B4%99.mp3"));
    QCOMPARE(percent_encode_path("a/b"), std::string("a/b"));
    QCOMPARE(percent_encode_path("100%"), std::string("100%25"));
    QCOMPARE(percent_encode_path("#1"), std::string("%231"));
    QCOMPARE(percent_encode_path("a?b"), std::string("a%3Fb"));
    QCOMPARE(percent_encode_path("-._~!$&'()*+,;=:@/"),
             std::string("-._~!$&'()*+,;=:@/"));
    QCOMPARE(percent_encode_path(""), std::string(""));
}

void LocalAudioTest::mime_types_follow_the_extension_table() {
    QCOMPARE(audio_mime_type("a.mp3"), std::string("audio/mpeg"));
    QCOMPARE(audio_mime_type("a.opus"), std::string("audio/opus"));
    QCOMPARE(audio_mime_type("a.ogg"), std::string("audio/ogg"));
    QCOMPARE(audio_mime_type("a.m4a"), std::string("audio/mp4"));
    QCOMPARE(audio_mime_type("a.aac"), std::string("audio/aac"));
    QCOMPARE(audio_mime_type("a.wav"), std::string("audio/wav"));
    QCOMPARE(audio_mime_type("a.flac"), std::string("audio/flac"));
    QCOMPARE(audio_mime_type("dir/A.MP3"), std::string("audio/mpeg"));
    QCOMPARE(audio_mime_type("dir/x.OPUS"), std::string("audio/opus"));
    QCOMPARE(audio_mime_type("a.tar.mp3"), std::string("audio/mpeg"));
    QCOMPARE(audio_mime_type("a.webm"), std::string("application/octet-stream"));
    QCOMPARE(audio_mime_type("a"), std::string("application/octet-stream"));
    QCOMPARE(audio_mime_type("a."), std::string("application/octet-stream"));
    QCOMPARE(audio_mime_type(".mp3"), std::string("application/octet-stream"));
    QCOMPARE(audio_mime_type(""), std::string("application/octet-stream"));
}

void LocalAudioTest::local_audio_url_joins_the_source_and_the_encoded_file() {
    QCOMPARE(local_audio_url("forvo", "a b/c.mp3"),
             std::string("http://localhost:18765/localaudio/forvo/a%20b/c.mp3"));
    QCOMPARE(local_audio_url("local source", "x.mp3"),
             std::string("http://localhost:18765/localaudio/local source/x.mp3"));
}

void LocalAudioTest::supported_extensions_are_recognized() {
    const std::array<std::string_view, 7> extensions{"mp3", "opus", "ogg", "m4a",
                                                     "aac", "wav",  "flac"};
    for (const std::string_view extension : extensions) {
        const std::string file = "clip." + std::string(extension);
        QVERIFY(is_supported_audio_file(file));
        QVERIFY(is_supported_audio_file("CLIP." + std::string(extension)));
    }
    QVERIFY(!is_supported_audio_file("clip.webm"));
    QVERIFY(!is_supported_audio_file("clip.mp3.bak"));
    QVERIFY(!is_supported_audio_file("clip"));
    QVERIFY(!is_supported_audio_file("clip."));
    QVERIFY(!is_supported_audio_file(".mp3"));

    TestDatabase database;
    QVERIFY(database.db != nullptr);
    for (const std::string_view extension : extensions) {
        database.execute("DELETE FROM entries");
        database.insert("\u732B", "", "taas", "clip." + std::string(extension));
        compare_entry(find_local_audio(database.db, "\u732B", ""), "taas",
                      "clip." + std::string(extension));
    }
}

void LocalAudioTest::empty_reading_matches_only_the_expression() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.insert("\u732B", "\u306D\u3053", "ozk5", "cat.mp3");
    database.insert("\u72AC", "\u306D\u3053", "nhk16", "dog.mp3");

    compare_entry(find_local_audio(database.db, "\u732B", ""), "ozk5", "cat.mp3");
}

void LocalAudioTest::empty_reading_prefers_the_default_source_order() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.insert("\u732B", "", "zzz", "unknown.mp3");
    database.insert("\u732B", "", "forvo", "forvo.mp3");
    database.insert("\u732B", "", "taas", "taas.mp3");
    database.insert("\u732B", "", "jpod", "jpod.mp3");

    compare_entry(find_local_audio(database.db, "\u732B", ""), "jpod", "jpod.mp3");
}

void LocalAudioTest::empty_reading_returns_the_unknown_source_last() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.insert("\u732B", "", "zzz", "unknown.mp3");
    database.insert("\u732B", "", "forvo_ext2", "last.mp3");

    compare_entry(find_local_audio(database.db, "\u732B", ""), "forvo_ext2", "last.mp3");
}

void LocalAudioTest::empty_reading_skips_unsupported_files() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.insert("\u732B", "", "nhk16", "cat.webm");
    database.insert("\u732B", "", "forvo_ext2", "cat.MP3");

    compare_entry(find_local_audio(database.db, "\u732B", ""), "forvo_ext2", "cat.MP3");

    database.execute("DELETE FROM entries");
    database.insert("\u732B", "", "nhk16", "cat.mp3.bak");
    database.insert("\u732B", "", "jpod", "cat");
    QVERIFY(!find_local_audio(database.db, "\u732B", "").has_value());
}

void LocalAudioTest::non_empty_reading_prefers_the_exact_pair() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.insert("\u732B", "\u306D\u3053", "forvo_ext2", "pair.mp3");
    database.insert("\u732B", "\u306C\u3053", "nhk16", "expression.mp3");
    database.insert("\u732B2", "\u306D\u3053", "nhk16", "reading.mp3");

    compare_entry(find_local_audio(database.db, "\u732B", "\u306D\u3053"), "forvo_ext2",
                  "pair.mp3");
}

void LocalAudioTest::non_empty_reading_ranks_reading_before_expression() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.insert("\u732B", "\u306C\u3053", "nhk16", "expression.mp3");
    database.insert("\u5225", "\u306D\u3053", "forvo_ext2", "reading.mp3");

    compare_entry(find_local_audio(database.db, "\u732B", "\u306D\u3053"), "forvo_ext2",
                  "reading.mp3");
}

void LocalAudioTest::non_empty_reading_breaks_ties_by_source_order() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.insert("\u732B", "\u306D\u3053", "taas", "taas.mp3");
    database.insert("\u732B", "\u306D\u3053", "daijisen", "daijisen.mp3");

    compare_entry(find_local_audio(database.db, "\u732B", "\u306D\u3053"), "daijisen",
                  "daijisen.mp3");
}

void LocalAudioTest::katakana_reading_is_converted_before_the_lookup() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.insert("\u732B", "\u306D\u3053", "taas", "cat.mp3");

    compare_entry(find_local_audio(database.db, "\u732B", "\u30CD\u30B3"), "taas", "cat.mp3");
}

void LocalAudioTest::only_one_row_is_returned() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.insert("\u732B", "", "taas", "first.mp3");
    database.insert("\u732B", "", "taas", "second.mp3");

    const std::optional<LocalAudioEntry> entry = find_local_audio(database.db, "\u732B", "");
    QVERIFY(entry.has_value());
    QCOMPARE(entry->source, std::string("taas"));
    QVERIFY(entry->file == "first.mp3" || entry->file == "second.mp3");
}

void LocalAudioTest::a_missing_table_returns_nothing() {
    TestDatabase database;
    QVERIFY(database.db != nullptr);
    database.execute("DROP TABLE entries");

    QVERIFY(!find_local_audio(database.db, "\u732B", "").has_value());
    QVERIFY(!find_local_audio(database.db, "\u732B", "\u306D\u3053").has_value());
}

QTEST_APPLESS_MAIN(LocalAudioTest)

#include "tst_local_audio.moc"

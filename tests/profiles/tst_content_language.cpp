// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/profiles/content_language.h"

#include <QTest>

#include <optional>
#include <string>

using niratan::profiles::ContentLanguage;
using niratan::profiles::display_count;
using niratan::profiles::language_id;
using niratan::profiles::normalize_language;
using niratan::profiles::raw_characters;

// Models/Profile.swift (ContentLanguageProfile: normalize(_:),
// displayCount(forRawCharacters:), rawCharacters(forDisplayCount:)) has no
// test script of its own, so the cases follow its branches directly.
class ContentLanguageTest : public QObject {
    Q_OBJECT

   private slots:
    void language_ids_are_the_swift_raw_values();
    void normalize_trims_and_lowercases();
    void normalize_keeps_the_primary_tag();
    void normalize_skips_leading_separators();
    void normalize_rejects_unknown_and_empty_tags();
    void japanese_counts_characters_as_they_are();
    void english_counts_five_characters_per_display_count();
    void negative_counts_clamp_to_zero();
    void raw_characters_invert_the_display_count();
};

void ContentLanguageTest::language_ids_are_the_swift_raw_values() {
    QCOMPARE(language_id(ContentLanguage::japanese), std::string_view("ja"));
    QCOMPARE(language_id(ContentLanguage::english), std::string_view("en"));
}

void ContentLanguageTest::normalize_trims_and_lowercases() {
    QCOMPARE(normalize_language(" JA "), std::optional<ContentLanguage>{ContentLanguage::japanese});
    QCOMPARE(normalize_language("\tEN\n"), std::optional<ContentLanguage>{ContentLanguage::english});
    QCOMPARE(normalize_language("\u3000Ja\u3000"),
             std::optional<ContentLanguage>{ContentLanguage::japanese});
}

void ContentLanguageTest::normalize_keeps_the_primary_tag() {
    QCOMPARE(normalize_language("ja-JP"), std::optional<ContentLanguage>{ContentLanguage::japanese});
    QCOMPARE(normalize_language("en_US"), std::optional<ContentLanguage>{ContentLanguage::english});
    QCOMPARE(normalize_language("ja--JP"), std::optional<ContentLanguage>{ContentLanguage::japanese});
    QCOMPARE(normalize_language("EN-us"), std::optional<ContentLanguage>{ContentLanguage::english});
}

void ContentLanguageTest::normalize_skips_leading_separators() {
    QCOMPARE(normalize_language("_en"), std::optional<ContentLanguage>{ContentLanguage::english});
    QCOMPARE(normalize_language("-ja"), std::optional<ContentLanguage>{ContentLanguage::japanese});
    QCOMPARE(normalize_language("__ja"), std::optional<ContentLanguage>{ContentLanguage::japanese});
}

void ContentLanguageTest::normalize_rejects_unknown_and_empty_tags() {
    QCOMPARE(normalize_language(""), std::optional<ContentLanguage>{});
    QCOMPARE(normalize_language("   "), std::optional<ContentLanguage>{});
    QCOMPARE(normalize_language("fr"), std::optional<ContentLanguage>{});
    QCOMPARE(normalize_language("japanese"), std::optional<ContentLanguage>{});
    QCOMPARE(normalize_language("_"), std::optional<ContentLanguage>{});
}

void ContentLanguageTest::japanese_counts_characters_as_they_are() {
    QCOMPARE(display_count(ContentLanguage::japanese, 0), 0);
    QCOMPARE(display_count(ContentLanguage::japanese, 1), 1);
    QCOMPARE(display_count(ContentLanguage::japanese, 12345), 12345);
}

void ContentLanguageTest::english_counts_five_characters_per_display_count() {
    QCOMPARE(display_count(ContentLanguage::english, 0), 0);
    QCOMPARE(display_count(ContentLanguage::english, 1), 1);
    QCOMPARE(display_count(ContentLanguage::english, 4), 1);
    QCOMPARE(display_count(ContentLanguage::english, 5), 1);
    QCOMPARE(display_count(ContentLanguage::english, 6), 2);
    QCOMPARE(display_count(ContentLanguage::english, 10), 2);
    QCOMPARE(display_count(ContentLanguage::english, 11), 3);
}

void ContentLanguageTest::negative_counts_clamp_to_zero() {
    QCOMPARE(display_count(ContentLanguage::japanese, -1), 0);
    QCOMPARE(display_count(ContentLanguage::english, -1), 0);
    QCOMPARE(raw_characters(ContentLanguage::japanese, -1), 0);
    QCOMPARE(raw_characters(ContentLanguage::english, -1), 0);
}

void ContentLanguageTest::raw_characters_invert_the_display_count() {
    QCOMPARE(raw_characters(ContentLanguage::japanese, 0), 0);
    QCOMPARE(raw_characters(ContentLanguage::japanese, 123), 123);
    QCOMPARE(raw_characters(ContentLanguage::english, 0), 0);
    QCOMPARE(raw_characters(ContentLanguage::english, 3), 15);
}

QTEST_APPLESS_MAIN(ContentLanguageTest)
#include "tst_content_language.moc"

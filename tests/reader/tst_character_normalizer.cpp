// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/character_normalizer.h"

#include <QTest>

#include <string>

using niratan::reader::decoded_character_references;
using niratan::reader::filtered_text;
using niratan::reader::readable_character_count;

// Cases follow Util/ReaderCharacterNormalizer.swift and
// Util/BookProcessor.swift (process, which counts filtered chapter text).
class CharacterNormalizerTest : public QObject {
    Q_OBJECT

   private slots:
    void keeps_only_the_body_when_present();
    void drops_furigana();
    void drops_script_and_style_blocks();
    void strips_tags_and_spaces();
    void decodes_character_references();
    void keeps_only_countable_characters();
    void counts_readable_characters();
};

void CharacterNormalizerTest::keeps_only_the_body_when_present() {
    const std::string markup =
        "<html><head><title>Outside</title></head><body><p>Inside</p></body></html>";
    QCOMPARE(filtered_text(markup), std::string("Inside"));
}

void CharacterNormalizerTest::drops_furigana() {
    const std::string markup = "<body><p><ruby>漢<rt>かん</rt></ruby>字</p></body>";
    QCOMPARE(filtered_text(markup), std::string("漢字"));
}

void CharacterNormalizerTest::drops_script_and_style_blocks() {
    const std::string markup =
        "<body><style>p { color: red; }</style><script>var x = 1;</script><p>Text</p></body>";
    QCOMPARE(filtered_text(markup), std::string("Text"));
}

void CharacterNormalizerTest::strips_tags_and_spaces() {
    QCOMPARE(filtered_text("<body><p>Hello&nbsp;World</p></body>"), std::string("HelloWorld"));
    QCOMPARE(filtered_text("<body><p>Hello <b>World</b>!</p></body>"), std::string("HelloWorld"));
}

void CharacterNormalizerTest::decodes_character_references() {
    QCOMPARE(decoded_character_references("&#65;&#x42;"), std::string("AB"));
    QCOMPARE(decoded_character_references("&quot;&apos;&lt;&gt;&amp;&nbsp;"),
             std::string("\"'<>& "));
    QCOMPARE(decoded_character_references("&#x3042;"), std::string("あ"));
    QCOMPARE(decoded_character_references("&#xFFFFFFFF;"), std::string(""));

    QCOMPARE(filtered_text("<body><p>&#x6F22;&#23383;</p></body>"), std::string("漢字"));
}

void CharacterNormalizerTest::keeps_only_countable_characters() {
    const std::string markup =
        "<body><p>abc 123　ＡＢＣ１２３ あいう アイウ ｱｲｳ 漢字。、！？ 〇 ◯ 々</p></body>";
    const std::string filtered = filtered_text(markup);

    QVERIFY(filtered.find(" ") == std::string::npos);
    QVERIFY(filtered.find("。") == std::string::npos);
    QVERIFY(filtered.find("!") == std::string::npos);
    QCOMPARE(filtered, std::string("abc123ＡＢＣ１２３あいうアイウｱｲｳ漢字〇◯々"));
}

void CharacterNormalizerTest::counts_readable_characters() {
    QCOMPARE(readable_character_count("<body><p>漢字です。</p></body>"), 4);
    QCOMPARE(readable_character_count("<body></body>"), 0);
    QCOMPARE(readable_character_count("<body><p>Hello, world!</p></body>"), 10);
}

QTEST_APPLESS_MAIN(CharacterNormalizerTest)
#include "tst_character_normalizer.moc"

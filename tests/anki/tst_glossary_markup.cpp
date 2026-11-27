// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/glossary_markup.h"

#include <QTest>

using niratan::anki::strip_dictionary_name;
using niratan::anki::strip_glossary_headers;

// Core/AnkiManager.swift (stripGlossaryHeaders, stripDictionaryName) has no
// test script of its own, so the cases follow its two regular expressions and
// the replacement closure directly.
class GlossaryMarkupTest : public QObject {
    Q_OBJECT

   private slots:
    void headers_are_dropped_from_every_item();
    void headers_may_have_an_empty_dictionary_or_label();
    void a_header_needs_the_space_after_its_closing_tag();
    void a_header_with_markup_inside_is_kept();
    void text_without_headers_is_unchanged();
    void dictionary_name_is_dropped_from_the_label();
    void a_label_that_is_only_the_dictionary_is_dropped_whole();
    void every_occurrence_in_the_label_is_dropped();
    void only_a_trailing_dictionary_name_is_dropped();
    void the_dictionary_name_is_matched_literally();
    void an_empty_dictionary_name_is_not_a_header();
    void each_item_is_handled_on_its_own();
};

void GlossaryMarkupTest::headers_are_dropped_from_every_item() {
    const std::string html =
        R"q(<ul><li data-dictionary="Jitendex"><i>(noun, Jitendex)</i> meaning</li><li data-dictionary="大辞林"><i>(大辞林)</i> x</li></ul>)q";
    QCOMPARE(strip_glossary_headers(html),
             std::string(R"q(<ul><li data-dictionary="Jitendex">meaning</li><li data-dictionary="大辞林">x</li></ul>)q"));
}

void GlossaryMarkupTest::headers_may_have_an_empty_dictionary_or_label() {
    QCOMPARE(strip_glossary_headers(R"q(<li data-dictionary=""><i></i> y)q"),
             std::string(R"q(<li data-dictionary="">y)q"));
}

void GlossaryMarkupTest::a_header_needs_the_space_after_its_closing_tag() {
    const std::string html = R"q(<li data-dictionary="A"><i>x</i>y)q";
    QCOMPARE(strip_glossary_headers(html), html);
    QCOMPARE(strip_dictionary_name(html), html);
}

void GlossaryMarkupTest::a_header_with_markup_inside_is_kept() {
    const std::string html = R"q(<li data-dictionary="A"><i>a<b>b</b></i> y)q";
    QCOMPARE(strip_glossary_headers(html), html);
    QCOMPARE(strip_dictionary_name(html), html);
}

void GlossaryMarkupTest::text_without_headers_is_unchanged() {
    const std::string html = "<ul><li>plain</li></ul> 決定";
    QCOMPARE(strip_glossary_headers(html), html);
    QCOMPARE(strip_dictionary_name(html), html);
    QCOMPARE(strip_glossary_headers(""), std::string());
    QCOMPARE(strip_dictionary_name(""), std::string());
}

void GlossaryMarkupTest::dictionary_name_is_dropped_from_the_label() {
    QCOMPARE(strip_dictionary_name(R"q(<li data-dictionary="Jitendex"><i>(noun, Jitendex)</i> meaning)q"),
             std::string(R"q(<li data-dictionary="Jitendex"><i>(noun)</i> meaning)q"));
    QCOMPARE(strip_dictionary_name(R"q(<li data-dictionary="Jitendex"><i>(noun, verb, Jitendex)</i> meaning)q"),
             std::string(R"q(<li data-dictionary="Jitendex"><i>(noun, verb)</i> meaning)q"));
}

void GlossaryMarkupTest::a_label_that_is_only_the_dictionary_is_dropped_whole() {
    QCOMPARE(strip_dictionary_name(R"q(<li data-dictionary="大辞林"><i>(大辞林)</i> x)q"),
             std::string(R"q(<li data-dictionary="大辞林">x)q"));
}

void GlossaryMarkupTest::every_occurrence_in_the_label_is_dropped() {
    QCOMPARE(strip_dictionary_name(R"q(<li data-dictionary="D"><i>(a, D), (b, D)</i> x)q"),
             std::string(R"q(<li data-dictionary="D"><i>(a), (b)</i> x)q"));
}

void GlossaryMarkupTest::only_a_trailing_dictionary_name_is_dropped() {
    const std::string leading = R"q(<li data-dictionary="D"><i>(D, noun)</i> x)q";
    QCOMPARE(strip_dictionary_name(leading), leading);
    const std::string middle = R"q(<li data-dictionary="D"><i>(noun, D, verb)</i> x)q";
    QCOMPARE(strip_dictionary_name(middle), middle);
}

void GlossaryMarkupTest::the_dictionary_name_is_matched_literally() {
    QCOMPARE(strip_dictionary_name(R"q(<li data-dictionary="A.B(1)"><i>(noun, A.B(1))</i> x)q"),
             std::string(R"q(<li data-dictionary="A.B(1)"><i>(noun)</i> x)q"));
    const std::string other = R"q(<li data-dictionary="A.B"><i>(noun, AxB)</i> x)q";
    QCOMPARE(strip_dictionary_name(other), other);
}

void GlossaryMarkupTest::an_empty_dictionary_name_is_not_a_header() {
    const std::string html = R"q(<li data-dictionary=""><i>x</i> y)q";
    QCOMPARE(strip_dictionary_name(html), html);
}

void GlossaryMarkupTest::each_item_is_handled_on_its_own() {
    const std::string html =
        R"q(<li data-dictionary="A"><i>(n, A)</i> a</li><li data-dictionary="B"><i>(B)</i> b</li><li data-dictionary="C"><i>(v, A)</i> c</li>)q";
    QCOMPARE(strip_dictionary_name(html),
             std::string(R"q(<li data-dictionary="A"><i>(n)</i> a</li><li data-dictionary="B">b</li><li data-dictionary="C"><i>(v, A)</i> c</li>)q"));
}

QTEST_APPLESS_MAIN(GlossaryMarkupTest)
#include "tst_glossary_markup.moc"

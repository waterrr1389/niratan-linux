// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/duplicate_scope.h"

#include <QTest>

using niratan::anki::DuplicateOptions;
using niratan::anki::DuplicateScope;
using niratan::anki::DuplicateScopeOptions;
using niratan::anki::duplicate_options;

// Cases follow script/test_anki_connect_options.swift, which specifies
// DuplicateScope.ankiConnectOptions.
class DuplicateScopeTest : public QObject {
    Q_OBJECT

   private slots:
    void collection_needs_no_scope_options();
    void deck_checks_only_the_named_deck();
    void deck_root_checks_the_top_level_deck_and_its_children();
    void a_deck_without_a_separator_is_its_own_root();
    void leading_separators_do_not_make_an_empty_root();
    void check_all_models_keeps_the_scope();
    void deck_root_and_check_all_models_share_one_object();
};

void DuplicateScopeTest::collection_needs_no_scope_options() {
    const auto options = duplicate_options(DuplicateScope::collection, "日本語::語彙", false, false);
    QCOMPARE(options, (DuplicateOptions{false, "collection", std::nullopt}));
}

void DuplicateScopeTest::deck_checks_only_the_named_deck() {
    const auto options = duplicate_options(DuplicateScope::deck, "日本語::語彙", true, false);
    QCOMPARE(options, (DuplicateOptions{true, "deck", std::nullopt}));
}

void DuplicateScopeTest::deck_root_checks_the_top_level_deck_and_its_children() {
    const auto options = duplicate_options(DuplicateScope::deck_root, "日本語::語彙::N1", false, false);
    QCOMPARE(options, (DuplicateOptions{false, "deck", DuplicateScopeOptions{"日本語", true, false}}));
}

void DuplicateScopeTest::a_deck_without_a_separator_is_its_own_root() {
    const auto options = duplicate_options(DuplicateScope::deck_root, "Mining", false, false);
    QVERIFY(options.scope_options.has_value());
    QCOMPARE(options.scope_options->deck_name, std::string("Mining"));
}

// String.split drops empty pieces, so leading separators do not make an
// empty root, and a name with no piece at all stays whole.
void DuplicateScopeTest::leading_separators_do_not_make_an_empty_root() {
    const auto root = [](std::string_view deck) {
        return duplicate_options(DuplicateScope::deck_root, deck, false, false).scope_options->deck_name;
    };
    QCOMPARE(root("::A::B"), std::string("A"));
    QCOMPARE(root("::::A"), std::string("A"));
    QCOMPARE(root("::"), std::string("::"));
}

void DuplicateScopeTest::check_all_models_keeps_the_scope() {
    const auto options = duplicate_options(DuplicateScope::collection, "Mining", false, true);
    QCOMPARE(options.scope, std::string("collection"));
    QCOMPARE(options.scope_options, (DuplicateScopeOptions{std::nullopt, false, true}));
}

void DuplicateScopeTest::deck_root_and_check_all_models_share_one_object() {
    const auto options = duplicate_options(DuplicateScope::deck_root, "A::B", false, true);
    QCOMPARE(options.scope_options, (DuplicateScopeOptions{"A", true, true}));
}

QTEST_APPLESS_MAIN(DuplicateScopeTest)
#include "tst_duplicate_scope.moc"

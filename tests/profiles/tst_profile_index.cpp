// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/profiles/profile_index.h"

#include <QTest>

#include <map>
#include <string>
#include <vector>

using niratan::profiles::ContentLanguage;
using niratan::profiles::HoshiProfile;
using niratan::profiles::ProfileIndex;
using niratan::profiles::default_japanese_profile;
using niratan::profiles::resolve_profile;

// Models/Profile.swift (HoshiProfile, ProfileIndex.initial, ProfileResolver)
// has no test script of its own, so the cases follow those types directly.
namespace {

HoshiProfile profile(std::string id, std::string name, std::string language,
                     bool is_default = false) {
    return HoshiProfile{std::move(id), std::move(name), std::move(language), is_default};
}

}  // namespace

class ProfileIndexTest : public QObject {
    Q_OBJECT

   private slots:
    void the_built_in_profile_matches_swift();
    void initial_index_holds_the_japanese_default();
    void language_is_english_only_for_the_exact_id();
    void display_name_replaces_legacy_default_names();
    void resolution_prefers_the_active_profile();
    void resolution_falls_back_to_the_default_then_the_first_entry();
    void resolution_falls_back_to_the_built_in_profile();
    void the_first_duplicate_id_wins();
};

void ProfileIndexTest::the_built_in_profile_matches_swift() {
    const HoshiProfile expected = profile("default-ja", "Japanese", "ja", true);

    QCOMPARE(default_japanese_profile(), expected);
}

void ProfileIndexTest::initial_index_holds_the_japanese_default() {
    const ProfileIndex index = ProfileIndex::initial();
    const std::vector<HoshiProfile> profiles{default_japanese_profile()};
    const std::map<std::string, std::string, std::less<>> primary{{"ja", "default-ja"}};

    QCOMPARE(index.profiles, profiles);
    QCOMPARE(index.default_profile_id, std::string("default-ja"));
    QCOMPARE(index.global_active_profile_id, std::string("default-ja"));
    QCOMPARE(index.primary_profile_ids_by_language, primary);
}

void ProfileIndexTest::language_is_english_only_for_the_exact_id() {
    QCOMPARE(profile("a", "A", "en").language(), ContentLanguage::english);
    QCOMPARE(profile("a", "A", "ja").language(), ContentLanguage::japanese);
    QCOMPARE(profile("a", "A", "en-US").language(), ContentLanguage::japanese);
    QCOMPARE(profile("a", "A", "").language(), ContentLanguage::japanese);
}

void ProfileIndexTest::display_name_replaces_legacy_default_names() {
    QCOMPARE(profile("default-ja", "Japanese", "ja", true).display_name(),
             std::string("Japanese"));
    QCOMPARE(profile("default-ja", "Japanese EPUB", "ja", true).display_name(),
             std::string("Japanese"));
    QCOMPARE(profile("default-ja", "Custom", "ja", true).display_name(),
             std::string("Custom"));
    QCOMPARE(profile("profile-1", "Japanese EPUB", "ja").display_name(),
             std::string("Japanese EPUB"));
}

void ProfileIndexTest::resolution_prefers_the_active_profile() {
    ProfileIndex index;
    index.profiles = {profile("a", "A", "ja"), profile("b", "B", "en")};
    index.default_profile_id = "a";
    index.global_active_profile_id = "b";

    QCOMPARE(resolve_profile(index), profile("b", "B", "en"));
}

void ProfileIndexTest::resolution_falls_back_to_the_default_then_the_first_entry() {
    ProfileIndex index;
    index.profiles = {profile("a", "A", "ja"), profile("b", "B", "en")};
    index.default_profile_id = "a";
    index.global_active_profile_id = "missing";

    QCOMPARE(resolve_profile(index), profile("a", "A", "ja"));

    index.default_profile_id = "missing";
    QCOMPARE(resolve_profile(index), profile("a", "A", "ja"));
}

void ProfileIndexTest::resolution_falls_back_to_the_built_in_profile() {
    QCOMPARE(resolve_profile(ProfileIndex{}), default_japanese_profile());
}

void ProfileIndexTest::the_first_duplicate_id_wins() {
    ProfileIndex index;
    index.profiles = {profile("a", "First", "ja"), profile("a", "Second", "en")};
    index.global_active_profile_id = "a";

    QCOMPARE(resolve_profile(index).name, std::string("First"));
}

QTEST_APPLESS_MAIN(ProfileIndexTest)
#include "tst_profile_index.moc"

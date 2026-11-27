// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/profiles/profile_ops.h"

#include <QTest>

#include <map>
#include <optional>
#include <string>
#include <vector>

using niratan::profiles::ContentLanguage;
using niratan::profiles::HoshiProfile;
using niratan::profiles::ProfileError;
using niratan::profiles::ProfileIndex;
using niratan::profiles::create_profile;
using niratan::profiles::default_japanese_profile;
using niratan::profiles::delete_profile;
using niratan::profiles::is_safe_profile_id;
using niratan::profiles::normalize_profile_index;
using niratan::profiles::rename_profile;
using niratan::profiles::set_active_profile;
using niratan::profiles::set_primary_profile;

// Core/ProfileRepository.swift and script/test_profile_repository.swift
// (testRepositoryMigrationAndPersistence, testProfileLifecycleAndPathSafety)
// define these rules; the cases follow both.
namespace {

HoshiProfile profile(std::string id, std::string name, std::string language,
                     bool is_default = false) {
    return HoshiProfile{std::move(id), std::move(name), std::move(language), is_default};
}

ProfileIndex two_profiles() {
    ProfileIndex index;
    index.profiles = {default_japanese_profile(), profile("english", "English", "en")};
    index.default_profile_id = "default-ja";
    index.global_active_profile_id = "english";
    index.primary_profile_ids_by_language = {{"ja", "default-ja"}, {"en", "english"}};
    return index;
}

}  // namespace

class ProfileOpsTest : public QObject {
    Q_OBJECT

   private slots:
    void unsafe_identifiers_are_rejected();
    void normalization_fills_an_empty_index();
    void normalization_drops_unsafe_profiles();
    void normalization_inserts_a_missing_built_in_profile();
    void normalization_repairs_the_built_in_profile();
    void normalization_chooses_the_default_and_active_profiles();
    void normalization_cleans_the_primary_profiles();
    void normalization_never_defaults_the_legacy_video_profile();
    void create_appends_and_fills_the_language_primary();
    void create_rejects_blank_names_and_unknown_sources();
    void rename_trims_and_rejects_blank_or_unknown();
    void the_default_profile_cannot_be_deleted();
    void deleting_removes_the_profile_and_its_references();
    void activation_requires_a_known_profile();
    void primary_requires_a_matching_language();
};

void ProfileOpsTest::unsafe_identifiers_are_rejected() {
    QVERIFY(is_safe_profile_id("default-ja"));
    QVERIFY(is_safe_profile_id("profile-123"));
    QVERIFY(!is_safe_profile_id(""));
    QVERIFY(!is_safe_profile_id("."));
    QVERIFY(!is_safe_profile_id(".."));
    QVERIFY(!is_safe_profile_id("a/b"));
    QVERIFY(!is_safe_profile_id("a\\b"));
}

void ProfileOpsTest::normalization_fills_an_empty_index() {
    QCOMPARE(normalize_profile_index(ProfileIndex{}), ProfileIndex::initial());
}

void ProfileOpsTest::normalization_drops_unsafe_profiles() {
    ProfileIndex index = two_profiles();
    index.profiles.push_back(profile("../escape", "Bad", "ja"));

    const ProfileIndex result = normalize_profile_index(index);

    QCOMPARE(result.profiles.size(), 2u);
    QCOMPARE(result.profiles.at(1).id, std::string("english"));
}

void ProfileOpsTest::normalization_inserts_a_missing_built_in_profile() {
    ProfileIndex index;
    index.profiles = {profile("english", "English", "en")};
    index.default_profile_id = "english";
    index.global_active_profile_id = "english";
    index.primary_profile_ids_by_language = {{"en", "english"}};

    const ProfileIndex result = normalize_profile_index(index);

    QCOMPARE(result.profiles.size(), 2u);
    QCOMPARE(result.profiles.front(), default_japanese_profile());
    QCOMPARE(result.profiles.at(1).id, std::string("english"));
}

void ProfileOpsTest::normalization_repairs_the_built_in_profile() {
    ProfileIndex index = ProfileIndex::initial();
    index.profiles.front().name = "Japanese EPUB";
    index.profiles.front().is_default = false;

    QCOMPARE(normalize_profile_index(index).profiles.front(), default_japanese_profile());

    index.profiles.front().name = "Japanese";
    index.profiles.front().is_default = false;
    QCOMPARE(normalize_profile_index(index).profiles.front(), default_japanese_profile());
}

void ProfileOpsTest::normalization_chooses_the_default_and_active_profiles() {
    ProfileIndex index = two_profiles();
    index.default_profile_id = "missing";
    index.global_active_profile_id = "missing";

    const ProfileIndex result = normalize_profile_index(index);

    QCOMPARE(result.default_profile_id, std::string("default-ja"));
    QCOMPARE(result.global_active_profile_id, std::string("default-ja"));

    index.default_profile_id = "english";
    index.global_active_profile_id = "missing";
    const ProfileIndex second = normalize_profile_index(index);
    QCOMPARE(second.default_profile_id, std::string("english"));
    QCOMPARE(second.global_active_profile_id, std::string("english"));
}

void ProfileOpsTest::normalization_cleans_the_primary_profiles() {
    ProfileIndex index = two_profiles();
    index.primary_profile_ids_by_language = {
        {"fr", "english"},
        {"en", "missing"},
    };

    const ProfileIndex result = normalize_profile_index(index);

    const std::map<std::string, std::string, std::less<>> expected{{"ja", "default-ja"}};
    QCOMPARE(result.primary_profile_ids_by_language, expected);
}

void ProfileOpsTest::normalization_never_defaults_the_legacy_video_profile() {
    ProfileIndex index = ProfileIndex::initial();
    index.profiles.push_back(profile("default-ja-video", "Japanese Video", "ja", true));
    index.default_profile_id = "default-ja-video";

    const ProfileIndex result = normalize_profile_index(index);

    QCOMPARE(result.profiles.back().is_default, false);
    QCOMPARE(result.default_profile_id, std::string("default-ja"));
}

void ProfileOpsTest::create_appends_and_fills_the_language_primary() {
    ProfileIndex index = ProfileIndex::initial();
    index.primary_profile_ids_by_language = {{"ja", "default-ja"}};

    const HoshiProfile created = profile("profile-en", "  English  ", "en");
    const auto error = create_profile(index, created, std::nullopt);

    QVERIFY(!error.has_value());
    QCOMPARE(index.profiles.size(), 2u);
    QCOMPARE(index.profiles.back().name, std::string("English"));
    QCOMPARE(index.primary_profile_ids_by_language.at("en"), std::string("profile-en"));

    const HoshiProfile second = profile("profile-ja", "Second", "ja");
    QVERIFY(!create_profile(index, second, "profile-en").has_value());
    QCOMPARE(index.primary_profile_ids_by_language.at("ja"), std::string("default-ja"));
}

void ProfileOpsTest::create_rejects_blank_names_and_unknown_sources() {
    ProfileIndex index = ProfileIndex::initial();
    const ProfileIndex before = index;

    const auto blank = create_profile(index, profile("a", "\u3000 \n", "ja"), std::nullopt);
    QCOMPARE(blank, std::optional<ProfileError>{ProfileError::blank_name});
    QCOMPARE(index, before);

    const auto unknown =
        create_profile(index, profile("b", "B", "ja"), std::string("missing"));
    QCOMPARE(unknown, std::optional<ProfileError>{ProfileError::unknown_profile});
    QCOMPARE(index, before);
}

void ProfileOpsTest::rename_trims_and_rejects_blank_or_unknown() {
    ProfileIndex index = two_profiles();

    QVERIFY(!rename_profile(index, "english", "  English JH  ").has_value());
    QCOMPARE(index.profiles.at(1).name, std::string("English JH"));

    QCOMPARE(rename_profile(index, "english", " "),
             std::optional<ProfileError>{ProfileError::blank_name});
    QCOMPARE(rename_profile(index, "missing", "Name"),
             std::optional<ProfileError>{ProfileError::unknown_profile});

    QCOMPARE(rename_profile(index, "default-ja", "Japanese EPUB"),
             std::optional<ProfileError>{});
    QCOMPARE(index.profiles.front().name, std::string("Japanese"));
}

void ProfileOpsTest::the_default_profile_cannot_be_deleted() {
    ProfileIndex index = two_profiles();

    QCOMPARE(delete_profile(index, "default-ja"),
             std::optional<ProfileError>{ProfileError::cannot_delete_default_profile});
    QCOMPARE(delete_profile(index, "missing"),
             std::optional<ProfileError>{ProfileError::unknown_profile});
}

void ProfileOpsTest::deleting_removes_the_profile_and_its_references() {
    ProfileIndex index = two_profiles();

    QVERIFY(!delete_profile(index, "english").has_value());

    QCOMPARE(index.profiles.size(), 1u);
    QCOMPARE(index.global_active_profile_id, std::string("default-ja"));
    const std::map<std::string, std::string, std::less<>> expected{{"ja", "default-ja"}};
    QCOMPARE(index.primary_profile_ids_by_language, expected);
}

void ProfileOpsTest::activation_requires_a_known_profile() {
    ProfileIndex index = two_profiles();

    QVERIFY(!set_active_profile(index, "default-ja").has_value());
    QCOMPARE(index.global_active_profile_id, std::string("default-ja"));
    QCOMPARE(set_active_profile(index, "missing"),
             std::optional<ProfileError>{ProfileError::unknown_profile});
}

void ProfileOpsTest::primary_requires_a_matching_language() {
    ProfileIndex index = two_profiles();

    QCOMPARE(set_primary_profile(index, "default-ja", ContentLanguage::english),
             std::optional<ProfileError>{ProfileError::language_mismatch});

    QCOMPARE(set_primary_profile(index, "default-ja", ContentLanguage::japanese),
             std::optional<ProfileError>{});
    QCOMPARE(set_primary_profile(index, "english", ContentLanguage::japanese),
             std::optional<ProfileError>{ProfileError::language_mismatch});
    QCOMPARE(set_primary_profile(index, "missing", ContentLanguage::english),
             std::optional<ProfileError>{ProfileError::unknown_profile});
}

QTEST_APPLESS_MAIN(ProfileOpsTest)
#include "tst_profile_ops.moc"

// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/profiles/profile_index_json.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

#include <optional>
#include <string>

using niratan::profiles::HoshiProfile;
using niratan::profiles::ProfileIndex;
using niratan::profiles::profile_index_from_json;
using niratan::profiles::profile_index_to_json;

// Models/Profile.swift (Codable shapes) and Core/ProfileRepository.swift
// (persistIndex) define the document; the cases follow that shape.
namespace {

ProfileIndex custom_index() {
    ProfileIndex index;
    index.profiles = {HoshiProfile{"default-ja", "Japanese", "ja", true},
                      HoshiProfile{"english", "English", "en", false}};
    index.default_profile_id = "default-ja";
    index.global_active_profile_id = "english";
    index.primary_profile_ids_by_language = {{"ja", "default-ja"}, {"en", "english"}};
    return index;
}

std::optional<ProfileIndex> from_text(const char* text) {
    return profile_index_from_json(QJsonDocument::fromJson(QByteArray(text)));
}

}  // namespace

class ProfileIndexJsonTest : public QObject {
    Q_OBJECT

   private slots:
    void the_initial_index_round_trips();
    void a_custom_index_round_trips();
    void the_document_uses_the_swift_keys();
    void a_missing_or_wrong_key_is_rejected();
    void extra_keys_are_ignored();
};

void ProfileIndexJsonTest::the_initial_index_round_trips() {
    const ProfileIndex index = ProfileIndex::initial();
    const auto decoded = profile_index_from_json(profile_index_to_json(index));

    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, index);
}

void ProfileIndexJsonTest::a_custom_index_round_trips() {
    const ProfileIndex index = custom_index();
    const auto decoded = profile_index_from_json(profile_index_to_json(index));

    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, index);
}

void ProfileIndexJsonTest::the_document_uses_the_swift_keys() {
    const QJsonObject object = profile_index_to_json(custom_index()).object();

    QVERIFY(object.contains("profiles"));
    QVERIFY(object.contains("defaultProfileId"));
    QVERIFY(object.contains("globalActiveProfileId"));
    QVERIFY(object.contains("primaryProfileIdsByLanguage"));

    const QJsonObject profile = object.value("profiles").toArray().at(0).toObject();
    QCOMPARE(profile.value("id").toString(), QString("default-ja"));
    QCOMPARE(profile.value("name").toString(), QString("Japanese"));
    QCOMPARE(profile.value("dictionaryLanguageId").toString(), QString("ja"));
    QCOMPARE(profile.value("isDefault").toBool(), true);
}

void ProfileIndexJsonTest::a_missing_or_wrong_key_is_rejected() {
    QVERIFY(!from_text("{}").has_value());
    QVERIFY(!from_text("[]").has_value());
    QVERIFY(!from_text(
                 "{\"profiles\":[],\"defaultProfileId\":\"a\","
                 "\"globalActiveProfileId\":\"a\"}")
                 .has_value());
    QVERIFY(!from_text(
                 "{\"profiles\":[{\"id\":\"a\",\"name\":\"A\","
                 "\"dictionaryLanguageId\":\"ja\"}],"
                 "\"defaultProfileId\":\"a\",\"globalActiveProfileId\":\"a\","
                 "\"primaryProfileIdsByLanguage\":{}}")
                 .has_value());
    QVERIFY(!from_text(
                 "{\"profiles\":[{\"id\":1,\"name\":\"A\","
                 "\"dictionaryLanguageId\":\"ja\",\"isDefault\":false}],"
                 "\"defaultProfileId\":\"a\",\"globalActiveProfileId\":\"a\","
                 "\"primaryProfileIdsByLanguage\":{}}")
                 .has_value());
}

void ProfileIndexJsonTest::extra_keys_are_ignored() {
    const auto index = from_text(
        "{\"profiles\":[{\"id\":\"a\",\"name\":\"A\","
        "\"dictionaryLanguageId\":\"ja\",\"isDefault\":false,\"extra\":1}],"
        "\"defaultProfileId\":\"a\",\"globalActiveProfileId\":\"a\","
        "\"primaryProfileIdsByLanguage\":{},\"unexpected\":true}");

    QVERIFY(index.has_value());
    QCOMPARE(index->profiles.size(), 1u);
    QCOMPARE(index->profiles.front().id, std::string("a"));
}

QTEST_APPLESS_MAIN(ProfileIndexJsonTest)
#include "tst_profile_index_json.moc"

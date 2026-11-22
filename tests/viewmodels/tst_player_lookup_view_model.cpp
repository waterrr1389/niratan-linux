// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for PlayerLookupViewModel and ShiftHoverLookupState, following
// Features/Video/VideoLookupCoordinator.swift, Features/Popup/
// PopupPresentationCoordinator.swift (present, applyLookupMatch),
// Features/Video/Subtitles/VideoShiftHoverLookupState.swift and
// script/test_video_subtitle_selection.swift / test_video_shift_hover_lookup
// cases that reach the video path. Behaviours are named after
// notes/specs/viewmodels-player-lookup.md.
//
// A temporary data root holds one imported dictionary with the terms the
// clips need (decide, 星, 😀, don't) and one English profile; the Japanese
// profile's scan length is 8 for the emoji/leading-space candidate case.

#include "viewmodels/player/player_lookup_view_model.h"

#include "data_root.h"
#include "models/profiles/dictionary_settings.h"
#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"
#include "viewmodels/dictionary/dictionary_popup_model.h"
#include "viewmodels/player/player_lookup_policy.h"
#include "yomitan_fixture.h"

#include <QFile>
#include <QSignalSpy>
#include <QtTest>

#include <filesystem>
#include <memory>
#include <string>

using niratan::dictionary::DictionaryStore;
using niratan::profiles::ContentLanguage;
using niratan::profiles::default_dictionary_settings;
using niratan::profiles::dictionary_settings_to_json;
using niratan::profiles::ProfileRepository;
using niratan::test::minimal_index;
using niratan::test::TempDataRoot;
using niratan::test::write_stored_zip;
using niratan::viewmodels::DictionaryPopupModel;
using niratan::viewmodels::normalized_delay_milliseconds;
using niratan::viewmodels::PlayerLookupViewModel;
using niratan::viewmodels::ShiftHoverLookupState;

namespace {

QUrl subtitle_url() {
    return QUrl::fromLocalFile(QStringLiteral("/tmp/sub.srt"));
}

QByteArray srt(const QString& body) {
    return body.toUtf8();
}

void load(PlayerLookupViewModel& model, const QByteArray& bytes) {
    model.setSubtitleDocument(subtitle_url(), bytes, true);
}

QStringList ids(const QVariantList& cues) {
    QStringList result;
    for (const QVariant& entry : cues) {
        result.append(entry.toMap().value(QStringLiteral("id")).toString());
    }
    return result;
}

QByteArray term_bank() {
    return R"([["decide","","","v",0,["to make up one's mind"],1,""],)"
           R"(["星","ホシ","","n",0,["star"],2,""],)"
           R"(["😀","","","",0,["grin"],3,""],)"
           R"(["don't","","","",0,["do not"],4,""],)"
           R"(["前","","","",0,["before"],5,""],)"
           R"(["現在","","","",0,["now"],6,""],)"
           R"(["次","","","",0,["next"],7,""]]])";
}

// The single lookup path uses Japanese profile settings unless switched.
const char* const kJapaneseProfile = "default-ja";

bool use_profile(const std::string& profile_id) {
    ProfileRepository profiles{niratan::storage::data_root()};
    return !profiles.set_active_profile(profile_id).has_value();
}

}  // namespace

class PlayerLookupViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void b1_shift_hover_state_sequence();
    void b1_view_model_gates_the_hover_timer();
    void b2_delay_is_clamped_and_defaults_to_45();
    void b3_candidates_use_utf16_offsets();
    void b4_first_lookup_pauses_once_when_playing();
    void b5_replacement_does_not_pause_again();
    void b6_no_candidate_keeps_the_popup_and_no_result_closes_it();
    void b7_close_resumes_once();
    void b8_resume_uses_the_pause_flag();
    void b9_matched_text_shortens_the_highlight();
    void b10_current_cues_follow_position_and_delay();
    void b11_popup_model_is_reused_and_needs_a_result();
    void mining_selection_follows_the_active_cue();

   private:
    std::unique_ptr<TempDataRoot> root_;
    std::string english_profile_;
};

void PlayerLookupViewModelTest::initTestCase() {
    root_ = std::make_unique<TempDataRoot>();
    QVERIFY(root_->isValid());

    const auto zip = std::filesystem::path(root_->path().toStdString()) / "dictionary.zip";
    write_stored_zip(zip,
                     {{QStringLiteral("index.json"), minimal_index(QStringLiteral("Probe"))},
                      {QStringLiteral("term_bank_1.json"), term_bank()}});
    const auto dictionaries = niratan::storage::dictionaries_root();
    QVERIFY(!dictionaries.empty());
    const auto imported = DictionaryStore(dictionaries).import_zip(zip);
    QVERIFY2(imported.ok, imported.error.c_str());

    ProfileRepository profiles{niratan::storage::data_root()};
    const auto created = profiles.create_profile(QStringLiteral("English").toStdString(),
                                                 ContentLanguage::english, std::nullopt);
    QVERIFY(created.profile);
    english_profile_ = created.profile->id;

    // The emoji/leading-space candidate case expects scan length 8.
    const auto japanese_directory = profiles.profile_directory(kJapaneseProfile);
    QVERIFY(japanese_directory);
    std::error_code error;
    std::filesystem::create_directories(*japanese_directory, error);
    auto settings = default_dictionary_settings();
    settings.scan_length = 8;
    QFile settings_file(
        QString::fromStdString((*japanese_directory / "dictionary_settings.json").string()));
    QVERIFY(settings_file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    settings_file.write(dictionary_settings_to_json(settings).toJson());
    settings_file.close();
    QVERIFY(use_profile(kJapaneseProfile));
}

void PlayerLookupViewModelTest::b1_shift_hover_state_sequence() {
    ShiftHoverLookupState state;
    QVERIFY(!state.set_hover_point_available(true));
    QVERIFY(state.set_shift_pressed(true));
    QVERIFY(state.pointer_moved());
    QVERIFY(!state.set_shift_pressed(false));
    QVERIFY(!state.pointer_moved());
    QVERIFY(!state.set_hover_point_available(false));
    QVERIFY(!state.set_shift_pressed(true));
    QVERIFY(state.set_hover_point_available(true));
    state.cancel();
    QVERIFY(!state.pointer_moved());
    QVERIFY(!state.shift_pressed());
    QVERIFY(!state.hover_point_available());
}

void PlayerLookupViewModelTest::b1_view_model_gates_the_hover_timer() {
    PlayerLookupViewModel model;
    QVERIFY(!model.setShiftPressed(true));
    QVERIFY(model.setHoverPointAvailable(true));
    QVERIFY(model.pointerMoved());
    QVERIFY(!model.setShiftPressed(false));
    QVERIFY(!model.pointerMoved());
    QVERIFY(model.setShiftPressed(true));
    QVERIFY(model.pointerMoved());
    model.cancelShiftHover();
    QVERIFY(!model.pointerMoved());
}

void PlayerLookupViewModelTest::b2_delay_is_clamped_and_defaults_to_45() {
    QCOMPARE(normalized_delay_milliseconds(-20), 0);
    QCOMPARE(normalized_delay_milliseconds(45), 45);
    QCOMPARE(normalized_delay_milliseconds(2000), 1000);

    PlayerLookupViewModel model;
    QCOMPARE(model.hover_delay_ms(), 45);
    QSignalSpy delay_spy(&model, &PlayerLookupViewModel::hoverDelayChanged);
    model.setLookupSettings(true, 2000);
    QCOMPARE(model.hover_delay_ms(), 1000);
    QCOMPARE(delay_spy.count(), 1);
    model.setLookupSettings(true, -20);
    QCOMPARE(model.hover_delay_ms(), 0);
    QCOMPARE(delay_spy.count(), 2);
    model.setLookupSettings(true, 0);
    QCOMPARE(delay_spy.count(), 2);

    // Auto-pause defaults to on: a first popup pauses a playing video.
    QVERIFY(use_profile(kJapaneseProfile));
    PlayerLookupViewModel defaults;
    QSignalSpy pause(&defaults, &PlayerLookupViewModel::pauseRequested);
    load(defaults, srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n星\n")));
    defaults.updatePlayback(1.0, 0.0);
    defaults.setPlaying(true);
    QVERIFY(!defaults.lookupAt(QStringLiteral("1"), 0).isEmpty());
    QCOMPARE(pause.count(), 1);
}

void PlayerLookupViewModelTest::b3_candidates_use_utf16_offsets() {
    QVERIFY(use_profile(kJapaneseProfile));
    PlayerLookupViewModel model;
    load(model, srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n😀  星を見ます。\n")));
    model.updatePlayback(1.0, 0.0);

    const auto japanese = model.lookupAt(QStringLiteral("1"), 2);
    QVERIFY(!japanese.isEmpty());
    QCOMPARE(japanese.value(QStringLiteral("text")).toString(), QStringLiteral("星を見ます。"));
    QCOMPARE(japanese.value(QStringLiteral("utf16Start")).toInt(), 4);

    PlayerLookupViewModel emoji_model;
    load(emoji_model, srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n😀星\n")));
    emoji_model.updatePlayback(1.0, 0.0);
    const auto emoji = emoji_model.lookupAt(QStringLiteral("1"), 0);
    QVERIFY(!emoji.isEmpty());
    QCOMPARE(emoji.value(QStringLiteral("highlightStart")).toInt(), 0);
    QCOMPARE(emoji.value(QStringLiteral("highlightLength")).toInt(), 2);

    QVERIFY(use_profile(english_profile_));
    PlayerLookupViewModel english;
    load(english,
         srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n"
                            "We watched don't panic together.\n")));
    english.updatePlayback(1.0, 0.0);
    const auto map = english.lookupAt(QStringLiteral("1"), 14);
    QVERIFY(!map.isEmpty());
    QCOMPARE(map.value(QStringLiteral("text")).toString(), QStringLiteral("don't panic toge"));
    QCOMPARE(map.value(QStringLiteral("utf16Start")).toInt(), 11);
}

void PlayerLookupViewModelTest::b4_first_lookup_pauses_once_when_playing() {
    QVERIFY(use_profile(kJapaneseProfile));
    const QByteArray cue = srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n星\n"));

    PlayerLookupViewModel model;
    QSignalSpy pause(&model, &PlayerLookupViewModel::pauseRequested);
    QSignalSpy play(&model, &PlayerLookupViewModel::playRequested);
    model.setLookupSettings(true, 45);
    load(model, cue);
    model.updatePlayback(1.0, 0.0);
    model.setPlaying(true);
    bool popup_visible_at_pause = false;
    QObject::connect(&model, &PlayerLookupViewModel::pauseRequested, &model,
                     [&] { popup_visible_at_pause = model.has_popup(); });

    const auto map = model.lookupAt(QStringLiteral("1"), 0);
    QVERIFY(!map.isEmpty());
    QVERIFY(model.has_popup());
    QCOMPARE(model.active_cue_id(), QStringLiteral("1"));
    QCOMPARE(pause.count(), 1);
    QVERIFY(popup_visible_at_pause);
    QCOMPARE(play.count(), 0);

    PlayerLookupViewModel muted;
    QSignalSpy muted_pause(&muted, &PlayerLookupViewModel::pauseRequested);
    muted.setLookupSettings(false, 45);
    load(muted, cue);
    muted.updatePlayback(1.0, 0.0);
    muted.setPlaying(true);
    QVERIFY(!muted.lookupAt(QStringLiteral("1"), 0).isEmpty());
    QCOMPARE(muted_pause.count(), 0);

    PlayerLookupViewModel stopped;
    QSignalSpy stopped_pause(&stopped, &PlayerLookupViewModel::pauseRequested);
    QSignalSpy stopped_play(&stopped, &PlayerLookupViewModel::playRequested);
    stopped.setLookupSettings(true, 45);
    load(stopped, cue);
    stopped.updatePlayback(1.0, 0.0);
    stopped.setPlaying(false);
    QVERIFY(!stopped.lookupAt(QStringLiteral("1"), 0).isEmpty());
    QCOMPARE(stopped_pause.count(), 0);
    stopped.closeLookup();
    QCOMPARE(stopped_play.count(), 0);
}

void PlayerLookupViewModelTest::b5_replacement_does_not_pause_again() {
    QVERIFY(use_profile(kJapaneseProfile));
    PlayerLookupViewModel model;
    QSignalSpy pause(&model, &PlayerLookupViewModel::pauseRequested);
    QSignalSpy play(&model, &PlayerLookupViewModel::playRequested);
    model.setLookupSettings(true, 45);
    load(model,
         srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n星\n\n"
                            "2\n00:00:11,000 --> 00:00:20,000\n😀\n")));
    model.updatePlayback(1.0, 0.0);
    model.setPlaying(true);

    QVERIFY(!model.lookupAt(QStringLiteral("1"), 0).isEmpty());
    QCOMPARE(pause.count(), 1);
    QCOMPARE(model.active_cue_id(), QStringLiteral("1"));

    QVERIFY(!model.lookupAt(QStringLiteral("2"), 0).isEmpty());
    QCOMPARE(pause.count(), 1);
    QCOMPARE(model.active_cue_id(), QStringLiteral("2"));
    QCOMPARE(play.count(), 0);

    model.closeLookup();
    QCOMPARE(play.count(), 1);
}

void PlayerLookupViewModelTest::b6_no_candidate_keeps_the_popup_and_no_result_closes_it() {
    QVERIFY(use_profile(kJapaneseProfile));
    PlayerLookupViewModel model;
    QSignalSpy pause(&model, &PlayerLookupViewModel::pauseRequested);
    QSignalSpy play(&model, &PlayerLookupViewModel::playRequested);
    model.setLookupSettings(true, 45);
    load(model,
         srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n星\n\n"
                            "2\n00:00:11,000 --> 00:00:20,000\n😀\n\n"
                            "3\n00:00:21,000 --> 00:00:30,000\nxyzzy\n")));
    model.updatePlayback(1.0, 0.0);
    model.setPlaying(true);

    QVERIFY(!model.lookupAt(QStringLiteral("1"), 0).isEmpty());
    QCOMPARE(pause.count(), 1);
    auto* popup = qobject_cast<DictionaryPopupModel*>(model.popup_model());
    QVERIFY(popup);

    QVERIFY(model.lookupAt(QStringLiteral("1"), 999).isEmpty());
    QVERIFY(popup->has_results());
    QCOMPARE(popup->matched_text(), QStringLiteral("星"));
    QVERIFY(model.has_popup());
    QCOMPARE(model.active_cue_id(), QStringLiteral("1"));
    QCOMPARE(play.count(), 0);

    QVERIFY(model.lookupAt(QStringLiteral("3"), 0).isEmpty());
    QVERIFY(!model.has_popup());
    QVERIFY(model.active_cue_id().isEmpty());
    QCOMPARE(play.count(), 1);

    QVERIFY(model.lookupAt(QStringLiteral("3"), 0).isEmpty());
    QCOMPARE(play.count(), 1);
}

void PlayerLookupViewModelTest::b7_close_resumes_once() {
    QVERIFY(use_profile(kJapaneseProfile));
    PlayerLookupViewModel model;
    QSignalSpy pause(&model, &PlayerLookupViewModel::pauseRequested);
    QSignalSpy play(&model, &PlayerLookupViewModel::playRequested);
    model.setLookupSettings(true, 45);
    load(model, srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n星\n")));
    model.updatePlayback(1.0, 0.0);
    model.setPlaying(true);
    QVERIFY(!model.lookupAt(QStringLiteral("1"), 0).isEmpty());
    QCOMPARE(pause.count(), 1);

    model.closeLookup();
    QCOMPARE(play.count(), 1);
    QVERIFY(!model.has_popup());
    QVERIFY(model.active_cue_id().isEmpty());
    model.closeLookup();
    QCOMPARE(play.count(), 1);

    PlayerLookupViewModel idle;
    QSignalSpy idle_play(&idle, &PlayerLookupViewModel::playRequested);
    load(idle, srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n星\n")));
    idle.updatePlayback(1.0, 0.0);
    idle.closeLookup();
    QCOMPARE(idle_play.count(), 0);
}

void PlayerLookupViewModelTest::b8_resume_uses_the_pause_flag() {
    QVERIFY(use_profile(kJapaneseProfile));
    PlayerLookupViewModel model;
    QSignalSpy pause(&model, &PlayerLookupViewModel::pauseRequested);
    model.setLookupSettings(true, 45);
    load(model, srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n星\n")));
    model.updatePlayback(1.0, 0.0);
    model.setPlaying(true);
    QVERIFY(!model.lookupAt(QStringLiteral("1"), 0).isEmpty());
    QCOMPARE(pause.count(), 1);

    // The page reports playing again while the popup is open; closing still
    // plays once, because Swift plays whenever the pause was remembered.
    model.setPlaying(true);
    QSignalSpy play(&model, &PlayerLookupViewModel::playRequested);
    model.closeLookup();
    QCOMPARE(play.count(), 1);
}

void PlayerLookupViewModelTest::b9_matched_text_shortens_the_highlight() {
    QVERIFY(use_profile(english_profile_));
    PlayerLookupViewModel model;
    model.setLookupSettings(true, 45);
    load(model, srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\ndecided now\n")));
    model.updatePlayback(1.0, 0.0);
    model.setPlaying(false);

    const auto map = model.lookupAt(QStringLiteral("1"), 0);
    QVERIFY(!map.isEmpty());
    QCOMPARE(map.value(QStringLiteral("text")).toString(), QStringLiteral("decided now"));
    QCOMPARE(map.value(QStringLiteral("matchedText")).toString(), QStringLiteral("decided"));
    QCOMPARE(map.value(QStringLiteral("highlightStart")).toInt(), 0);
    QCOMPARE(map.value(QStringLiteral("highlightLength")).toInt(), 7);

    auto* popup = qobject_cast<DictionaryPopupModel*>(model.popup_model());
    QVERIFY(popup);
    QCOMPARE(popup->matched_text(), QStringLiteral("decided"));
    popup->clear();
    QVERIFY(popup->matched_text().isEmpty());
}

void PlayerLookupViewModelTest::b10_current_cues_follow_position_and_delay() {
    PlayerLookupViewModel model;
    load(model,
         srt(QStringLiteral("1\n00:00:00,500 --> 00:00:02,000\nA\n\n"
                            "2\n00:00:01,500 --> 00:00:03,000\nB\n\n"
                            "3\n00:00:06,000 --> 00:00:07,000\nC\n")));

    model.updatePlayback(2.0, 0.0);
    QCOMPARE(ids(model.current_cues()), (QStringList{"1", "2"}));
    const QVariantMap first = model.current_cues().front().toMap();
    QCOMPARE(first.value(QStringLiteral("start")).toDouble(), 0.5);
    QCOMPARE(first.value(QStringLiteral("end")).toDouble(), 2.0);
    QCOMPARE(first.value(QStringLiteral("text")).toString(), QStringLiteral("A"));

    model.updatePlayback(2.75, 1.0);
    QCOMPARE(ids(model.current_cues()), (QStringList{"1", "2"}));
    model.updatePlayback(2.75, 0.0);
    QCOMPARE(ids(model.current_cues()), QStringList{"2"});
    model.updatePlayback(6.0, 0.0);
    QCOMPARE(ids(model.current_cues()), QStringList{"3"});
    model.updatePlayback(31.5, 0.0);
    QVERIFY(model.current_cues().isEmpty());

    PlayerLookupViewModel long_cue;
    load(long_cue, srt(QStringLiteral("1\n00:00:00,000 --> 00:00:31,000\nlong\n")));
    long_cue.updatePlayback(30.0, 0.0);
    QCOMPARE(ids(long_cue.current_cues()), QStringList{"1"});
    long_cue.updatePlayback(31.0, 0.0);
    QCOMPARE(ids(long_cue.current_cues()), QStringList{"1"});
    long_cue.updatePlayback(31.5, 0.0);
    QVERIFY(long_cue.current_cues().isEmpty());

    model.setSubtitleDocument(QUrl{}, {}, false);
    QVERIFY(model.current_cues().isEmpty());
}

void PlayerLookupViewModelTest::b11_popup_model_is_reused_and_needs_a_result() {
    QVERIFY(use_profile(kJapaneseProfile));
    PlayerLookupViewModel model;
    QVERIFY(model.popup_model() != nullptr);
    auto* popup = qobject_cast<DictionaryPopupModel*>(model.popup_model());
    QVERIFY(popup);
    QVERIFY(!model.has_popup());

    model.setLookupSettings(true, 45);
    load(model, srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\n星\n")));
    model.updatePlayback(1.0, 0.0);
    model.setPlaying(false);
    QVERIFY(!model.lookupAt(QStringLiteral("1"), 0).isEmpty());
    QVERIFY(model.has_popup());
    QVERIFY(popup->has_results());
    QCOMPARE(model.popup_model(), static_cast<QObject*>(popup));
    QVERIFY(!model.lookupAt(QStringLiteral("1"), 0).isEmpty());
    QCOMPARE(model.popup_model(), static_cast<QObject*>(popup));

    PlayerLookupViewModel no_result;
    load(no_result, srt(QStringLiteral("1\n00:00:00,000 --> 00:00:10,000\nxyzzy\n")));
    no_result.updatePlayback(1.0, 0.0);
    no_result.setPlaying(false);
    QVERIFY(no_result.lookupAt(QStringLiteral("1"), 0).isEmpty());
    QVERIFY(!no_result.has_popup());
}

void PlayerLookupViewModelTest::mining_selection_follows_the_active_cue() {
    QVERIFY(use_profile(kJapaneseProfile));
    const QByteArray cues = srt(QStringLiteral("a\n00:00:01,000 --> 00:00:02,000\n前。\n\n"
                                               "b\n00:00:03,000 --> 00:00:04,000\n現在。\n\n"
                                               "c\n00:00:05,000 --> 00:00:06,000\n次。\n\n"
                                               "d\n00:00:07,000 --> 00:00:08,000\n次の次。\n"));

    PlayerLookupViewModel model;
    load(model, cues);
    model.updatePlayback(3.5, 0.0);
    QVERIFY(model.miningSelection().isEmpty());
    QVERIFY(!model.lookupAt(QStringLiteral("b"), 0).isEmpty());
    QCOMPARE(model.active_cue_id(), QStringLiteral("b"));
    const auto selection = model.miningSelection();
    QCOMPARE(selection.value("cueText").toString(), QString::fromUtf8("現在。"));
    QCOMPARE(selection.value("sentence").toString(), QString::fromUtf8("現在。"));
    QCOMPARE(selection.value("cueStart").toDouble(), 3.0);
    QCOMPARE(selection.value("cueEnd").toDouble(), 4.0);
    QCOMPARE(selection.value("previousCueText").toString(), QString::fromUtf8("前。"));
    QCOMPARE(selection.value("nextCueText").toString(), QString::fromUtf8("次。"));

    PlayerLookupViewModel first;
    load(first, cues);
    first.updatePlayback(1.5, 0.0);
    QVERIFY(!first.lookupAt(QStringLiteral("a"), 0).isEmpty());
    const auto first_selection = first.miningSelection();
    QCOMPARE(first_selection.value("cueText").toString(), QString::fromUtf8("前。"));
    QVERIFY(!first_selection.contains("previousCueText"));
    QCOMPARE(first_selection.value("nextCueText").toString(), QString::fromUtf8("現在。"));

    PlayerLookupViewModel loaded_without_active;
    load(loaded_without_active, cues);
    QVERIFY(loaded_without_active.miningSelection().isEmpty());
    PlayerLookupViewModel without_document;
    QVERIFY(without_document.miningSelection().isEmpty());
}

QTEST_MAIN(PlayerLookupViewModelTest)
#include "tst_player_lookup_view_model.moc"

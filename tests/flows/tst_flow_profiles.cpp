// SPDX-License-Identifier: GPL-3.0-or-later
//
// Flow 5: create an English profile, enable its dictionaries, look up a
// deinflected English word, mine it, and prove the Japanese profile is
// unchanged.
//
// The test drives the same view models and services the pages use, with a
// loopback AnkiConnect stub and a headless QWebEnginePage for the popup
// bridge:
//
// - Features/Settings/ProfilesView.swift (the create sheet and activate) and
//   Core/ProfileRepository.swift (createProfile, copyProfileOwnedFiles,
//   setGlobalActiveProfile), Core/ProfileActivationCoordinator.swift
//   (activateGlobal).
// - Features/Settings/DictionaryView.swift and Core/DictionaryManager.swift
//   (toggleDictionary, saveDictionaryConfig,
//   applyActiveProfileDictionaryConfiguration, activeLanguage,
//   refreshLookupQueryIfNeeded).
// - Features/Dictionary/DictionarySearchView.swift and
//   Features/Popup/PopupView.swift (buildContent, buildLookupEntries),
//   Core/LookupEngine.swift (buildQuery, lookup: the English deinflection
//   turn and the frequency ranking).
// - Features/Popup/PopupView.swift (mineEntry), Features/Popup/popup.js
//   (mineEntry, mineEntryAtIndex), Features/Popup/PopupWebView.swift
//   (Coordinator.userContentController) and Core/AnkiManager.swift
//   (mineAnkiEntry, addNoteAnkiConnect, duplicateLookup).
//
// Decision 4: activation only writes the index and the Anki effect; the
// dictionary and popup consumers re-resolve the active profile themselves, so
// the test never reassigns their state. The test does not open an EPUB; flow 1
// covers the reader page path.

#include "viewmodels/anki/mining_view_model.h"
#include "viewmodels/dictionary/dictionary_popup_model.h"
#include "viewmodels/dictionary/dictionary_settings_view_model.h"
#include "viewmodels/profiles/profiles_view_model.h"

#include "anki_stub.h"
#include "data_root.h"
#include "yomitan_fixture.h"

#include "models/dictionary/dictionary_config.h"
#include "services/anki/anki_config.h"
#include "services/dictionary/dictionary_config_store.h"
#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QSignalSpy>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <exception>
#include <filesystem>
#include <string>

using niratan::anki::AnkiConfigStore;
using niratan::dictionary::DictionaryStore;
using niratan::dictionary::load_dictionary_config;
using niratan::profiles::ProfileRepository;
using niratan::test::AnkiStub;
using niratan::test::minimal_index;
using niratan::test::minimal_term_bank;
using niratan::test::TempDataRoot;
using niratan::test::write_stored_zip;
using niratan::viewmodels::DictionaryPopupModel;
using niratan::viewmodels::DictionarySettingsViewModel;
using niratan::viewmodels::MiningViewModel;
using niratan::viewmodels::ProfilesViewModel;

namespace {

const QString kDictionaryTitle = QStringLiteral("Probe");
const QString kWord = QStringLiteral("decide");
const QString kInflected = QStringLiteral("decided");
const QString kSentence = QStringLiteral("I decided to go.");
const QString kBookTitle = QStringLiteral("English Book");
const QString kAddedIdText = QStringLiteral("9007199254740993");

// term_meta_bank_1.json: the frequency Probe stores for decide.
const QByteArray kTermMeta = QByteArrayLiteral(R"([["decide","freq",12345]])");

// One zip with the term bank and the frequency entry; the importer writes
// Term/Probe and Frequency/Probe.
bool install_dictionary(const std::filesystem::path& data_root) {
    const auto zip = data_root / "profile_dictionary.zip";
    try {
        write_stored_zip(zip,
                         {{"index.json", minimal_index(kDictionaryTitle)},
                          {"term_bank_1.json", minimal_term_bank()},
                          {"term_meta_bank_1.json", kTermMeta}});
    } catch (const std::exception&) {
        return false;
    }
    const auto imported = DictionaryStore(niratan::storage::dictionaries_root()).import_zip(zip);
    return imported.ok;
}

QVariantMap mining_payload(const QString& status, const QString& message,
                           const QString& note_id = {}) {
    QVariantMap payload{{"status", status}, {"message", message}};
    if (!note_id.isEmpty()) {
        payload.insert("noteID", note_id);
    }
    return payload;
}

QString evaluate(QWebEnginePage& page, const QString& script) {
    QString result;
    bool finished = false;
    QEventLoop loop;
    page.runJavaScript(script, [&](const QVariant& value) {
        result = value.toString();
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    return finished ? result : QString();
}

}  // namespace

// The bridge exposes the same methods ReaderLookupSurface.qml's
// dictionaryBridge does, so popup.js reaches the view model unchanged.
class MiningBridgeProbe : public QObject {
    Q_OBJECT

   public slots:
    void mineEntry(int id, const QVariant& data) { emit mineReceived(id, data); }
    void duplicateCheck(int id, const QVariant& word) { emit duplicateReceived(id, word); }
    void openAnkiNote(const QVariant& ids) { emit openReceived(ids); }
    void miningFeedback(const QVariant& data) { emit feedbackReceived(data); }

   signals:
    void mineReceived(int id, const QVariant& data);
    void duplicateReceived(int id, const QVariant& word);
    void openReceived(const QVariant& ids);
    void feedbackReceived(const QVariant& data);
    void replyReady(int id, const QVariant& payload);
};

class FlowProfilesTest : public QObject {
    Q_OBJECT

   private slots:
    void english_profile_lookup_and_mining_then_japanese_is_unchanged();

   private:
    AnkiStub anki_;
    MiningViewModel mining_;
    MiningBridgeProbe probe_;
};

// B1: creation from the active Japanese profile. B2: the English profile's
// dictionaries. B3: the English lookup and its frequency. B4: the mined card.
// B5: the unchanged Japanese profile and lookup.
void FlowProfilesTest::english_profile_lookup_and_mining_then_japanese_is_unchanged() {
    const std::filesystem::path data_root = niratan::storage::data_root();
    QVERIFY(!data_root.empty());
    QVERIFY(install_dictionary(data_root));

    ProfilesViewModel profiles_view;
    DictionaryPopupModel popup;

    // The default profile has no dictionary_config.json, so the no-config
    // fallback enables every installed dictionary and decide resolves.
    popup.lookup(kWord);
    QVERIFY(popup.has_results());
    QVERIFY(popup.error().isEmpty());
    const QVariantList japanese_entries = popup.getEntries(0, 1);
    QCOMPARE(japanese_entries.size(), qsizetype(1));

    const std::filesystem::path japanese_config =
        *ProfileRepository(data_root).profile_directory("default-ja") / "dictionary_config.json";
    QVERIFY(!std::filesystem::exists(japanese_config));

    // B1: the create sheet's activate copies nothing (the default owns no
    // file), stores the language and makes English active.
    QVERIFY(profiles_view.createProfile(QStringLiteral("English"), QStringLiteral("en")));
    QVERIFY(profiles_view.error().isEmpty());
    const QVariantMap english = profiles_view.profiles().last().toMap();
    QCOMPARE(english.value(QStringLiteral("name")).toString(), QStringLiteral("English"));
    QCOMPARE(english.value(QStringLiteral("language")).toString(), QStringLiteral("en"));
    QVERIFY(english.value(QStringLiteral("active")).toBool());
    const std::string english_id = english.value(QStringLiteral("id")).toString().toStdString();
    {
        ProfileRepository fresh{data_root};
        QCOMPARE(fresh.active_profile().id, english_id);
    }
    QVERIFY(!std::filesystem::exists(japanese_config));

    // B2: DictionaryView toggles one type at a time; each save resolves the
    // active profile, so only the English config appears.
    DictionarySettingsViewModel dictionary_settings;
    dictionary_settings.refresh();
    QCOMPARE(dictionary_settings.error(), QString());
    QCOMPARE(dictionary_settings.rows().size(), qsizetype(1));
    QCOMPARE(dictionary_settings.rows().first().toMap().value(QStringLiteral("title")).toString(),
             kDictionaryTitle);
    QVERIFY(!dictionary_settings.rows().first().toMap().value(QStringLiteral("enabled")).toBool());
    dictionary_settings.setEnabled(kDictionaryTitle, true);
    dictionary_settings.selectType(QStringLiteral("Frequency"));
    dictionary_settings.setEnabled(kDictionaryTitle, true);

    {
        ProfileRepository fresh{data_root};
        const auto directory = fresh.profile_directory(english_id);
        QVERIFY(directory.has_value());
        const auto config = load_dictionary_config(*directory / "dictionary_config.json");
        QVERIFY(config.has_value());
        QCOMPARE(config->term_dictionaries.size(), std::size_t(1));
        QCOMPARE(config->term_dictionaries.front().file_name, kDictionaryTitle.toStdString());
        QVERIFY(config->term_dictionaries.front().is_enabled);
        QCOMPARE(config->term_dictionaries.front().order, std::int64_t(0));
        QCOMPARE(config->frequency_dictionaries.size(), std::size_t(1));
        QCOMPARE(config->frequency_dictionaries.front().file_name, kDictionaryTitle.toStdString());
        QVERIFY(config->frequency_dictionaries.front().is_enabled);
        QCOMPARE(config->frequency_dictionaries.front().order, std::int64_t(0));
    }
    QVERIFY(!std::filesystem::exists(japanese_config));

    // The English profile's Anki configuration: the stub transport and the one
    // mapped field. The stub supplies canAdd, getMediaDirPath and addNote.
    {
        ProfileRepository repository{data_root};
        AnkiConfigStore store{data_root, repository};
        store.load();
        store.settings().transport->url = anki_.url();
        store.settings().selected_deck = "Default";
        store.settings().selected_note_type = "Lapis";
        store.settings().field_mappings = {{"Expression", "{expression}"}};
        QVERIFY(store.save().ok);
    }

    // B3: the English deinflection turn and the frequency ranking.
    popup.lookup(kInflected);
    QVERIFY(popup.has_results());
    QVERIFY(popup.error().isEmpty());
    const QVariantList decided = popup.getEntries(0, 1);
    QCOMPARE(decided.size(), qsizetype(1));
    const QVariantMap entry = decided.first().toMap();
    QCOMPARE(entry.value(QStringLiteral("expression")).toString(), kWord);

    bool past_step = false;
    for (const QVariant& candidate : entry.value(QStringLiteral("deinflectionTraces")).toList()) {
        for (const QVariant& step : candidate.toList()) {
            const QVariantMap step_map = step.toMap();
            if (step_map.value(QStringLiteral("name")).toString() == QStringLiteral("past") &&
                step_map.value(QStringLiteral("description")).toString() ==
                    QStringLiteral("Simple past tense of a verb")) {
                past_step = true;
            }
        }
    }
    QVERIFY(past_step);

    const QVariantList frequencies = entry.value(QStringLiteral("frequencies")).toList();
    QCOMPARE(frequencies.size(), qsizetype(1));
    const QVariantMap frequency_group = frequencies.first().toMap();
    QCOMPARE(frequency_group.value(QStringLiteral("dictionary")).toString(), kDictionaryTitle);
    const QVariantList ranks = frequency_group.value(QStringLiteral("frequencies")).toList();
    QCOMPARE(ranks.size(), qsizetype(1));
    const QVariantMap rank = ranks.first().toMap();
    QCOMPARE(rank.value(QStringLiteral("value")).toInt(), 12345);
    QCOMPARE(rank.value(QStringLiteral("displayValue")).toString(), QStringLiteral("12345"));

    // B4: the popup document and bridge, as ReaderLookupSurface.qml loads
    // them, with the dictionary page's mining context.
    mining_.setContext(kSentence, kBookTitle, QString(), QString());
    QWebEnginePage popup_page;
    QWebChannel channel(&popup_page);
    channel.registerObject(QStringLiteral("dictionaryBridge"), &probe_);
    popup_page.setWebChannel(&channel);
    connect(&probe_, &MiningBridgeProbe::mineReceived, this,
            [this](int id, const QVariant& data) { mining_.mineEntry(id, data); });
    connect(&probe_, &MiningBridgeProbe::duplicateReceived, this,
            [this](int id, const QVariant& word) { mining_.duplicateCheck(id, word); });
    connect(&probe_, &MiningBridgeProbe::openReceived, this,
            [this](const QVariant& ids) { mining_.openAnkiNote(ids); });
    connect(&probe_, &MiningBridgeProbe::feedbackReceived, this,
            [this](const QVariant& data) { mining_.miningFeedback(data); });
    connect(&mining_, &MiningViewModel::replyReady, &probe_, &MiningBridgeProbe::replyReady);

    QList<QVariantMap> mine_results;
    connect(&mining_, &MiningViewModel::replyReady, this,
            [&mine_results](int, const QVariant& payload) {
                const auto map = payload.toMap();
                if (map.contains(QStringLiteral("status"))) {
                    mine_results.append(map);
                }
            });
    // The popup checks duplicates for the rendered entry as soon as renderPopup
    // runs, which the bridge does after its handshake.
    QSignalSpy duplicates(&probe_, &MiningBridgeProbe::duplicateReceived);

    QSignalSpy loaded(&popup_page, &QWebEnginePage::loadFinished);
    popup_page.setHtml(popup.html());
    QVERIFY(loaded.wait(15000));
    QVERIFY(loaded.last().at(0).toBool());
    const auto options = QString::fromUtf8(
        QJsonDocument::fromVariant(mining_.popupMiningOptions()).toJson(QJsonDocument::Compact));
    evaluate(popup_page,
             QStringLiteral("window.hoshiMiningRequestBase = 0; window.hoshiMiningOptions = ") +
                 options + QStringLiteral(";\n") + popup.bridge_script());

    QString handler;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
        handler = evaluate(popup_page,
                           QStringLiteral("typeof window.webkit?.messageHandlers?.mineEntry"));
        if (handler == QStringLiteral("object")) {
            break;
        }
        QTest::qWait(50);
    }
    QCOMPARE(handler, QStringLiteral("object"));
    QCOMPARE(evaluate(popup_page, QStringLiteral("window.lookupEntries[0].expression")), kWord);

    QTRY_COMPARE_WITH_TIMEOUT(duplicates.size(), 1, 5000);
    QCOMPARE(duplicates.first().at(1).toString(), kWord);

    anki_.requests.clear();
    QCOMPARE(evaluate(popup_page, QStringLiteral("mineEntryAtIndex(0); 'sent';")),
             QStringLiteral("sent"));
    QTRY_COMPARE_WITH_TIMEOUT(mine_results.size(), 1, 10000);
    QCOMPARE(
        mine_results.first(),
        mining_payload(QStringLiteral("added"), QStringLiteral("Added to Anki."), kAddedIdText));

    QTRY_VERIFY_WITH_TIMEOUT(anki_.actions().contains(QStringLiteral("addNote")), 10000);
    const auto note =
        anki_.params(QStringLiteral("addNote")).value(QStringLiteral("note")).toObject();
    // PopupView.mineEntry with only {expression} mapped: the card carries the
    // entry's expression and nothing else; the exact object also proves the
    // tags, audio and picture keys are absent.
    QCOMPARE(note,
             (QJsonObject{{"deckName", QStringLiteral("Default")},
                          {"modelName", QStringLiteral("Lapis")},
                          {"options",
                           QJsonObject{{"allowDuplicate", false},
                                       {"duplicateScope", QStringLiteral("collection")}}},
                          {"fields", QJsonObject{{"Expression", kWord}}}}));

    // B5: back to the Japanese profile. Its config was never written and the
    // no-config fallback returns the same entry map as before.
    QVERIFY(profiles_view.activateProfile(QStringLiteral("default-ja")));
    {
        ProfileRepository fresh{data_root};
        QCOMPARE(fresh.active_profile().id, std::string("default-ja"));
    }
    popup.lookup(kWord);
    QVERIFY(popup.has_results());
    QVERIFY(popup.error().isEmpty());
    QCOMPARE(QJsonArray::fromVariantList(popup.getEntries(0, 1)),
             QJsonArray::fromVariantList(japanese_entries));
    QVERIFY(!std::filesystem::exists(japanese_config));
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    TempDataRoot data_root;
    if (!data_root.isValid()) {
        return 1;
    }
    QGuiApplication app(argc, argv);
    FlowProfilesTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_flow_profiles.moc"

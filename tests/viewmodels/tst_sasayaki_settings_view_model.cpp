// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Sasayaki panel's settings bridge and the app-level Enable and Show Skip
// switches, following Features/Sasayaki/SasayakiSheet.swift (settingsTab),
// Features/Settings/SasayakiSettingsView.swift and the didSet observers of
// enableSasayaki, sasayakiSkipControls, readerShowSasayakiToggle,
// sasayakiAutoScroll, sasayakiAutoPause and the four sasayaki colours in
// Core/UserConfig.swift. Each test runs in a temporary data root so the active
// profile and both settings documents are real files.

#include "viewmodels/sasayaki/sasayaki_settings_view_model.h"

#include <QJsonDocument>
#include <QSignalSpy>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "data_root.h"
#include "services/profiles/profile_repository.h"
#include "services/settings/global_settings.h"
#include "services/settings/settings_store.h"
#include "services/storage/app_paths.h"

using niratan::settings::GlobalSettings;
using niratan::settings::SettingsStore;
using niratan::settings::StoreError;
using niratan::test::TempDataRoot;
using niratan::viewmodels::SasayakiSettingsViewModel;

namespace {

std::filesystem::path settings_file() {
    return niratan::storage::data_root() / "settings.json";
}

std::filesystem::path reader_settings_file() {
    const niratan::profiles::ProfileRepository repository(niratan::storage::data_root());
    const auto directory = repository.profile_directory(repository.active_profile().id);
    return directory.has_value() ? *directory / "reader_settings.json" : std::filesystem::path{};
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

QJsonObject read_object(const std::filesystem::path& path) {
    return QJsonDocument::fromJson(QByteArray::fromStdString(read_text(path))).object();
}

QStringList sorted_keys(const QJsonObject& object) {
    QStringList keys = object.keys();
    keys.sort();
    return keys;
}

}  // namespace

class SasayakiSettingsViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void defaultsInFreshRoot();
    void enableAndSkipControls();
    void settersWriteTheirOwnKeys();
    void secondInstanceReadsStoredValues();
    void showToggleKeepsTheOtherReaderSettings();
    void changesEmitTheirSignals();
    void reloadReadsBothStores();
    void failedSaveLeavesValueAndBytesAlone();
};

// S1: a fresh data root reads module 3's defaults and the show toggle's false.
void SasayakiSettingsViewModelTest::defaultsInFreshRoot() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SasayakiSettingsViewModel model;
    QCOMPARE(model.show_sasayaki_toggle(), false);
    QCOMPARE(model.auto_scroll(), true);
    QCOMPARE(model.auto_pause(), true);
    QCOMPARE(model.text_color(), QStringLiteral("#000000"));
    QCOMPARE(model.background_color(), QStringLiteral("#87CFFA66"));
    QCOMPARE(model.dark_text_color(), QStringLiteral("#FFFFFF"));
    QCOMPARE(model.dark_background_color(), QStringLiteral("#87CFFA66"));
    QVERIFY(!std::filesystem::exists(settings_file()));
}

// B2: enableSasayaki and sasayakiSkipControls: UserConfig.init's `?? false`
// defaults, the two didSet observers each writing only their own key, and a
// stored value of another type reading as false.
void SasayakiSettingsViewModelTest::enableAndSkipControls() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SasayakiSettingsViewModel model;
    QCOMPARE(model.enable(), false);
    QCOMPARE(model.skip_controls(), false);
    QVERIFY(!std::filesystem::exists(settings_file()));

    QSignalSpy enable_spy(&model, &SasayakiSettingsViewModel::enableChanged);
    QSignalSpy skip_spy(&model, &SasayakiSettingsViewModel::skipControlsChanged);

    model.setEnable(true);
    QCOMPARE(model.enable(), true);
    QCOMPARE(read_object(settings_file()), (QJsonObject{{"enableSasayaki", true}}));

    model.setSkipControls(true);
    QCOMPARE(model.skip_controls(), true);
    QCOMPARE(read_object(settings_file()),
             (QJsonObject{{"enableSasayaki", true}, {"sasayakiSkipControls", true}}));
    QCOMPARE(enable_spy.count(), 1);
    QCOMPARE(skip_spy.count(), 1);

    SasayakiSettingsViewModel second;
    QCOMPARE(second.enable(), true);
    QCOMPARE(second.skip_controls(), true);

    QCOMPARE(GlobalSettings(settings_file()).save_enable_sasayaki(false), StoreError::none);
    model.reload();
    QCOMPARE(model.enable(), false);
    QCOMPARE(model.skip_controls(), true);

    write_text(settings_file(), R"({"enableSasayaki":"true","sasayakiSkipControls":1})");
    SasayakiSettingsViewModel typed;
    QCOMPARE(typed.enable(), false);
    QCOMPARE(typed.skip_controls(), false);
}

// S2: every setter writes exactly its own key and exposes the stored value,
// with colours normalized; the show toggle goes to the profile's reader
// settings, not the global document.
void SasayakiSettingsViewModelTest::settersWriteTheirOwnKeys() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SasayakiSettingsViewModel model;

    model.setAutoScroll(false);
    QCOMPARE(model.auto_scroll(), false);
    QCOMPARE(sorted_keys(read_object(settings_file())),
             QStringList{QStringLiteral("sasayakiAutoScroll")});

    model.setAutoPause(false);
    QCOMPARE(model.auto_pause(), false);
    QCOMPARE(
        sorted_keys(read_object(settings_file())),
        (QStringList{QStringLiteral("sasayakiAutoPause"), QStringLiteral("sasayakiAutoScroll")}));

    model.setTextColor(QStringLiteral("#abc"));
    QCOMPARE(model.text_color(), QStringLiteral("#AABBCC"));
    model.setBackgroundColor(QStringLiteral("#112233FF"));
    QCOMPARE(model.background_color(), QStringLiteral("#112233"));
    model.setDarkTextColor(QStringLiteral("ffff"));
    QCOMPARE(model.dark_text_color(), QStringLiteral("#FFFFFF"));
    model.setDarkBackgroundColor(QStringLiteral("#11223344"));
    QCOMPARE(model.dark_background_color(), QStringLiteral("#11223344"));

    const QJsonObject global = read_object(settings_file());
    QCOMPARE(sorted_keys(global),
             (QStringList{QStringLiteral("sasayakiAutoPause"), QStringLiteral("sasayakiAutoScroll"),
                          QStringLiteral("sasayakiBackgroundColor"),
                          QStringLiteral("sasayakiDarkBackgroundColor"),
                          QStringLiteral("sasayakiDarkTextColor"),
                          QStringLiteral("sasayakiTextColor")}));
    QCOMPARE(global.value(QStringLiteral("sasayakiAutoScroll")).toBool(), false);
    QCOMPARE(global.value(QStringLiteral("sasayakiAutoPause")).toBool(), false);
    QCOMPARE(global.value(QStringLiteral("sasayakiTextColor")).toString(),
             QStringLiteral("#AABBCC"));
    QCOMPARE(global.value(QStringLiteral("sasayakiBackgroundColor")).toString(),
             QStringLiteral("#112233"));
    QCOMPARE(global.value(QStringLiteral("sasayakiDarkTextColor")).toString(),
             QStringLiteral("#FFFFFF"));
    QCOMPARE(global.value(QStringLiteral("sasayakiDarkBackgroundColor")).toString(),
             QStringLiteral("#11223344"));
    QVERIFY(!global.contains(QStringLiteral("showSasayakiToggle")));

    model.setShowSasayakiToggle(true);
    QCOMPARE(model.show_sasayaki_toggle(), true);
    const QJsonObject reader = read_object(reader_settings_file());
    QCOMPARE(reader.value(QStringLiteral("showSasayakiToggle")).toBool(), true);
    QVERIFY(!read_object(settings_file()).contains(QStringLiteral("showSasayakiToggle")));
}

// S1/S2: stores are read at construction, so a second instance sees the files.
void SasayakiSettingsViewModelTest::secondInstanceReadsStoredValues() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    {
        SasayakiSettingsViewModel first;
        first.setAutoScroll(false);
        first.setAutoPause(false);
        first.setTextColor(QStringLiteral("#102030"));
        first.setBackgroundColor(QStringLiteral("#40506080"));
        first.setDarkTextColor(QStringLiteral("#FFFFFF"));
        first.setDarkBackgroundColor(QStringLiteral("#00000000"));
        first.setShowSasayakiToggle(true);
    }

    SasayakiSettingsViewModel second;
    QCOMPARE(second.auto_scroll(), false);
    QCOMPARE(second.auto_pause(), false);
    QCOMPARE(second.text_color(), QStringLiteral("#102030"));
    QCOMPARE(second.background_color(), QStringLiteral("#40506080"));
    QCOMPARE(second.dark_text_color(), QStringLiteral("#FFFFFF"));
    QCOMPARE(second.dark_background_color(), QStringLiteral("#00000000"));
    QCOMPARE(second.show_sasayaki_toggle(), true);
}

// S2: the show toggle keeps the profile's other reader settings.
void SasayakiSettingsViewModelTest::showToggleKeepsTheOtherReaderSettings() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    std::optional<SettingsStore> seed = SettingsStore::create();
    QVERIFY(seed.has_value());
    auto reader = seed->active_reader().settings;
    reader.theme = "Dark";
    reader.font_size = 30;
    reader.show_title = false;
    QCOMPARE(seed->save_active_reader(reader), StoreError::none);

    SasayakiSettingsViewModel model;
    model.setShowSasayakiToggle(true);

    std::optional<SettingsStore> reread = SettingsStore::create();
    QVERIFY(reread.has_value());
    const auto saved = reread->active_reader().settings;
    QCOMPARE(QString::fromStdString(saved.theme), QStringLiteral("Dark"));
    QCOMPARE(saved.font_size, 30);
    QCOMPARE(saved.show_title, false);
    QCOMPARE(saved.show_sasayaki_toggle, true);
}

// S2: every changed property emits its NOTIFY signal.
void SasayakiSettingsViewModelTest::changesEmitTheirSignals() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SasayakiSettingsViewModel model;
    QSignalSpy auto_spy(&model, &SasayakiSettingsViewModel::autoScrollChanged);
    QSignalSpy color_spy(&model, &SasayakiSettingsViewModel::textColorChanged);
    QSignalSpy toggle_spy(&model, &SasayakiSettingsViewModel::showSasayakiToggleChanged);

    model.setAutoScroll(false);
    model.setTextColor(QStringLiteral("#abc"));
    model.setShowSasayakiToggle(true);

    QCOMPARE(auto_spy.count(), 1);
    QCOMPARE(color_spy.count(), 1);
    QCOMPARE(toggle_spy.count(), 1);
}

// S1: reload() re-reads both stores.
void SasayakiSettingsViewModelTest::reloadReadsBothStores() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SasayakiSettingsViewModel model;
    QCOMPARE(model.auto_scroll(), true);
    QCOMPARE(model.show_sasayaki_toggle(), false);

    QCOMPARE(GlobalSettings(settings_file()).save_sasayaki_auto_scroll(false), StoreError::none);
    std::optional<SettingsStore> store = SettingsStore::create();
    QVERIFY(store.has_value());
    auto reader = store->active_reader();
    reader.settings.show_sasayaki_toggle = true;
    QCOMPARE(store->save_active_reader(reader.settings), StoreError::none);

    model.reload();
    QCOMPARE(model.auto_scroll(), false);
    QCOMPARE(model.auto_pause(), true);
    QCOMPARE(model.show_sasayaki_toggle(), true);
}

// S2: a failed save leaves the exposed value and the bytes unchanged.
void SasayakiSettingsViewModelTest::failedSaveLeavesValueAndBytesAlone() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const std::filesystem::path global_path = settings_file();
    const std::filesystem::path reader_path = reader_settings_file();
    std::filesystem::create_directories(global_path.parent_path());
    std::filesystem::create_directories(reader_path.parent_path());
    write_text(global_path, "{bad json");
    write_text(reader_path, "{bad reader");

    SasayakiSettingsViewModel model;
    QCOMPARE(model.auto_scroll(), true);
    QCOMPARE(model.text_color(), QStringLiteral("#000000"));
    QCOMPARE(model.show_sasayaki_toggle(), false);

    model.setAutoScroll(false);
    model.setTextColor(QStringLiteral("#abc"));
    model.setShowSasayakiToggle(true);
    QCOMPARE(model.auto_scroll(), true);
    QCOMPARE(model.text_color(), QStringLiteral("#000000"));
    QCOMPARE(model.show_sasayaki_toggle(), false);
    QCOMPARE(read_text(global_path), std::string("{bad json"));
    QCOMPARE(read_text(reader_path), std::string("{bad reader"));
}

QTEST_APPLESS_MAIN(SasayakiSettingsViewModelTest)
#include "tst_sasayaki_settings_view_model.moc"

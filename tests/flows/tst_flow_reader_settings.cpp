// SPDX-License-Identifier: GPL-3.0-or-later
//
// Flow test: a reader settings edit reaches an open book, following
// Features/Settings/AppearanceView.swift (the Layout card's Mouse Wheel Page
// Turn toggle and the Display card's Show Statistics Toggle, Show Reading
// Speed and Show Reading Time), Core/UserConfig.swift
// (readerWheelPageTurnEnabled, readerShowStatisticsToggle,
// readerShowReadingSpeed, readerShowReadingTime and readerProfileSettings()),
// NativeMac/NativeReaderView.swift (line 5087's registerWheelNavigation
// injection, which is false in continuous mode) and the Linux
// ReaderSettingsWatch, which reloads reader_settings.json into the open
// ReaderViewModel.
//
// The global statistics switch is written to settings.json first, as
// tests/flows/tst_flow_statistics.cpp does. No continuous scroll-speed setting
// exists in pinned Swift, so the document must not grow one (Decision D1).

#include "viewmodels/reader/reader_view_model.h"
#include "viewmodels/settings/settings_view_model.h"

#include "data_root.h"
#include "epub_fixture.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QTest>

#include <filesystem>
#include <string>

#include "services/storage/app_paths.h"

using niratan::viewmodels::ReaderViewModel;
using niratan::viewmodels::SettingsViewModel;

namespace {

const QString kTitle = QStringLiteral("読書設定の本");
const QString kFirstChapter = QStringLiteral("あいうえおかきくけこ");
const QString kSecondChapter = QStringLiteral("さしすせそたちつ");

bool write_statistics_enabled() {
    const auto root = niratan::storage::data_root();
    std::error_code error;
    std::filesystem::create_directories(root, error);
    const QJsonObject settings{
        {"enableStatistics", true},
        {"statisticsAutostartMode", "Off"},
        {"statisticsResetTime", 0},
        {"statisticsResetTimeMigratedToMinutes", true},
    };
    QFile file(QString::fromStdString((root / "settings.json").string()));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(QJsonDocument(settings).toJson()) > 0;
}

bool contains(const std::string& document, const char* text) {
    return document.find(text) != std::string::npos;
}

}  // namespace

class FlowReaderSettingsTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void settings_page_edits_reach_the_open_reader();

   private:
    niratan::test::TempDataRoot data_root_;
    ReaderViewModel* reader_ = nullptr;
};

void FlowReaderSettingsTest::initTestCase() {
    QVERIFY(data_root_.isValid());
    reader_ = ReaderViewModel::instance();
}

// R4, R6, R7: the settings page writes the four values, an open reader reloads
// them from the profile file, and the document's wheel flag follows the wheel
// setting and the continuous mode.
void FlowReaderSettingsTest::settings_page_edits_reach_the_open_reader() {
    QVERIFY(write_statistics_enabled());
    const QString source = data_root_.path() + "/reader-settings.epub";
    QVERIFY(niratan::test::write_epub(source.toStdString(), kTitle.toStdString(),
                                      kFirstChapter.toStdString(), kSecondChapter.toStdString()));
    QVERIFY(reader_->importAndOpen(QUrl::fromLocalFile(source)));
    QCOMPARE(reader_->chapter_count(), 2);

    SettingsViewModel settings;
    settings.setWheelPageTurnEnabled(false);
    settings.setShowStatisticsToggle(true);
    settings.setShowReadingSpeed(true);
    settings.setShowReadingTime(true);

    QTRY_COMPARE(reader_->show_statistics_toggle(), true);
    QTRY_COMPARE(reader_->show_reading_speed(), true);
    QTRY_COMPARE(reader_->show_reading_time(), true);
    QTRY_VERIFY(contains(reader_->document_for("first.xhtml"), "registerWheelNavigation?.(false)"));

    settings.setWheelPageTurnEnabled(true);
    QTRY_VERIFY(contains(reader_->document_for("first.xhtml"), "registerWheelNavigation?.(true)"));

    settings.setContinuousMode(true);
    QTRY_VERIFY(contains(reader_->document_for("first.xhtml"), "registerWheelNavigation?.(false)"));

    const std::string document = reader_->document_for("first.xhtml");
    QVERIFY(!contains(document, "scrollSpeed"));
    QVERIFY(!contains(document, "continuousSpeed"));

    reader_->closeBook();
}

QTEST_GUILESS_MAIN(FlowReaderSettingsTest)
#include "tst_flow_reader_settings.moc"

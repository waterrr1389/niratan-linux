// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader overlays' text, following NativeMac/NativeReaderView.swift
// (NativeReaderView.showsPageCount, progressString, progressLine and
// statisticsString), Features/Reader/Search/ReaderGoToView.swift
// (progressText and the Jump to alert's Go action: Int(jumpInput), count >= 0,
// rawCharacters(forDisplayCount:)) and Models/Profile.swift
// (ContentLanguageProfile.displayCount and rawCharacters).
//
// The pure functions run under a German default locale, so a decimal comma
// or a grouping dot in the output means host formatting leaked in; Swift
// writes plain Int interpolation, String(format:) percentages and grouping
// .never.
//
// The view model half opens a two-chapter book whose navigation document
// lists both chapters, so the table-of-contents chapter ranges are the ten
// and eight characters of the two spine items and differ from the book
// total of eighteen.

#include "viewmodels/reader/reader_presentation.h"
#include "viewmodels/reader/reader_view_model.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSignalSpy>
#include <QUrl>
#include <QUrlQuery>
#include <QtTest>

#include <zip.h>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "data_root.h"
#include "models/profiles/reader_settings.h"
#include "services/profiles/profile_repository.h"
#include "services/settings/settings_store.h"
#include "services/storage/app_paths.h"

namespace presentation = niratan::viewmodels::reader_presentation;
using niratan::viewmodels::ReaderViewModel;
using presentation::PageCount;
using presentation::ProgressInput;
using presentation::ProgressLineOptions;

class ReaderPresentationTextTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();

    void display_counts();
    void progress_line_counts_characters();
    void progress_line_counts_pages();
    void progress_line_omits_disabled_parts();
    void progress_string_joins_book_and_chapter_lines();
    void statistics_string_formats_speed_and_time();
    void position_text_uses_one_decimal();
    void parse_jump_count_follows_swift_int();

    void view_model_progress_text();
    void view_model_progress_text_at_the_bottom();
    void view_model_progress_text_before_pages_are_measured();
    void view_model_progress_text_counts_measured_pages();
    void view_model_statistics_text_ignores_tracking();
    void view_model_position_text();
    void view_model_jump_to_display_count();
    void view_model_statistics_text_is_empty_when_disabled();
    void view_model_english_jump_uses_raw_characters();

   private:
    bool save_reader(const niratan::profiles::ReaderProfileSettings& settings);
    void jump_and_restore(int character);
    void send_page(int index, int count);

    niratan::test::TempDataRoot data_root_;
    ReaderViewModel* reader_ = nullptr;
    QString folder_;
    std::optional<niratan::profiles::ReaderProfileSettings> saved_;
};

namespace {

const QString kTitle = QStringLiteral("表示の本");

bool write_nav_epub(const std::filesystem::path& path) {
    int error = 0;
    zip_t* archive = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (archive == nullptr) {
        return false;
    }
    const std::string opf =
        R"XML(<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
    <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
        <dc:title>表示の本</dc:title>
    </metadata>
    <manifest>
        <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
        <item id="first" href="first.xhtml" media-type="application/xhtml+xml"/>
        <item id="second" href="second.xhtml" media-type="application/xhtml+xml"/>
    </manifest>
    <spine>
        <itemref idref="first"/>
        <itemref idref="second"/>
    </spine>
</package>
)XML";
    const std::string nav =
        R"XML(<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><body>
<nav epub:type="toc"><ol>
<li><a href="first.xhtml">第一章</a></li>
<li><a href="second.xhtml">第二章</a></li>
</ol></nav>
</body></html>
)XML";
    const auto chapter = [](const std::string& text) {
        return "<html><body><p>" + text + "</p></body></html>";
    };
    const std::vector<std::pair<std::string, std::string>> entries{
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml",
         R"XML(<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0">
    <rootfiles>
        <rootfile full-path="book.opf" media-type="application/oebps-package+xml"/>
    </rootfiles>
</container>
)XML"},
        {"book.opf", opf},
        {"nav.xhtml", nav},
        {"first.xhtml", chapter("あいうえおかきくけこ")},
        {"second.xhtml", chapter("さしすせそたちつ")},
    };
    for (const auto& [name, bytes] : entries) {
        zip_source_t* source = zip_source_buffer(archive, bytes.data(), bytes.size(), 0);
        if (source == nullptr || zip_file_add(archive, name.c_str(), source, 0) < 0) {
            if (source != nullptr) {
                zip_source_free(source);
            }
            zip_discard(archive);
            return false;
        }
    }
    if (zip_close(archive) < 0) {
        zip_discard(archive);
        return false;
    }
    return true;
}

bool write_global_settings(bool statistics_enabled) {
    const auto root = niratan::storage::data_root();
    std::error_code error;
    std::filesystem::create_directories(root, error);
    const QJsonObject settings{
        {"enableStatistics", statistics_enabled},
        {"statisticsAutostartMode", "Off"},
        {"statisticsResetTime", 0},
        {"statisticsResetTimeMigratedToMinutes", true},
    };
    QFile file(QString::fromStdString((root / "settings.json").string()));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(QJsonDocument(settings).toJson()) > 0;
}

niratan::profiles::ReaderProfileSettings base_settings() {
    auto settings = niratan::profiles::default_reader_settings();
    settings.continuous_mode = false;
    settings.show_progress = true;
    settings.show_chapter_progress = true;
    settings.progress_count = "Characters";
    settings.show_percentage = true;
    settings.show_progress_top = true;
    settings.show_reading_speed = true;
    settings.show_reading_time = true;
    return settings;
}

QString revision(const ReaderViewModel& reader) {
    return QUrlQuery(QUrl(reader.chapter_url())).queryItemValue(QStringLiteral("v"));
}

void complete_restore(ReaderViewModel& reader) {
    reader.message(QStringLiteral("restoreCompleted"),
                   QString::fromUtf8(QJsonDocument(QJsonObject{{"token", revision(reader)}})
                                         .toJson(QJsonDocument::Compact)));
}

}  // namespace

void ReaderPresentationTextTest::initTestCase() {
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    QVERIFY(data_root_.isValid());
    QVERIFY(write_global_settings(true));
    QVERIFY(save_reader(base_settings()));

    const auto source = std::filesystem::path(data_root_.path().toStdString()) / "nav.epub";
    QVERIFY(write_nav_epub(source));
    reader_ = ReaderViewModel::instance();
    QVERIFY(reader_->importAndOpen(QUrl::fromLocalFile(QString::fromStdString(source.string()))));
    folder_ = reader_->book_folder();
    QVERIFY(!folder_.isEmpty());
    QCOMPARE(reader_->chapter_count(), 2);
    reader_->setReaderActive(true);
    complete_restore(*reader_);
    QVERIFY(!reader_->restoring());
}

bool ReaderPresentationTextTest::save_reader(
    const niratan::profiles::ReaderProfileSettings& settings) {
    auto store = niratan::settings::SettingsStore::create();
    if (!store) {
        return false;
    }
    // The model only reports a reload that changed something, and a changed
    // layout reloads the chapter, which the page would answer with a restore.
    const bool live = reader_ != nullptr && reader_->has_book() && saved_ != settings;
    saved_ = settings;
    QSignalSpy changed(ReaderViewModel::instance(), &ReaderViewModel::settingsChanged);
    if (store->save_active_reader(settings) != niratan::settings::StoreError::none) {
        return false;
    }
    if (live && !changed.wait(5000)) {
        return false;
    }
    if (reader_ != nullptr && reader_->restoring()) {
        complete_restore(*reader_);
    }
    return true;
}

void ReaderPresentationTextTest::jump_and_restore(int character) {
    reader_->jumpToCharacter(character);
    complete_restore(*reader_);
}

void ReaderPresentationTextTest::send_page(int index, int count) {
    reader_->message(QStringLiteral("pageChanged"),
                     QString::fromUtf8(QJsonDocument(QJsonObject{{"token", revision(*reader_)},
                                                                 {"pageIndex", index},
                                                                 {"pageCount", count}})
                                           .toJson(QJsonDocument::Compact)));
}

// ContentLanguageProfile.displayCount and rawCharacters.
void ReaderPresentationTextTest::display_counts() {
    QCOMPARE(presentation::display_count(0, false), 0);
    QCOMPARE(presentation::display_count(7, false), 7);
    QCOMPARE(presentation::display_count(-3, false), 0);
    QCOMPARE(presentation::display_count(0, true), 0);
    QCOMPARE(presentation::display_count(1, true), 1);
    QCOMPARE(presentation::display_count(5, true), 1);
    QCOMPARE(presentation::display_count(6, true), 2);
    QCOMPARE(presentation::display_count(-1, true), 0);

    QCOMPARE(presentation::raw_characters(12, false), 12);
    QCOMPARE(presentation::raw_characters(-4, false), 0);
    QCOMPARE(presentation::raw_characters(3, true), 15);
    QCOMPARE(presentation::raw_characters(-2, true), 0);
}

// progressLine: "current / total" in display units, then "%.2f%%".
void ReaderPresentationTextTest::progress_line_counts_characters() {
    const ProgressLineOptions characters{false, false, true, false};
    QCOMPARE(presentation::progress_line(5, 18, std::nullopt, characters),
             QStringLiteral("5 / 18 27.78%"));
    QCOMPARE(presentation::progress_line(1, 8, std::nullopt, characters),
             QStringLiteral("1 / 8 12.50%"));
    QCOMPARE(presentation::progress_line(0, 0, std::nullopt, characters),
             QStringLiteral("0 / 0 0.00%"));
    QCOMPARE(presentation::progress_line(1234567, 2345678, std::nullopt, characters),
             QStringLiteral("1234567 / 2345678 52.63%"));

    const ProgressLineOptions english{false, false, true, true};
    QCOMPARE(presentation::progress_line(12, 18, std::nullopt, english),
             QStringLiteral("3 / 4 66.67%"));
}

// showsPageCount writes "current of total", or "…" before measurement.
void ReaderPresentationTextTest::progress_line_counts_pages() {
    const ProgressLineOptions pages{true, false, true, false};
    QCOMPARE(presentation::progress_line(5, 18, PageCount{3, 10}, pages),
             QStringLiteral("3 of 10 27.78%"));
    QCOMPARE(presentation::progress_line(5, 18, std::nullopt, pages), QStringLiteral("… 27.78%"));

    const ProgressLineOptions pages_only{true, false, false, false};
    QCOMPARE(presentation::progress_line(5, 18, PageCount{3, 10}, pages_only),
             QStringLiteral("3 of 10"));
}

void ReaderPresentationTextTest::progress_line_omits_disabled_parts() {
    QCOMPARE(presentation::progress_line(5, 18, std::nullopt, {false, true, true, false}),
             QStringLiteral("27.78%"));
    QCOMPARE(presentation::progress_line(5, 18, std::nullopt, {false, true, false, false}),
             QString());
    QCOMPARE(presentation::progress_line(5, 18, std::nullopt, {false, false, false, false}),
             QStringLiteral("5 / 18"));
}

// progressString: book line, then "(chapter line)", space at the top and
// newline at the bottom, empty lines skipped.
void ReaderPresentationTextTest::progress_string_joins_book_and_chapter_lines() {
    ProgressInput input;
    input.line = {false, false, true, false};
    input.show_book = true;
    input.show_chapter = true;
    input.top = true;
    input.book_current = 5;
    input.book_total = 18;
    input.chapter_current = 5;
    input.chapter_total = 10;
    QCOMPARE(presentation::progress_string(input), QStringLiteral("5 / 18 27.78% (5 / 10 50.00%)"));

    input.top = false;
    QCOMPARE(presentation::progress_string(input),
             QStringLiteral("5 / 18 27.78%\n(5 / 10 50.00%)"));

    input.show_book = false;
    QCOMPARE(presentation::progress_string(input), QStringLiteral("(5 / 10 50.00%)"));

    input.show_book = true;
    input.show_chapter = false;
    QCOMPARE(presentation::progress_string(input), QStringLiteral("5 / 18 27.78%"));

    input.show_book = false;
    QCOMPARE(presentation::progress_string(input), QString());

    input.show_book = true;
    input.show_chapter = true;
    input.line = {false, true, false, false};
    QCOMPARE(presentation::progress_string(input), QString());

    input.top = true;
    input.line = {true, false, true, false};
    input.pages = presentation::PageProgress{3, 10, 1, 4};
    QCOMPARE(presentation::progress_string(input),
             QStringLiteral("3 of 10 27.78% (1 of 4 50.00%)"));
    input.pages.reset();
    QCOMPARE(presentation::progress_string(input), QStringLiteral("… 27.78% (… 50.00%)"));
}

// statisticsString: "<speed> / h" with grouping .never, then the hourMinute
// session time; nothing when statistics are disabled.
void ReaderPresentationTextTest::statistics_string_formats_speed_and_time() {
    QCOMPARE(presentation::statistics_string(false, true, true, 12345, 600, false), QString());
    QCOMPARE(presentation::statistics_string(true, false, false, 12345, 600, false), QString());
    QCOMPARE(presentation::statistics_string(true, true, false, 12345, 0, false),
             QStringLiteral("12345 / h"));
    QCOMPARE(presentation::statistics_string(true, true, false, 12345, 0, true),
             QStringLiteral("2469 / h"));
    QCOMPARE(presentation::statistics_string(true, true, true, 0, 0, false),
             QStringLiteral("0 / h 0:00"));

    QCOMPARE(presentation::statistics_string(true, false, true, 0, 29, false),
             QStringLiteral("0:00"));
    QCOMPARE(presentation::statistics_string(true, false, true, 0, 31, false),
             QStringLiteral("0:01"));
    QCOMPARE(presentation::statistics_string(true, false, true, 0, 90, false),
             QStringLiteral("0:02"));
    QCOMPARE(presentation::statistics_string(true, false, true, 0, 150, false),
             QStringLiteral("0:02"));
    QCOMPARE(presentation::statistics_string(true, false, true, 0, 7180, false),
             QStringLiteral("2:00"));
    QCOMPARE(presentation::statistics_string(true, false, true, 0, 12 * 3600 + 5 * 60, false),
             QStringLiteral("12:05"));
}

// ReaderGoToView.progressText.
void ReaderPresentationTextTest::position_text_uses_one_decimal() {
    QCOMPARE(presentation::position_text(0, 18, false), QStringLiteral("0 / 18 (0.0%)"));
    QCOMPARE(presentation::position_text(5, 18, false), QStringLiteral("5 / 18 (27.8%)"));
    QCOMPARE(presentation::position_text(0, 0, false), QStringLiteral("0 / 0 (0.0%)"));
    QCOMPARE(presentation::position_text(12, 18, true), QStringLiteral("3 / 4 (66.7%)"));
    QCOMPARE(presentation::position_text(1234567, 2345678, false),
             QStringLiteral("1234567 / 2345678 (52.6%)"));
}

// The Jump to alert: Int(jumpInput) and count >= 0.
void ReaderPresentationTextTest::parse_jump_count_follows_swift_int() {
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("12")), std::optional<int>(12));
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("0")), std::optional<int>(0));
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("+7")), std::optional<int>(7));
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("-0")), std::optional<int>(0));
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("007")), std::optional<int>(7));

    QCOMPARE(presentation::parse_jump_count(QString()), std::nullopt);
    QCOMPARE(presentation::parse_jump_count(QStringLiteral(" 5")), std::nullopt);
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("5 ")), std::nullopt);
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("-1")), std::nullopt);
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("1.5")), std::nullopt);
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("abc")), std::nullopt);
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("+")), std::nullopt);
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("-")), std::nullopt);
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("１２")), std::nullopt);
    QCOMPARE(presentation::parse_jump_count(QStringLiteral("99999999999999999999")), std::nullopt);
}

// progressString from the open book: the book total is 18 and the chapter
// line uses the table-of-contents range (10, then 8), not the book.
void ReaderPresentationTextTest::view_model_progress_text() {
    QVERIFY(save_reader(base_settings()));
    jump_and_restore(0);
    QCOMPARE(reader_->progress_text(), QStringLiteral("0 / 18 0.00% (0 / 10 0.00%)"));

    reader_->updateChapterProgress(0.5);
    QCOMPARE(reader_->progress_text(), QStringLiteral("5 / 18 27.78% (5 / 10 50.00%)"));

    jump_and_restore(12);
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->progress_text(), QStringLiteral("12 / 18 66.67% (2 / 8 25.00%)"));
}

void ReaderPresentationTextTest::view_model_progress_text_at_the_bottom() {
    auto settings = base_settings();
    settings.show_progress_top = false;
    QVERIFY(save_reader(settings));
    jump_and_restore(12);
    QCOMPARE(reader_->progress_text(), QStringLiteral("12 / 18 66.67%\n(2 / 8 25.00%)"));
}

void ReaderPresentationTextTest::view_model_progress_text_before_pages_are_measured() {
    auto settings = base_settings();
    settings.progress_count = "Pages";
    QVERIFY(save_reader(settings));
    jump_and_restore(12);
    QCOMPARE(reader_->progress_text(), QStringLiteral("… 66.67% (… 25.00%)"));

    settings.continuous_mode = true;
    QVERIFY(save_reader(settings));
    jump_and_restore(12);
    QCOMPARE(reader_->progress_text(), QStringLiteral("12 / 18 66.67% (2 / 8 25.00%)"));
}

// NativeReaderModel.pageProgress: both lines map one ReaderPageIndex.Progress
// built from measured pages only. The first spine reports page 2 of 3, then
// the second page 2 of 2: the book has five measured pages and the chapter
// lines follow the table-of-contents ranges, which here match the spines.
void ReaderPresentationTextTest::view_model_progress_text_counts_measured_pages() {
    auto settings = base_settings();
    settings.progress_count = "Pages";
    settings.continuous_mode = false;
    settings.show_progress_top = true;
    QVERIFY(save_reader(settings));

    jump_and_restore(5);
    send_page(2, 3);
    QCOMPARE(reader_->progress_text(), QStringLiteral("2 of 3 27.78% (2 of 3 50.00%)"));

    jump_and_restore(12);
    send_page(2, 2);
    QCOMPARE(reader_->progress_text(), QStringLiteral("5 of 5 66.67% (2 of 2 25.00%)"));
}

// statisticsString is gated by enableStatistics and the two show settings,
// not by tracking: the On autostart is off, so nothing is tracked here.
void ReaderPresentationTextTest::view_model_statistics_text_ignores_tracking() {
    QVERIFY(save_reader(base_settings()));
    QVERIFY(!reader_->tracking());
    QCOMPARE(reader_->statistics_text(), QStringLiteral("0 / h 0:00"));

    auto settings = base_settings();
    settings.show_reading_speed = false;
    QVERIFY(save_reader(settings));
    QCOMPARE(reader_->statistics_text(), QStringLiteral("0:00"));

    settings.show_reading_time = false;
    QVERIFY(save_reader(settings));
    QCOMPARE(reader_->statistics_text(), QString());
}

void ReaderPresentationTextTest::view_model_position_text() {
    QVERIFY(save_reader(base_settings()));
    jump_and_restore(12);
    QCOMPARE(reader_->position_text(), QStringLiteral("12 / 18 (66.7%)"));
    QCOMPARE(reader_->positionTextAt(5), QStringLiteral("5 / 18 (27.8%)"));
}

// Go: a valid count jumps to its raw character; anything else changes nothing.
void ReaderPresentationTextTest::view_model_jump_to_display_count() {
    jump_and_restore(0);
    QCOMPARE(reader_->character_offset(), 0);

    QVERIFY(!reader_->jumpToDisplayCount(QStringLiteral("abc")));
    QVERIFY(!reader_->jumpToDisplayCount(QStringLiteral("-1")));
    QVERIFY(!reader_->jumpToDisplayCount(QString()));
    QVERIFY(!reader_->restoring());
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->character_offset(), 0);

    QVERIFY(reader_->jumpToDisplayCount(QStringLiteral("12")));
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->character_offset(), 12);

    QVERIFY(reader_->jumpToDisplayCount(QStringLiteral("5")));
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->character_offset(), 5);
}

// Runs last: the statistics preference is read when a book opens.
void ReaderPresentationTextTest::view_model_statistics_text_is_empty_when_disabled() {
    QVERIFY(save_reader(base_settings()));
    QVERIFY(write_global_settings(false));
    reader_->closeBook();
    QVERIFY(reader_->openBook(folder_));
    complete_restore(*reader_);
    QVERIFY(!reader_->statistics_enabled());
    QCOMPARE(reader_->statistics_text(), QString());
    QVERIFY(!reader_->progress_text().isEmpty());
}

// Go converts the entered word count with rawCharacters(forDisplayCount:):
// under an English profile two words are ten raw characters.
void ReaderPresentationTextTest::view_model_english_jump_uses_raw_characters() {
    auto repository = niratan::profiles::ProfileRepository::at_data_root();
    QVERIFY(repository.has_value());
    const auto created = repository->create_profile(
        "English", niratan::profiles::ContentLanguage::english, std::nullopt);
    QVERIFY(created.profile.has_value());
    QVERIFY(!repository->set_active_profile(created.profile->id).has_value());
    QTRY_VERIFY(reader_->english_content());
    if (reader_->restoring()) {
        complete_restore(*reader_);
    }

    jump_and_restore(0);
    QVERIFY(reader_->jumpToDisplayCount(QStringLiteral("2")));
    complete_restore(*reader_);
    QCOMPARE(reader_->character_offset(), 10);
}

QTEST_GUILESS_MAIN(ReaderPresentationTextTest)

#include "tst_reader_presentation_text.moc"

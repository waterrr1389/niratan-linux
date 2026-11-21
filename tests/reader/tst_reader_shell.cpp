// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/reader_shell.h"

#include <QtTest>

#include <string>

using niratan::reader::Appearance;
using niratan::reader::Colors;
using niratan::reader::PageSize;
using niratan::reader::ReaderScripts;
using niratan::reader::ShellInput;
using niratan::reader::reader_document;
using niratan::reader::reader_script_block;
using niratan::reader::reader_style_block;

// Cases follow NativeReaderView.injectReader: the injected script, the
// viewport, the page size variables, the reader selection and the restore
// call.
namespace {

ShellInput input() {
    ShellInput shell;
    shell.appearance.selected_font = "Noto Serif JP";
    shell.page = PageSize{800, 600};
    shell.colors = Colors{"#111111", "#FAFAFA", "#222222", "#EEEEEE"};
    shell.content_language = "ja";
    shell.scan_length = 16;
    shell.hover_delay_ms = 250;
    shell.restore_token = "token-1";
    shell.initial_restore_expression = "window.hoshiReader.restoreProgress(0.25)";
    return shell;
}

ReaderScripts scripts() {
    ReaderScripts result;
    result.selection = "/* selection */";
    result.reader = "/* reader */";
    result.scrollreader = "/* scrollreader */";
    result.highlights = "/* highlights */";
    result.paragraph = "/* paragraph */";
    result.webchannel = "/* webchannel */";
    return result;
}

bool has(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

class ReaderShellTest : public QObject {
    Q_OBJECT

   private slots:
    void the_document_keeps_the_chapter_and_inserts_both_blocks();
    void missing_head_and_body_markers_still_receive_the_blocks();
    void the_page_size_and_viewport_are_set();
    void the_spread_size_applies_only_to_two_columns();
    void the_reader_selection_follows_the_mode();
    void paragraph_script_is_only_included_in_paragraph_mode();
    void the_restore_runs_after_the_fonts_are_ready();
    void the_bridge_shim_is_installed_before_the_scripts();
    void the_language_and_lookup_settings_are_passed();
};

void ReaderShellTest::the_document_keeps_the_chapter_and_inserts_both_blocks() {
    const std::string document =
        reader_document("<html><head><title>T</title></head><body><p>Text</p></body></html>",
                        input(), scripts());

    QVERIFY(has(document, "<p>Text</p>"));
    const std::size_t style = document.find("writing-mode: vertical-rl");
    const std::size_t head_end = document.find("</head>");
    QVERIFY(style != std::string::npos);
    QVERIFY(style < head_end);

    const std::size_t script = document.find("hoshiReaderRestoreToken");
    const std::size_t body_end = document.find("</body>");
    QVERIFY(script != std::string::npos);
    QVERIFY(script < body_end);
}

void ReaderShellTest::missing_head_and_body_markers_still_receive_the_blocks() {
    const std::string document = reader_document("<p>Bare</p>", input(), scripts());

    QVERIFY(has(document, "<p>Bare</p>"));
    QVERIFY(has(document, "writing-mode: vertical-rl"));
    QVERIFY(has(document, "hoshiReaderRestoreToken"));
}

void ReaderShellTest::the_page_size_and_viewport_are_set() {
    const std::string block = reader_script_block(input(), scripts());

    QVERIFY(has(block, "setProperty('--page-height', '600px')"));
    QVERIFY(has(block, "setProperty('--page-width', '800px')"));
    QVERIFY(has(block, "name = 'viewport'"));
    QVERIFY(has(block, "user-scalable=no"));
}

void ReaderShellTest::the_spread_size_applies_only_to_two_columns() {
    ShellInput shell = input();
    shell.appearance.vertical_writing = false;
    shell.appearance.two_column = true;
    QVERIFY(has(reader_script_block(shell, scripts()), "hoshiReader.horizontalPageColumns = 2;"));
    QVERIFY(has(reader_script_block(shell, scripts()),
                "hoshiReader.horizontalSpreadPageSize = 792;"));

    shell.appearance.paragraph_mode = true;
    QVERIFY(has(reader_script_block(shell, scripts()), "hoshiReader.horizontalPageColumns = 1;"));
    QVERIFY(has(reader_script_block(shell, scripts()),
                "hoshiReader.horizontalSpreadPageSize = 800;"));
}

void ReaderShellTest::the_reader_selection_follows_the_mode() {
    const ReaderScripts sources = scripts();
    const std::string paged = reader_script_block(input(), sources);
    QVERIFY(has(paged, "/* reader */"));
    QVERIFY(!has(paged, "/* scrollreader */"));

    ShellInput shell = input();
    shell.appearance.continuous = true;
    const std::string scrolling = reader_script_block(shell, sources);
    QVERIFY(has(scrolling, "/* scrollreader */"));
    QVERIFY(!has(scrolling, "/* reader */"));
}

void ReaderShellTest::paragraph_script_is_only_included_in_paragraph_mode() {
    const ReaderScripts sources = scripts();
    QVERIFY(!has(reader_script_block(input(), sources), "/* paragraph */"));

    ShellInput shell = input();
    shell.appearance.paragraph_mode = true;
    QVERIFY(has(reader_script_block(shell, sources), "/* paragraph */"));
}

void ReaderShellTest::the_restore_runs_after_the_fonts_are_ready() {
    const std::string block = reader_script_block(input(), scripts());
    const auto ready = block.find("Promise.all([document.fonts.ready, ...readyImages])");
    const auto restore = block.find("return window.hoshiReader.restoreProgress(0.25)");
    QVERIFY(ready != std::string::npos);
    QVERIFY(restore != std::string::npos);
    QVERIFY(ready < restore);
    QVERIFY(has(block, "setTimeout(revealFallback, 2500)"));

    ShellInput empty = input();
    empty.initial_restore_expression.clear();
    QVERIFY(!has(reader_script_block(empty, scripts()), "setTimeout(revealFallback"));
}

void ReaderShellTest::the_bridge_shim_is_installed_before_the_scripts() {
    const std::string block = reader_script_block(input(), scripts());

    const std::size_t shim = block.find("messageHandlers");
    const std::size_t webchannel = block.find("/* webchannel */");
    const std::size_t selection = block.find("/* selection */");
    QVERIFY(shim != std::string::npos);
    QVERIFY(webchannel != std::string::npos);
    QVERIFY(webchannel < shim);
    QVERIFY(shim < selection);
}

void ReaderShellTest::the_language_and_lookup_settings_are_passed() {
    ShellInput shell = input();
    shell.content_language = "en";
    shell.scan_length = 24;
    shell.hover_delay_ms = 400;
    shell.shared_sync_coordinates = true;
    const std::string block = reader_script_block(shell, scripts());

    QVERIFY(has(block, "hoshiSelection.language = 'en';"));
    QVERIFY(has(block, "registerShiftHoverLookup(24, 400)"));
    QVERIFY(has(block, "hoshiReader.sharedSyncCoordinates = true;"));
    QVERIFY(has(block, "scanNonJapaneseText = true;"));
}

QTEST_APPLESS_MAIN(ReaderShellTest)
#include "tst_reader_shell.moc"

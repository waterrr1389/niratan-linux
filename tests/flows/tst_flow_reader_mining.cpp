// SPDX-License-Identifier: GPL-3.0-or-later
//
// Flow 1: open an EPUB, look up a word in the reader and add an Anki card.
//
// The test drives the same view models and services the pages use, with a
// loopback AnkiConnect stub and a headless QWebEnginePage for the popup
// bridge, from the settings page to the addNote request:
//
// - Features/Settings/AnkiView.swift (Connect, Fetch, the Deck and Model
//   pickers, the field mapping menu and Tags) and Core/AnkiManager.swift
//   (loadTransport, refreshAnkiConnectStatus, fetchAnkiConnect,
//   applyFetchedAnkiMetadata, save).
// - NativeMac/NativeReaderView.swift (NativeReaderWebView.Coordinator's
//   textSelected, NativeReaderModel.title/coverURL and popupLayer),
//   Features/Popup/PopupView.swift (mineEntry and its document context),
//   Features/Popup/PopupWebView.swift (Coordinator.userContentController) and
//   Features/Popup/popup.js (miningContent, mineEntry and mineEntryAtIndex).
// - Core/AnkiManager.swift (duplicateLookup, mineAnkiEntry and
//   addNoteAnkiConnect) and Models/Anki.swift (DuplicateScope.ankiConnectOptions
//   and MiningContext).
//
// The reader page's wiring is mirrored where the views are not loaded:
// ReaderPage.qml's onLookupRequested sets the mining context from
// ReaderViewModel.documentTitle, coverUrl and coverPath and then runs the
// lookup; ReaderLookupSurface.qml's showResults only loads the generated
// document on htmlChanged, so no second context call is made.
//
// The imported EPUB comes from epub_fixture.h. Its cover and renamed library
// title are installed afterwards, so the EPUB title the card must carry
// differs from the library display title and the cover has local bytes.

#include "viewmodels/anki/anki_settings_view_model.h"
#include "viewmodels/anki/mining_view_model.h"
#include "viewmodels/dictionary/dictionary_popup_model.h"
#include "viewmodels/reader/reader_view_model.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QSignalSpy>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include "anki_stub.h"
#include "data_root.h"
#include "epub_fixture.h"
#include "yomitan_fixture.h"

#include "services/anki/anki_config.h"
#include "services/anki/duplicate_lookup.h"
#include "services/books/book_storage.h"
#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

using niratan::anki::AnkiConfigStore;
using niratan::books::BookStorage;
using niratan::dictionary::DictionaryStore;
using niratan::profiles::ProfileRepository;
using niratan::test::added_id;
using niratan::test::AnkiStub;
using niratan::test::minimal_index;
using niratan::test::write_stored_zip;
using niratan::viewmodels::AnkiSettingsViewModel;
using niratan::viewmodels::DictionaryPopupModel;
using niratan::viewmodels::MiningViewModel;
using niratan::viewmodels::ReaderViewModel;

namespace {

const QString kTitle = QStringLiteral("読書の本");
const QString kRenamedTitle = QStringLiteral("書棚の本");
const QString kSentence = QStringLiteral("今日は学校に行きます。");
const QString kWord = QStringLiteral("学校");
const QString kReading = QStringLiteral("がっこう");
const QString kDictionaryTitle = QStringLiteral("Probe");
const QString added_id_text = QStringLiteral("9007199254740993");
const QString pendingMessage = QString::fromUtf8("Preparing card…");
const QString findQuery = QStringLiteral("\"note:Lapis\" \"expression:学校\"");

const QByteArray kCover = QByteArray("cover\0bytes", 11);

QString revision(const ReaderViewModel& reader) {
    return QUrlQuery(QUrl(reader.chapter_url())).queryItemValue(QStringLiteral("v"));
}

void send(ReaderViewModel& reader, const QString& name, const QJsonObject& data) {
    reader.message(name, QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
}

bool install_dictionary(const std::filesystem::path& data_root) {
    const auto zip = data_root / "mining_dictionary.zip";
    try {
        write_stored_zip(zip,
                         {{"index.json", minimal_index(kDictionaryTitle)},
                          {"term_bank_1.json",
                           QByteArrayLiteral(R"([["学校","がっこう","","",0,["school"],1,""]])")}});
    } catch (const std::exception&) {
        return false;
    }
    const auto imported = DictionaryStore(niratan::storage::dictionaries_root()).import_zip(zip);
    return imported.ok;
}

// The imported EPUB has no cover, and its library title equals the EPUB title.
// Give it both a cover file and a renamed library title so the card's
// {book-cover} and {document-title} distinguish Swift's sources.
bool install_book_extras(const QString& data_root, const QString& folder) {
    const BookStorage storage(data_root.toStdString());
    const auto directory = storage.book_directory(folder.toStdString());
    if (!directory) {
        return false;
    }
    QFile cover(QString::fromStdString((*directory / "cover.png").string()));
    if (!cover.open(QIODevice::WriteOnly) || cover.write(kCover) != kCover.size()) {
        return false;
    }
    cover.close();
    auto metadata = storage.load_metadata(folder.toStdString());
    if (!metadata) {
        return false;
    }
    metadata->cover = "Books/" + folder.toStdString() + "/cover.png";
    metadata->renamed_title = kRenamedTitle.toStdString();
    return storage.save_metadata(*metadata);
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

class FlowReaderMiningTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void cleanupTestCase();
    void connect_reader_lookup_and_add_the_card();

   private:
    AnkiStub anki_;
    ReaderViewModel* reader_ = nullptr;
    DictionaryPopupModel popup_;
    MiningViewModel mining_;
    MiningBridgeProbe probe_;
};

void FlowReaderMiningTest::initTestCase() {
    reader_ = ReaderViewModel::instance();
    QVERIFY(install_dictionary(niratan::storage::data_root()));
    const auto source = niratan::storage::data_root() / "mining.epub";
    QVERIFY(niratan::test::write_epub(source, kTitle.toStdString(), kSentence.toStdString(),
                                      "二章目です。"));

    anki_.responses["version"] = {{"result", 6}};
    anki_.responses["deckNames"] = {{"result", QJsonArray{"Default", "Reading", "Mining"}}};
    anki_.responses["modelNames"] = {{"result", QJsonArray{"Basic", "Kiku", "Lapis"}}};
    anki_.model_field_responses["Basic"] = {{"result", QJsonArray{}}};
    anki_.model_field_responses["Kiku"] = {{"result", QJsonArray{"Word"}}};
    anki_.model_field_responses["Lapis"] = {
        {"result",
         QJsonArray{"Expression", "Sentence", "ExpressionReading", "Definition", "BookTitle",
                    "Cover"}}};
}

void FlowReaderMiningTest::cleanupTestCase() {
    if (reader_ != nullptr) {
        reader_->closeBook();
    }
}

// B1-B4: settings fetch and the saved picks. B5: the reader selection and the
// popup entries. B6: popup.js mineEntry content. B7: the duplicate preflight.
// B8: the addNote card. B9-B10: the reply, toast and saved word. B11: the
// second mine.
void FlowReaderMiningTest::connect_reader_lookup_and_add_the_card() {
    const std::filesystem::path data_root = niratan::storage::data_root();
    QVERIFY(!data_root.empty());

    // Anki settings: the stored transport points at the loopback stub.
    ProfileRepository profiles{data_root};
    {
        AnkiConfigStore store{data_root, profiles};
        store.settings().transport->url = anki_.url();
        QVERIFY(store.save().ok);
    }

    AnkiSettingsViewModel settings;
    anki_.requests.clear();
    settings.open();
    QTRY_VERIFY_WITH_TIMEOUT(settings.connected(), 3000);
    QCOMPARE(anki_.actions(), QStringList{"version"});

    anki_.requests.clear();
    settings.fetchMetadata();
    QTRY_VERIFY_WITH_TIMEOUT(!settings.busy(), 3000);
    QCOMPARE(anki_.actions(),
             (QStringList{"version", "deckNames", "modelNames", "modelFieldNames",
                          "modelFieldNames", "modelFieldNames"}));
    const QStringList models{"Basic", "Kiku", "Lapis"};
    for (qsizetype index = 0; index < models.size(); ++index) {
        QCOMPARE(anki_.requests.at(index + 3).value("params").toObject(),
                 (QJsonObject{{"modelName", models.at(index)}}));
    }
    QVERIFY(settings.error_message().isEmpty());
    QCOMPARE(settings.available_decks(), (QStringList{"Default", "Reading", "Mining"}));
    QCOMPARE(settings.available_note_types(), (QStringList{"Kiku", "Lapis"}));
    QCOMPARE(settings.selected_deck(), QStringLiteral("Reading"));
    QCOMPARE(settings.selected_note_type(), QStringLiteral("Kiku"));
    QCOMPARE(settings.fields(), QStringList{"Word"});

    // The user picks the deck and note type, maps every field and sets tags.
    settings.setSelectedDeck(QStringLiteral("Mining"));
    settings.setSelectedNoteType(QStringLiteral("Lapis"));
    QCOMPARE(settings.fields(),
             (QStringList{"Expression", "Sentence", "ExpressionReading", "Definition", "BookTitle",
                          "Cover"}));
    settings.setFieldMapping(QStringLiteral("Expression"), QStringLiteral("{expression}"));
    settings.setFieldMapping(QStringLiteral("Sentence"), QStringLiteral("{sentence}"));
    settings.setFieldMapping(QStringLiteral("ExpressionReading"), QStringLiteral("{reading}"));
    settings.setFieldMapping(QStringLiteral("Definition"), QStringLiteral("{glossary}"));
    settings.setFieldMapping(QStringLiteral("BookTitle"), QStringLiteral("{document-title}"));
    settings.setFieldMapping(QStringLiteral("Cover"), QStringLiteral("{book-cover}"));
    settings.setTags(QStringLiteral("日本語 読書"));
    settings.setDuplicateScope(QStringLiteral("collection"));
    settings.setCheckAllModels(false);
    settings.close();

    {
        AnkiConfigStore saved{data_root, profiles};
        saved.load();
        QCOMPARE(saved.settings().selected_deck, std::optional<std::string>("Mining"));
        QCOMPARE(saved.settings().selected_note_type, std::optional<std::string>("Lapis"));
        QCOMPARE(saved.settings().tags, std::string("日本語 読書"));
        QCOMPARE(saved.settings().field_mappings,
                 (decltype(saved.settings().field_mappings){{"Expression", "{expression}"},
                                                            {"Sentence", "{sentence}"},
                                                            {"ExpressionReading", "{reading}"},
                                                            {"Definition", "{glossary}"},
                                                            {"BookTitle", "{document-title}"},
                                                            {"Cover", "{book-cover}"}}));
    }

    // Reader: import the EPUB fixture, add its cover and library title, then
    // open it the way the reader page does.
    const QUrl source =
        QUrl::fromLocalFile(QString::fromStdString((data_root / "mining.epub").string()));
    QVERIFY(reader_->importAndOpen(source));
    const QString folder = reader_->book_folder();
    QVERIFY(!folder.isEmpty());
    reader_->closeBook();
    QVERIFY(install_book_extras(QString::fromStdString(data_root.string()), folder));
    QVERIFY(reader_->openBook(folder));
    reader_->setReaderActive(true);
    send(*reader_, QStringLiteral("restoreCompleted"), {{"token", revision(*reader_)}});
    QVERIFY(!reader_->restoring());
    QCOMPARE(reader_->title(), kRenamedTitle);
    QCOMPARE(reader_->document_title(), kTitle);
    QCOMPARE(reader_->cover_path(),
             QString::fromStdString(
                 (data_root / "Books" / folder.toStdString() / "cover.png").string()));
    QCOMPARE(reader_->cover_url(), QStringLiteral("http://localhost:18765/cover/cover.png"));

    QSignalSpy lookup(reader_, &ReaderViewModel::lookupRequested);
    const QJsonObject selection{
        {"token", revision(*reader_)},
        {"text", kWord},
        {"sentence", kSentence},
        {"rect", QJsonObject{{"x", 10}, {"y", 20}, {"width", 30}, {"height", 40}}},
        {"normalizedOffset", 3},
        {"miningContext",
         QJsonObject{{"currentIndex", 0},
                     {"sentences", QJsonArray{QJsonObject{{"id", "0"}, {"text", kSentence}}}}}}};
    send(*reader_, QStringLiteral("textSelected"), selection);
    QCOMPARE(lookup.size(), 1);
    QCOMPARE(lookup.first().at(0).toString(), kWord);
    QCOMPARE(lookup.first().at(1).toString(), kSentence);
    QCOMPARE(lookup.first().at(2).toRectF(), QRectF(10, 20, 30, 40));

    // ReaderPage.qml onLookupRequested: set the context from the reader, then
    // look the word up. ReaderLookupSurface.qml showResults then loads the
    // document on the model's htmlChanged, as the page's handler wires it.
    mining_.setContext(lookup.first().at(1).toString(), reader_->document_title(),
                       reader_->cover_url(), reader_->cover_path());
    popup_.lookup(lookup.first().at(0).toString());
    QVERIFY(popup_.has_results());
    QVERIFY(popup_.error().isEmpty());

    // The popup document and bridge, as ReaderLookupSurface.qml loads them.
    QWebEnginePage page;
    QWebChannel channel(&page);
    channel.registerObject(QStringLiteral("dictionaryBridge"), &probe_);
    page.setWebChannel(&channel);
    connect(&probe_, &MiningBridgeProbe::mineReceived, this,
            [this](int id, const QVariant& data) { mining_.mineEntry(id, data); });
    connect(&probe_, &MiningBridgeProbe::duplicateReceived, this,
            [this](int id, const QVariant& word) { mining_.duplicateCheck(id, word); });
    connect(&probe_, &MiningBridgeProbe::openReceived, this,
            [this](const QVariant& ids) { mining_.openAnkiNote(ids); });
    connect(&probe_, &MiningBridgeProbe::feedbackReceived, this,
            [this](const QVariant& data) { mining_.miningFeedback(data); });
    connect(&mining_, &MiningViewModel::replyReady, &probe_, &MiningBridgeProbe::replyReady);

    QList<QStringList> toasts;
    connect(&mining_, &MiningViewModel::toastChanged, this,
            [this, &toasts] { toasts.append({mining_.toast_status(), mining_.toast_message()}); });
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

    QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
    page.setHtml(popup_.html());
    QVERIFY(loaded.wait(15000));
    QVERIFY(loaded.last().at(0).toBool());
    const auto options = QString::fromUtf8(
        QJsonDocument::fromVariant(mining_.popupMiningOptions()).toJson(QJsonDocument::Compact));
    evaluate(page,
             QStringLiteral("window.hoshiMiningRequestBase = 0; window.hoshiMiningOptions = ") +
                 options + QStringLiteral(";\n") + popup_.bridge_script());

    QString handler;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
        handler =
            evaluate(page, QStringLiteral("typeof window.webkit?.messageHandlers?.mineEntry"));
        if (handler == QStringLiteral("object")) {
            break;
        }
        QTest::qWait(50);
    }
    QCOMPARE(handler, QStringLiteral("object"));
    QCOMPARE(evaluate(page, QStringLiteral("String(window.lookupEntries?.length)")),
             QStringLiteral("1"));
    QCOMPARE(evaluate(page, QStringLiteral("window.lookupEntries[0].expression")), kWord);
    QCOMPARE(evaluate(page, QStringLiteral("window.lookupEntries[0].reading")), kReading);
    QCOMPARE(evaluate(page, QStringLiteral("window.lookupEntries[0].matched")), kWord);
    QCOMPARE(evaluate(page, QStringLiteral("String(window.lookupEntries[0].glossaries.length)")),
             QStringLiteral("1"));
    QCOMPARE(
        evaluate(page,
                 QStringLiteral("document.querySelectorAll('#entries-container .entry').length")),
        QStringLiteral("1"));

    QTRY_COMPARE_WITH_TIMEOUT(duplicates.size(), 1, 5000);
    QCOMPARE(duplicates.first().at(1).toString(), kWord);

    // Mine 1: the popup builds the content and posts it through the bridge.
    anki_.requests.clear();
    QCOMPARE(evaluate(page, QStringLiteral("mineEntryAtIndex(0); 'sent';")),
             QStringLiteral("sent"));
    QTRY_COMPARE_WITH_TIMEOUT(mine_results.size(), 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(anki_.actions().size(), qsizetype(4), 10000);
    const auto added =
        mining_payload(QStringLiteral("added"), QStringLiteral("Added to Anki."), added_id_text);
    QCOMPARE(mine_results.first(), added);
    QVERIFY(toasts.size() >= 2);
    QCOMPARE(toasts.at(0), (QStringList{"pending", pendingMessage}));
    QCOMPARE(toasts.at(1), (QStringList{"added", QStringLiteral("Added to Anki.")}));
    QTRY_COMPARE_WITH_TIMEOUT(mining_.toast_status(), QStringLiteral("added"), 5000);
    QCOMPARE(mining_.toast_message(), QStringLiteral("Added to Anki."));
    QCOMPARE(mining_.toast_note_id(), added_id_text);

    QCOMPARE(anki_.actions(),
             (QStringList{"canAddNotesWithErrorDetail", "getMediaDirPath", "addNote",
                          "canAddNotesWithErrorDetail"}));
    QVERIFY(!anki_.requests.at(1).contains("params"));
    QCOMPARE(anki_.params(QStringLiteral("canAddNotesWithErrorDetail")),
             (QJsonObject{{"notes",
                           QJsonArray{QJsonObject{
                               {"deckName", QStringLiteral("Mining")},
                               {"modelName", QStringLiteral("Lapis")},
                               {"fields", QJsonObject{{"Expression", kWord}}},
                               {"options", QJsonObject{{"duplicateScope", "collection"}}}}}}}));
    QCOMPARE(anki_.params(QStringLiteral("addNote")).keys(), QStringList{"note"});

    // AnkiManager.addNoteAnkiConnect: the note is the exact card the reader
    // context, the entry content and the saved mapping produce.
    QCOMPARE(anki_.actions().count(QStringLiteral("addNote")), qsizetype(1));
    const auto note =
        anki_.params(QStringLiteral("addNote")).value(QStringLiteral("note")).toObject();
    QCOMPARE(note.value("deckName").toString(), QStringLiteral("Mining"));
    QCOMPARE(note.value("modelName").toString(), QStringLiteral("Lapis"));

    const QString glossary =
        QStringLiteral("<div style=\"text-align: left;\" class=\"yomitan-glossary\"><ol><li "
                       "data-dictionary=\"Probe\"><i>(1, Probe)</i> "
                       "<span>school</span></li></ol></div>");
    const auto fields = note.value("fields").toObject();
    QCOMPARE(fields.value("Expression").toString(), kWord);
    QCOMPARE(fields.value("ExpressionReading").toString(), kReading);
    QCOMPARE(fields.value("Definition").toString(), glossary);
    QVERIFY(!fields.contains(QStringLiteral("Cover")));
    QVERIFY(!note.contains(QStringLiteral("audio")));
    QCOMPARE(note.value("tags").toArray(), (QJsonArray{"日本語", "読書"}));
    QCOMPARE(
        note.value("options").toObject(),
        (QJsonObject{{"allowDuplicate", false}, {"duplicateScope", QStringLiteral("collection")}}));

    // A successful add records the word for the next duplicate check.
    QFile saved_words(QString::fromStdString(niratan::anki::saved_words_path(data_root).string()));
    QVERIFY(saved_words.open(QIODevice::ReadOnly));
    QCOMPARE(QJsonDocument::fromJson(saved_words.readAll()).array(), QJsonArray{kWord});

    // Mine 2: Anki now holds the card, so canAdd is false and findNotes
    // reports the note; the popup repeats the check after the mine. The
    // post-mine check of mine 1 must finish before its reply changes.
    QTRY_COMPARE_WITH_TIMEOUT(duplicates.size(), 2, 5000);
    const qsizetype add_notes_after_first = anki_.actions().count(QStringLiteral("addNote"));
    anki_.responses["canAddNotesWithErrorDetail"] = {
        {"result", QJsonArray{QJsonObject{{"canAdd", false}}}}};
    anki_.requests.clear();
    QCOMPARE(evaluate(page, QStringLiteral("mineEntryAtIndex(0); 'sent';")),
             QStringLiteral("sent"));
    QTRY_COMPARE_WITH_TIMEOUT(mine_results.size(), 2, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(anki_.actions().size(), qsizetype(4), 10000);
    QCOMPARE(mine_results.at(1),
             mining_payload(QStringLiteral("duplicate"), QStringLiteral("Already exists in Anki."),
                            added_id_text));
    QTRY_COMPARE_WITH_TIMEOUT(mining_.toast_status(), QStringLiteral("duplicate"), 5000);
    QCOMPARE(mining_.toast_note_id(), added_id_text);
    QCOMPARE(anki_.actions(),
             (QStringList{"canAddNotesWithErrorDetail", "findNotes", "canAddNotesWithErrorDetail",
                          "findNotes"}));
    int find_requests = 0;
    for (const auto& request : anki_.requests) {
        if (request.value("action").toString() == QStringLiteral("findNotes")) {
            ++find_requests;
            QCOMPARE(request.value("params").toObject().value("query").toString(), findQuery);
        }
    }
    QCOMPARE(find_requests, 2);
    QTRY_COMPARE_WITH_TIMEOUT(duplicates.size(), 3, 5000);
    QCOMPARE(add_notes_after_first, qsizetype(1));
    QCOMPARE(anki_.actions().count(QStringLiteral("addNote")), qsizetype(0));

    // The reader context: the selection's sentence, the EPUB title and the
    // book's cover reach the card, as NativeReaderView.popupLayer passes
    // documentTitle and coverURL and textSelected supplies the sentence. The
    // whole note is compared so one card shows every field at once.
    const auto sha1 =
        QString::fromLatin1(QCryptographicHash::hash(kCover, QCryptographicHash::Sha1).toHex());
    const QJsonObject picture{
        {"filename", QStringLiteral("hoshi_cover_") + sha1 + QStringLiteral(".png")},
        {"data", QString::fromLatin1(kCover.toBase64())},
        {"fields", QJsonArray{"Cover"}}};
    QCOMPARE(
        note,
        (QJsonObject{{"deckName", QStringLiteral("Mining")},
                     {"modelName", QStringLiteral("Lapis")},
                     {"fields",
                      QJsonObject{{"Expression", kWord},
                                  {"ExpressionReading", kReading},
                                  {"Definition", glossary},
                                  {"Sentence", QStringLiteral("今日は<b>学校</b>に行きます。")},
                                  {"BookTitle", kTitle}}},
                     {"picture", QJsonArray{picture}},
                     {"tags", QJsonArray{"日本語", "読書"}},
                     {"options",
                      QJsonObject{{"allowDuplicate", false},
                                  {"duplicateScope", QStringLiteral("collection")}}}}));
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    niratan::test::TempDataRoot data_root;
    if (!data_root.isValid()) {
        return 1;
    }
    QGuiApplication app(argc, argv);
    FlowReaderMiningTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_flow_reader_mining.moc"

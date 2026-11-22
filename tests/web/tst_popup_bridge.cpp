// SPDX-License-Identifier: GPL-3.0-or-later
//
// The script bridge between the generated dictionary popup and the C++ side,
// loaded the way DictionaryPage.qml does it: the popup shell from
// services/dictionary/popup_html, then the WebChannel library and bridge.js
// injected into the loaded document. A message posted through
// window.webkit.messageHandlers.playWordAudio must reach the registered
// dictionaryBridge object.

#include "services/dictionary/popup_assets.h"
#include "services/dictionary/popup_html.h"

#include <QGuiApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <string>

namespace {

QString evaluate(QWebEnginePage& page, const QString& script) {
    QString result;
    bool finished = false;
    QEventLoop loop;
    page.runJavaScript(script, [&](const QVariant& value) {
        result = value.toString();
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
    return finished ? result : QString();
}

// The shell renders one entry; only the bridge is under test, so the entry
// carries the fields the popup script reads and no glossaries.
QJsonArray entries_fixture() {
    QJsonObject entry;
    entry.insert(QStringLiteral("expression"), QStringLiteral("decide"));
    entry.insert(QStringLiteral("reading"), QStringLiteral("decide"));
    entry.insert(QStringLiteral("matched"), QStringLiteral("decide"));
    entry.insert(QStringLiteral("deinflectionTraces"), QJsonArray{});
    entry.insert(QStringLiteral("deinflectionTrace"), QJsonArray{});
    entry.insert(QStringLiteral("glossaries"), QJsonArray{});
    entry.insert(QStringLiteral("frequencies"), QJsonArray{});
    entry.insert(QStringLiteral("pitches"), QJsonArray{});
    entry.insert(QStringLiteral("rules"), QJsonArray{});
    return QJsonArray{entry};
}

}  // namespace

class DictionaryBridgeProbe : public QObject {
    Q_OBJECT

   public slots:
    void playWordAudio(const QString& url) { emit audioReceived(url); }

   signals:
    void audioReceived(const QString& url);
};

class MiningBridgeProbe : public QObject {
    Q_OBJECT

   public slots:
    void mineEntry(int id, const QVariant& data) { emit mineReceived(id, data); }
    void duplicateCheck(int id, const QVariant& word) { emit duplicateReceived(id, word); }
    void openAnkiNote(const QVariant& ids) { emit openReceived(ids); }
    void miningFeedback(const QVariant& data) { emit feedbackReceived(data); }
    void getEntries(int id, const QVariant& data) { emit entriesReceived(id, data); }
    void lookupRedirect(int id, const QVariant& query) { emit redirectReceived(id, query); }
    void queryTextSelected(int id, const QVariant& data) { emit querySelectedReceived(id, data); }
    void textSelected(const QVariant& data) { emit textSelectedReceived(data); }

   signals:
    void mineReceived(int id, const QVariant& data);
    void duplicateReceived(int id, const QVariant& word);
    void openReceived(const QVariant& ids);
    void feedbackReceived(const QVariant& data);
    void entriesReceived(int id, const QVariant& data);
    void redirectReceived(int id, const QVariant& query);
    void querySelectedReceived(int id, const QVariant& data);
    void textSelectedReceived(const QVariant& data);
    void replyReady(int id, const QVariant& payload);
};

namespace {

struct MiningPopup {
    MiningBridgeProbe probe;
    QWebEnginePage page;
    QWebChannel channel{&page};

    bool load(QObject* bridge = nullptr) {
        const auto assets = niratan::dictionary::load_popup_assets();
        if (assets.js.empty() || assets.web_channel.empty() || assets.bridge.empty() ||
            assets.selection.empty()) {
            return false;
        }
        const auto shell = niratan::dictionary::popup_shell_html(
            entries_fixture(), {}, niratan::dictionary::PopupConfig{}, assets.css, assets.selection,
            assets.js);
        channel.registerObject(QStringLiteral("dictionaryBridge"), bridge ? bridge : &probe);
        page.setWebChannel(&channel);
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setHtml(QString::fromStdString(shell));
        if (!loaded.wait(15000) || !loaded.last().at(0).toBool()) {
            return false;
        }
        js(QString::fromStdString(assets.web_channel) + "\n" +
           QString::fromStdString(assets.bridge));
        return true;
    }

    QString js(const QString& script) { return evaluate(page, script); }
};

}  // namespace

class PopupBridgeTest : public QObject {
    Q_OBJECT

   private slots:
    void play_word_audio_reaches_the_bridge();
    void play_entry_audio_uses_the_entry_reading();
    void mine_entry_posts_a_string_map_and_awaits_its_payload();
    void duplicate_check_awaits_its_payload_and_reveals_note_button();
    void open_anki_note_posts_the_button_ids();
    void mining_feedback_posts_pending_before_mining();
    void missing_mine_callback_resolves_with_failure();
    void the_shell_defines_the_selection_object();
    void inline_lookup_requests_reach_the_bridge();
};

void PopupBridgeTest::play_word_audio_reaches_the_bridge() {
    const auto assets = niratan::dictionary::load_popup_assets();
    QVERIFY(!assets.js.empty());
    QVERIFY(!assets.web_channel.empty());
    QVERIFY(!assets.bridge.empty());

    const std::string shell = niratan::dictionary::popup_shell_html(
        entries_fixture(), {}, niratan::dictionary::PopupConfig{}, assets.css, assets.selection,
        assets.js);

    QWebEnginePage page;
    QWebChannel channel(&page);
    page.setWebChannel(&channel);
    DictionaryBridgeProbe probe;
    channel.registerObject(QStringLiteral("dictionaryBridge"), &probe);
    QSignalSpy audio_spy(&probe, &DictionaryBridgeProbe::audioReceived);

    QSignalSpy load_spy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(QString::fromStdString(shell));
    QVERIFY(load_spy.wait(15000));
    QVERIFY(load_spy.last().at(0).toBool());

    evaluate(page,
             QString::fromStdString(assets.web_channel) + QStringLiteral("\n") +
                 QString::fromStdString(assets.bridge));

    // The bridge installs the handlers once the WebChannel handshake resolves.
    QElapsedTimer timer;
    timer.start();
    QString handler_type;
    while (timer.elapsed() < 10000) {
        handler_type =
            evaluate(page, QStringLiteral("typeof window.webkit?.messageHandlers?.playWordAudio"));
        if (handler_type == QStringLiteral("object")) {
            break;
        }
        QTest::qWait(50);
    }
    QCOMPARE(handler_type, QStringLiteral("object"));

    evaluate(page,
             QStringLiteral("window.webkit.messageHandlers.playWordAudio.postMessage("
                            "{url: 'https://example.test/audio.mp3'});"));

    QTRY_COMPARE_WITH_TIMEOUT(audio_spy.count(), 1, 10000);
    QCOMPARE(audio_spy.first().first().toString(),
             QStringLiteral("https://example.test/audio.mp3"));
}

// B4: Features/Popup/popup.js playEntryAudio passes entry.reading only, while
// fetchAudioList keeps the reading || expression fallback.
void PopupBridgeTest::play_entry_audio_uses_the_entry_reading() {
    const auto assets = niratan::dictionary::load_popup_assets();
    QVERIFY(!assets.js.empty());
    QVERIFY(!assets.web_channel.empty());
    QVERIFY(!assets.bridge.empty());
    QVERIFY(!assets.selection.empty());

    QJsonArray entries = entries_fixture();
    QJsonObject entry = entries.first().toObject();
    entry.insert(QStringLiteral("reading"), QString());
    entries = QJsonArray{entry};

    niratan::dictionary::PopupConfig config;
    config.audio.sources = {"http://localhost/audio?reading={reading}"};
    config.audio.source_names = {"Local"};

    const std::string shell = niratan::dictionary::popup_shell_html(entries, {}, config, assets.css,
                                                                    assets.selection, assets.js);

    QWebEnginePage page;
    QWebChannel channel(&page);
    page.setWebChannel(&channel);
    DictionaryBridgeProbe probe;
    channel.registerObject(QStringLiteral("dictionaryBridge"), &probe);
    QSignalSpy audio_spy(&probe, &DictionaryBridgeProbe::audioReceived);

    QSignalSpy load_spy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(QString::fromStdString(shell));
    QVERIFY(load_spy.wait(15000));
    QVERIFY(load_spy.last().at(0).toBool());

    evaluate(page,
             QString::fromStdString(assets.web_channel) + QStringLiteral("\n") +
                 QString::fromStdString(assets.bridge));

    QElapsedTimer timer;
    timer.start();
    QString handler_type;
    while (timer.elapsed() < 10000) {
        handler_type =
            evaluate(page, QStringLiteral("typeof window.webkit?.messageHandlers?.playWordAudio"));
        if (handler_type == QStringLiteral("object")) {
            break;
        }
        QTest::qWait(50);
    }
    QCOMPARE(handler_type, QStringLiteral("object"));

    evaluate(page, QStringLiteral(R"JS(
        window.audioProbe = [];
        window.fetchAudioSources = async (template, expression, reading) => {
            window.audioProbe.push(reading);
            return [{url: 'http://localhost/audio.mp3'}];
        };
        window.fetchAudioList(0);
        window.playEntryAudio(0);
    )JS"));

    QTRY_COMPARE_WITH_TIMEOUT(audio_spy.count(), 1, 10000);
    QCOMPARE(audio_spy.first().first().toString(), QStringLiteral("http://localhost/audio.mp3"));
    // fetchAudioList falls back to the expression; playEntryAudio passes the
    // empty reading unchanged.
    QCOMPARE(evaluate(page, QStringLiteral("String(window.audioProbe[0])")),
             QStringLiteral("decide"));
    QCOMPARE(evaluate(page, QStringLiteral("String(window.audioProbe[1])")), QString());
}

// B1, B2, B6, B23: PopupWebView.Coordinator.userContentController (async),
// AnkiMiningResult.webPayload and popup.js miningContent/mineEntry.
void PopupBridgeTest::mine_entry_posts_a_string_map_and_awaits_its_payload() {
    MiningPopup popup;
    QSignalSpy received(&popup.probe, &MiningBridgeProbe::mineReceived);
    QVERIFY(popup.load());
    QTRY_COMPARE_WITH_TIMEOUT(popup.js("typeof window.webkit?.messageHandlers?.mineEntry"),
                              QStringLiteral("object"), 2000);
    popup.js(QStringLiteral(R"JS(
        window.mineReply = 'waiting';
        const mineHandler = window.webkit.messageHandlers.mineEntry;
        const postMine = mineHandler.postMessage.bind(mineHandler);
        mineHandler.postMessage = content => {
            const reply = postMine(content);
            window.mineIsPromise = typeof reply?.then === 'function';
            return reply;
        };
        mineEntry('decide', 'decide', [], [], [], 'decided', 0, 'selection')
            .then(reply => { window.mineReply = JSON.stringify(reply) ?? 'missing'; });
        'sent';
    )JS"));
    QTRY_COMPARE_WITH_TIMEOUT(received.size(), 1, 2000);
    const auto body = QJsonValue::fromVariant(received.first().at(1)).toObject();
    QStringList keys{"expression",         "reading",         "matched",
                     "furiganaPlain",      "frequenciesHtml", "freqHarmonicRank",
                     "glossary",           "glossaryFirst",   "singleGlossaries",
                     "pitchPositions",     "pitchCategories", "phoneticTranscriptions",
                     "popupSelectionText", "audio",           "selectedDictionary",
                     "dictionaryMedia"};
    keys.sort();
    QCOMPARE(body.keys(), keys);
    for (const auto& value : body) {
        QVERIFY(value.isString());
    }
    QCOMPARE(body.value("expression"), QJsonValue("decide"));
    QCOMPARE(body.value("matched"), QJsonValue("decided"));
    QCOMPARE(body.value("popupSelectionText"), QJsonValue("selection"));
    QCOMPARE(popup.js("String(window.mineIsPromise)"), QStringLiteral("true"));
    QCOMPARE(popup.js("window.mineReply"), QStringLiteral("waiting"));

    const QVariantMap payload{{"status", QStringLiteral("added")},
                              {"message", QStringLiteral("Added to Anki.")},
                              {"noteID", QStringLiteral("9007199254740993")}};
    emit popup.probe.replyReady(received.first().at(0).toInt(), payload);
    QTRY_VERIFY_WITH_TIMEOUT(popup.js("window.mineReply") != "waiting", 2000);
    QCOMPARE(QJsonDocument::fromJson(popup.js("window.mineReply").toUtf8()).object(),
             QJsonObject::fromVariantMap(payload));
}

// B1, B3, B16, B17, B23: PopupWebView.Coordinator.userContentController,
// AnkiDuplicateLookupResult.webPayload and popup.js createEntryHeader/applyAnkiDuplicateLookup.
void PopupBridgeTest::duplicate_check_awaits_its_payload_and_reveals_note_button() {
    MiningPopup popup;
    QSignalSpy received(&popup.probe, &MiningBridgeProbe::duplicateReceived);
    QVERIFY(popup.load());
    QTRY_COMPARE_WITH_TIMEOUT(received.size(), 1, 2000);
    QCOMPARE(received.first().at(1).metaType(), QMetaType::fromType<QString>());
    QCOMPARE(received.first().at(1).toString(), QStringLiteral("decide"));
    QCOMPARE(popup.js("String(document.querySelector('[data-kind=viewNote]').hidden)"),
             QStringLiteral("true"));
    QCOMPARE(popup.js("String(document.querySelector('[data-kind=viewNote]').disabled)"),
             QStringLiteral("true"));

    const QVariantMap duplicate{{"isDuplicate", true},
                                {"noteIDs", QStringList{"9007199254740993", "18"}}};
    emit popup.probe.replyReady(received.first().at(0).toInt(), duplicate);
    QTRY_COMPARE_WITH_TIMEOUT(
        popup.js("document.querySelector('[data-kind=viewNote]').dataset.noteIds"),
        QStringLiteral("9007199254740993 18"), 2000);
    QCOMPARE(popup.js("String(document.querySelector('[data-kind=viewNote]').hidden)"),
             QStringLiteral("false"));
    QCOMPARE(popup.js("String(document.querySelector('[data-kind=viewNote]').disabled)"),
             QStringLiteral("false"));
    QCOMPARE(popup.js("String(document.querySelector('[data-kind=mine]').disabled)"),
             QStringLiteral("true"));

    popup.js(QStringLiteral(R"JS(
        window.duplicateReply = 'waiting';
        window.webkit.messageHandlers.duplicateCheck.postMessage('unknown')
            .then(reply => { window.duplicateReply = JSON.stringify(reply) ?? 'missing'; });
        'sent';
    )JS"));
    QTRY_COMPARE_WITH_TIMEOUT(received.size(), 2, 2000);
    QCOMPARE(received.last().at(1).toString(), QStringLiteral("unknown"));
    QCOMPARE(popup.js("window.duplicateReply"), QStringLiteral("waiting"));
    const QVariantMap absent{{"isDuplicate", false}, {"noteIDs", QStringList{}}};
    emit popup.probe.replyReady(received.last().at(0).toInt(), absent);
    QTRY_VERIFY_WITH_TIMEOUT(popup.js("window.duplicateReply") != "waiting", 2000);
    QCOMPARE(QJsonDocument::fromJson(popup.js("window.duplicateReply").toUtf8()).object(),
             QJsonObject::fromVariantMap(absent));
}

// B1, B4, B18, B19: PopupWebView.updateUIView/userContentController
// and popup.js showAnkiNoteButton/openAnkiNoteAtIndex.
void PopupBridgeTest::open_anki_note_posts_the_button_ids() {
    MiningPopup popup;
    QSignalSpy received(&popup.probe, &MiningBridgeProbe::openReceived);
    QVERIFY(popup.load());
    QTRY_COMPARE_WITH_TIMEOUT(popup.js("typeof window.webkit?.messageHandlers?.openAnkiNote"),
                              QStringLiteral("object"), 2000);
    popup.js("window.hoshiShowAnkiNoteButton(0, '9007199254740993');");
    QCOMPARE(popup.js("document.querySelector('[data-kind=viewNote]').dataset.noteIds"),
             QStringLiteral("9007199254740993"));
    const auto disabled_during_open = popup.js(QStringLiteral(R"JS(
        window.hoshiShowAnkiNoteButton(0, ['9007199254740993', '18']);
        const button = document.querySelector('[data-kind=viewNote]');
        button.click();
        String(button.disabled);
    )JS"));
    QCOMPARE(disabled_during_open, QStringLiteral("true"));
    QTRY_COMPARE_WITH_TIMEOUT(received.size(), 1, 2000);
    QCOMPARE(QJsonValue::fromVariant(received.first().at(0)),
             (QJsonValue(QJsonArray{"9007199254740993", "18"})));
    QTRY_COMPARE_WITH_TIMEOUT(
        popup.js("String(document.querySelector('[data-kind=viewNote]').disabled)"),
        QStringLiteral("false"), 2000);
}

// B1, B5, B11, B17, B24: PopupWebView.Coordinator.userContentController (one-way),
// PopupView.onMiningFeedback and popup.js mineEntryAtIndex/showAnkiNoteButton.
void PopupBridgeTest::mining_feedback_posts_pending_before_mining() {
    MiningPopup popup;
    QStringList order;
    connect(&popup.probe, &MiningBridgeProbe::feedbackReceived, this,
            [&] { order.append("feedback"); });
    connect(&popup.probe, &MiningBridgeProbe::mineReceived, this, [&] { order.append("mine"); });
    QSignalSpy received(&popup.probe, &MiningBridgeProbe::feedbackReceived);
    QSignalSpy mine(&popup.probe, &MiningBridgeProbe::mineReceived);
    QSignalSpy duplicate(&popup.probe, &MiningBridgeProbe::duplicateReceived);
    QVERIFY(popup.load());
    QTRY_COMPARE_WITH_TIMEOUT(popup.js("typeof window.webkit?.messageHandlers?.miningFeedback"),
                              QStringLiteral("object"), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(duplicate.size(), 1, 2000);
    popup.js("mineEntryAtIndex(0); 'sent';");
    QTRY_COMPARE_WITH_TIMEOUT(order, (QStringList{"feedback", "mine"}), 2000);
    QCOMPARE(received.size(), 1);
    const QJsonObject pending{{"status", "pending"},
                              {"message", QString::fromUtf8("Preparing card…")}};
    QCOMPARE(QJsonValue::fromVariant(received.first().at(0)).toObject(), pending);
    QCOMPARE(mine.size(), 1);
    const QVariantMap added{{"status", QStringLiteral("added")},
                            {"message", QStringLiteral("Added to Anki.")},
                            {"noteID", QStringLiteral("9007199254740993")}};
    emit popup.probe.replyReady(mine.first().at(0).toInt(), added);
    QTRY_COMPARE_WITH_TIMEOUT(duplicate.size(), 2, 2000);
    QCOMPARE(duplicate.last().at(1).toString(), QStringLiteral("decide"));
    QCOMPARE(popup.js("document.querySelector('[data-kind=viewNote]').dataset.noteIds"),
             QStringLiteral("9007199254740993"));
}

// B21: PopupWebView.Coordinator.userContentController returns a failure when onMine is nil.
void PopupBridgeTest::missing_mine_callback_resolves_with_failure() {
    DictionaryBridgeProbe audio_only;
    MiningPopup popup;
    QVERIFY(popup.load(&audio_only));
    QTRY_COMPARE_WITH_TIMEOUT(popup.js("typeof window.webkit?.messageHandlers?.mineEntry"),
                              QStringLiteral("object"), 2000);
    popup.js(QStringLiteral(R"JS(
        window.mineReply = 'waiting';
        window.webkit.messageHandlers.mineEntry.postMessage({expression: 'decide'})
            .then(reply => { window.mineReply = JSON.stringify(reply) ?? 'missing'; });
        'sent';
    )JS"));
    QTRY_VERIFY_WITH_TIMEOUT(popup.js("window.mineReply") != "waiting", 2000);
    const QJsonObject expected{{"status", "failed"}, {"message", "Unable to add card."}};
    QCOMPARE(QJsonDocument::fromJson(popup.js("window.mineReply").toUtf8()).object(), expected);
}

// B2: PopupWebView.constructHtml loads selection.js before the language
// assignment, so the popup document owns window.hoshiSelection.
void PopupBridgeTest::the_shell_defines_the_selection_object() {
    const auto assets = niratan::dictionary::load_popup_assets();
    QVERIFY(!assets.selection.empty());
    niratan::dictionary::PopupConfig config;
    config.display.content_language_id = "en";
    const std::string shell = niratan::dictionary::popup_shell_html(
        entries_fixture(), {}, config, assets.css, assets.selection, assets.js);

    QWebEnginePage page;
    QSignalSpy load_spy(&page, &QWebEnginePage::loadFinished);
    page.setHtml(QString::fromStdString(shell));
    QVERIFY(load_spy.wait(15000));
    QVERIFY(load_spy.last().at(0).toBool());

    QCOMPARE(evaluate(page, QStringLiteral("typeof window.hoshiSelection")),
             QStringLiteral("object"));
    QCOMPARE(evaluate(page, QStringLiteral("window.hoshiSelection.language")),
             QStringLiteral("en"));
    QCOMPARE(evaluate(page, QStringLiteral("typeof window.hoshiSelection.selectText")),
             QStringLiteral("function"));
}

// B6: PopupWebView.Coordinator.getEntries, lookupRedirect and
// queryTextSelected settle the page's promises; textSelected is one-way.
void PopupBridgeTest::inline_lookup_requests_reach_the_bridge() {
    MiningPopup popup;
    QSignalSpy entries(&popup.probe, &MiningBridgeProbe::entriesReceived);
    QSignalSpy redirect(&popup.probe, &MiningBridgeProbe::redirectReceived);
    QSignalSpy selected(&popup.probe, &MiningBridgeProbe::querySelectedReceived);
    QSignalSpy one_way(&popup.probe, &MiningBridgeProbe::textSelectedReceived);
    QVERIFY(popup.load());
    QTRY_COMPARE_WITH_TIMEOUT(popup.js("typeof window.webkit?.messageHandlers?.getEntries"),
                              QStringLiteral("object"), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(popup.js("typeof window.webkit?.messageHandlers?.lookupRedirect"),
                              QStringLiteral("object"), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(popup.js("typeof window.webkit?.messageHandlers?.queryTextSelected"),
                              QStringLiteral("object"), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(popup.js("typeof window.webkit?.messageHandlers?.textSelected"),
                              QStringLiteral("object"), 2000);

    popup.js(QStringLiteral(R"JS(
        window.entriesReply = 'waiting';
        window.webkit.messageHandlers.getEntries.postMessage({start: 0, count: 1})
            .then(reply => { window.entriesReply = JSON.stringify(reply) ?? 'missing'; });
        'sent';
    )JS"));
    QTRY_COMPARE_WITH_TIMEOUT(entries.size(), 1, 2000);
    const auto request = entries.first().at(1).toMap();
    QCOMPARE(request.value(QStringLiteral("start")).toInt(), 0);
    QCOMPARE(request.value(QStringLiteral("count")).toInt(), 1);
    QCOMPARE(popup.js("window.entriesReply"), QStringLiteral("waiting"));
    const QVariantList slice{QVariantMap{{QStringLiteral("expression"), QStringLiteral("decide")}}};
    emit popup.probe.replyReady(entries.first().at(0).toInt(), slice);
    QTRY_VERIFY_WITH_TIMEOUT(popup.js("window.entriesReply") != "waiting", 2000);
    QCOMPARE(QJsonDocument::fromJson(popup.js("window.entriesReply").toUtf8()).array(),
             (QJsonArray{QJsonObject{{"expression", "decide"}}}));

    popup.js(QStringLiteral(R"JS(
        window.redirectReply = 'waiting';
        window.webkit.messageHandlers.lookupRedirect.postMessage('decide')
            .then(reply => { window.redirectReply = String(reply); });
        'sent';
    )JS"));
    QTRY_COMPARE_WITH_TIMEOUT(redirect.size(), 1, 2000);
    QCOMPARE(redirect.first().at(1).toString(), QStringLiteral("decide"));
    emit popup.probe.replyReady(redirect.first().at(0).toInt(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(popup.js("window.redirectReply"), QStringLiteral("1"), 2000);

    popup.js(QStringLiteral(R"JS(
        window.queryReply = 'waiting';
        window.webkit.messageHandlers.queryTextSelected.postMessage({text: 'decide'})
            .then(reply => { window.queryReply = JSON.stringify(reply) ?? 'missing'; });
        'sent';
    )JS"));
    QTRY_COMPARE_WITH_TIMEOUT(selected.size(), 1, 2000);
    QCOMPARE(selected.first().at(1).toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("decide"));
    const QVariantMap result{{QStringLiteral("entryCount"), 1},
                             {QStringLiteral("matchedCharacterCount"), 6}};
    emit popup.probe.replyReady(selected.first().at(0).toInt(), result);
    QTRY_VERIFY_WITH_TIMEOUT(popup.js("window.queryReply") != "waiting", 2000);
    QCOMPARE(QJsonDocument::fromJson(popup.js("window.queryReply").toUtf8()).object(),
             QJsonObject::fromVariantMap(result));

    popup.js(QStringLiteral(
        "window.webkit.messageHandlers.textSelected.postMessage({text: 'decide'}); 'sent';"));
    QTRY_COMPARE_WITH_TIMEOUT(one_way.size(), 1, 2000);
    QCOMPARE(one_way.first().at(0).toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("decide"));
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();

    QTemporaryDir data_root;
    if (data_root.isValid()) {
        const QByteArray base = data_root.path().toUtf8();
        qputenv("HOME", base);
        qputenv("XDG_DATA_HOME", base + "/data");
        qputenv("XDG_CACHE_HOME", base + "/cache");
    }

    QGuiApplication app(argc, argv);
    PopupBridgeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_popup_bridge.moc"

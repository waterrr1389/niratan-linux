// SPDX-License-Identifier: GPL-3.0-or-later
//
// Core/AnkiManager.swift addNote: the word audio download feeds attachment
// bytes. The mining scheme rule also accepts file URLs but excludes data URLs.

#include "services/audio/web_audio_download.h"

#include <QTemporaryFile>
#include <QTest>

using niratan::audio::WebAudioDownload;

class WebAudioDownloadTest : public QObject {
    Q_OBJECT

   private slots:
    void rejects_unsupported_scheme();
    void reads_file_url();
    void missing_file_is_not_an_attachment();
};

void WebAudioDownloadTest::rejects_unsupported_scheme() {
    WebAudioDownload download;
    bool finished = false;
    download.get(QUrl("data:audio/mpeg;base64,c291bmQ="), [&](auto bytes) {
        finished = true;
        QVERIFY(!bytes);
    });
    QTRY_VERIFY(finished);
}

void WebAudioDownloadTest::reads_file_url() {
    QTemporaryFile file;
    QVERIFY(file.open());
    const QByteArray audio("audio\0bytes", 11);
    QCOMPARE(file.write(audio), audio.size());
    QVERIFY(file.flush());

    WebAudioDownload download;
    std::optional<QByteArray> received;
    download.get(QUrl::fromLocalFile(file.fileName()),
                 [&](auto bytes) { received = std::move(bytes); });
    QTRY_VERIFY(received.has_value());
    QCOMPARE(*received, audio);
}

void WebAudioDownloadTest::missing_file_is_not_an_attachment() {
    QTemporaryFile file;
    QVERIFY(file.open());
    const auto url = QUrl::fromLocalFile(file.fileName());
    QVERIFY(file.remove());

    WebAudioDownload download;
    bool finished = false;
    download.get(url, [&](auto bytes) {
        finished = true;
        QVERIFY(!bytes);
    });
    QTRY_VERIFY(finished);
}

QTEST_GUILESS_MAIN(WebAudioDownloadTest)
#include "tst_web_audio_download.moc"

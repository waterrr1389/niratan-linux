// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/media_type.h"

#include <QTest>

#include <string_view>

using niratan::dictionary::dictionary_media_mime_type;

// Features/Popup/PopupWebView.swift (ImageHandler.mimeType) has no test
// script; the cases follow its switch and path-extension rule.
class MediaTypeTest : public QObject {
    Q_OBJECT

   private slots:
    void image_extensions_map_to_their_types();
    void the_extension_is_lowercased_and_taken_from_the_last_component();
    void unknown_and_missing_extensions_fall_back();
};

void MediaTypeTest::image_extensions_map_to_their_types() {
    QCOMPARE(dictionary_media_mime_type("a.png"), std::string_view("image/png"));
    QCOMPARE(dictionary_media_mime_type("a.jpg"), std::string_view("image/jpeg"));
    QCOMPARE(dictionary_media_mime_type("a.jpeg"), std::string_view("image/jpeg"));
    QCOMPARE(dictionary_media_mime_type("a.gif"), std::string_view("image/gif"));
    QCOMPARE(dictionary_media_mime_type("a.webp"), std::string_view("image/webp"));
    QCOMPARE(dictionary_media_mime_type("a.avif"), std::string_view("image/avif"));
    QCOMPARE(dictionary_media_mime_type("a.heic"), std::string_view("image/heic"));
    QCOMPARE(dictionary_media_mime_type("a.svg"), std::string_view("image/svg+xml"));
}

void MediaTypeTest::the_extension_is_lowercased_and_taken_from_the_last_component() {
    QCOMPARE(dictionary_media_mime_type("a.PNG"), std::string_view("image/png"));
    QCOMPARE(dictionary_media_mime_type("dir/a.JpG"), std::string_view("image/jpeg"));
    QCOMPARE(dictionary_media_mime_type("a.tar.png"), std::string_view("image/png"));
}

void MediaTypeTest::unknown_and_missing_extensions_fall_back() {
    QCOMPARE(dictionary_media_mime_type("a.bmp"), std::string_view("application/octet-stream"));
    QCOMPARE(dictionary_media_mime_type("a"), std::string_view("application/octet-stream"));
    QCOMPARE(dictionary_media_mime_type("a."), std::string_view("application/octet-stream"));
    QCOMPARE(dictionary_media_mime_type(".png"), std::string_view("application/octet-stream"));
    QCOMPARE(dictionary_media_mime_type(""), std::string_view("application/octet-stream"));
}

QTEST_APPLESS_MAIN(MediaTypeTest)
#include "tst_media_type.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader:// scheme, following the file URLs NativeReaderView loads from
// the extracted EPUB: reader://book/<container path> serves one entry of the
// open book, with the injected stylesheet and script for spine documents.
//
// register_reader_url_schemes must run before QGuiApplication is created, as
// Qt requires for custom schemes. The scheme is secure, local-access allowed,
// CORS enabled and fetchable so the chapter may load its own images, styles
// and fonts through relative URLs.
//
// A request for a spine document is answered with text/html; any other entry
// is answered with its container bytes and the type from the extension table.
// An unknown or unsafe path fails with UrlNotFound rather than reading outside
// the archive.

#pragma once

#include <QObject>
#include <QWebEngineUrlSchemeHandler>

namespace niratan::viewmodels {
class ReaderViewModel;
}

namespace niratan::ui {

void register_reader_url_schemes();

class ReaderSchemeHandler : public QWebEngineUrlSchemeHandler {
    Q_OBJECT

   public:
    explicit ReaderSchemeHandler(QObject* parent = nullptr);

    // The handler reads the open book through the view model; both live in the
    // same process and the view model outlives the profile.
    void set_view_model(niratan::viewmodels::ReaderViewModel* view_model);

    void requestStarted(QWebEngineUrlRequestJob* job) override;

   private:
    niratan::viewmodels::ReaderViewModel* view_model_ = nullptr;
};

}  // namespace niratan::ui

// SPDX-License-Identifier: GPL-3.0-or-later
//
// The audio:// scheme the popup script uses for word audio, following
// AudioHandler in Features/Popup/PopupWebView.swift.
//
// popup.js builds audio://?url=<encoded audio URL>; this handler fetches that
// URL with the 5 second localhost / 4 second other timeout from the Swift
// handler and answers with the body as application/json, whatever the HTTP
// status. Only http and https targets are fetched: URLSession rejects other
// schemes, while QNetworkAccessManager would happily read a file URL.
//
// register_popup_url_schemes must run before QGuiApplication is created, as
// Qt requires for custom schemes. The scheme is secure, fetchable and CORS
// enabled so the popup document may use fetch on it.

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QWebEngineUrlSchemeHandler>

namespace niratan::ui {

void register_popup_url_schemes();

class AudioSchemeHandler : public QWebEngineUrlSchemeHandler {
    Q_OBJECT

   public:
    explicit AudioSchemeHandler(QObject* parent = nullptr);

    void requestStarted(QWebEngineUrlRequestJob* job) override;

   private:
    QNetworkAccessManager network_;
};

}  // namespace niratan::ui

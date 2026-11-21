// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_scheme_handler.h"

#include <QBuffer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>

#include "services/audio/audio_request.h"

namespace niratan::ui {

void register_popup_url_schemes() {
    QWebEngineUrlScheme audio("audio");
    audio.setSyntax(QWebEngineUrlScheme::Syntax::Path);
    audio.setFlags(QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::FetchApiAllowed |
                   QWebEngineUrlScheme::CorsEnabled);
    QWebEngineUrlScheme::registerScheme(audio);

    QWebEngineUrlScheme media("dict");
    media.setSyntax(QWebEngineUrlScheme::Syntax::Path);
    media.setFlags(QWebEngineUrlScheme::SecureScheme);
    QWebEngineUrlScheme::registerScheme(media);
}

AudioSchemeHandler::AudioSchemeHandler(QObject* parent)
    : QWebEngineUrlSchemeHandler(parent), network_(this) {}

void AudioSchemeHandler::requestStarted(QWebEngineUrlRequestJob* job) {
    const auto request =
        audio::parse_audio_request(job->requestUrl().toString().toStdString());
    if (!request) {
        job->fail(QWebEngineUrlRequestJob::UrlInvalid);
        return;
    }

    const QUrl target(QString::fromStdString(request->url));
    if (target.scheme() != QStringLiteral("http") && target.scheme() != QStringLiteral("https")) {
        job->fail(QWebEngineUrlRequestJob::UrlInvalid);
        return;
    }

    QNetworkRequest network_request(target);
    network_request.setTransferTimeout(request->timeout_ms);
    QNetworkReply* reply = network_.get(network_request);

    // The job may be destroyed while the request is in flight.
    QObject::connect(job, &QObject::destroyed, reply, &QObject::deleteLater);
    QObject::connect(reply, &QNetworkReply::finished, job, [job, reply] {
        const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (!status.isValid() && reply->error() != QNetworkReply::NoError) {
            job->fail(QWebEngineUrlRequestJob::RequestFailed);
            reply->deleteLater();
            return;
        }

        auto* buffer = new QBuffer(job);
        buffer->setData(reply->readAll());
        buffer->open(QIODevice::ReadOnly);
        job->reply("application/json", buffer);
        reply->deleteLater();
    });
}

}  // namespace niratan::ui

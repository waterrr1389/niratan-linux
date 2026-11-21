// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_scheme_handler.h"

#include <QBuffer>
#include <QByteArray>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>

#include <string>

#include "viewmodels/reader/reader_view_model.h"

namespace niratan::ui {

void register_reader_url_schemes() {
    QWebEngineUrlScheme scheme(QByteArrayLiteral("reader"));
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::LocalAccessAllowed |
                    QWebEngineUrlScheme::CorsEnabled | QWebEngineUrlScheme::FetchApiAllowed);
    // Chromium rejects a host-and-port scheme whose port is 0, which leaves the
    // navigation pending; the reader URL carries the book in the host and the
    // container path after it, so a host-only syntax is enough.
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    QWebEngineUrlScheme::registerScheme(scheme);
}

ReaderSchemeHandler::ReaderSchemeHandler(QObject* parent)
    : QWebEngineUrlSchemeHandler(parent) {}

void ReaderSchemeHandler::set_view_model(niratan::viewmodels::ReaderViewModel* view_model) {
    view_model_ = view_model;
}

void ReaderSchemeHandler::requestStarted(QWebEngineUrlRequestJob* job) {
    const QUrl url = job->requestUrl();
    std::string path = url.path().toStdString();
    if (!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }

    if (view_model_ == nullptr || url.host() != QLatin1String("book") || path.empty()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    const std::string document = view_model_->document_for(path);
    const QByteArray mime = document.empty() ? QByteArray() : QByteArrayLiteral("text/html");
    const QByteArray body =
        document.empty() ? QByteArray() : QByteArray::fromStdString(document);

    if (!document.empty()) {
        auto* buffer = new QBuffer(job);
        buffer->setData(body);
        buffer->open(QIODevice::ReadOnly);
        job->reply(mime, buffer);
        return;
    }

    const auto resource = view_model_->resource(path);
    if (!resource.has_value()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    auto* buffer = new QBuffer(job);
    buffer->setData(QByteArray::fromStdString(resource->bytes));
    buffer->open(QIODevice::ReadOnly);
    job->reply(QByteArray::fromStdString(resource->mime_type), buffer);
}

}  // namespace niratan::ui

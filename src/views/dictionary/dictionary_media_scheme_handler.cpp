// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_media_scheme_handler.h"

#include <QBuffer>
#include <QWebEngineUrlRequestJob>

#include "services/storage/app_paths.h"

namespace niratan::ui {

DictionaryMediaSchemeHandler::DictionaryMediaSchemeHandler(QObject* parent)
    : QWebEngineUrlSchemeHandler(parent), media_(storage::dictionaries_root()) {}

void DictionaryMediaSchemeHandler::requestStarted(QWebEngineUrlRequestJob* job) {
    if (job->requestMethod() != "GET") {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }

    auto response = media_.get(job->requestUrl());
    switch (response.status) {
        case dictionary::MediaStatus::Invalid:
            job->fail(QWebEngineUrlRequestJob::UrlInvalid);
            return;
        case dictionary::MediaStatus::Missing:
            job->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
        case dictionary::MediaStatus::Found:
            auto* buffer = new QBuffer(job);
            buffer->setData(response.bytes);
            buffer->open(QIODevice::ReadOnly);
            job->reply(response.mime_type, buffer);
    }
}

}  // namespace niratan::ui

// SPDX-License-Identifier: GPL-3.0-or-later

#include "popup_documents.h"

#include <QBuffer>
#include <QJSEngine>
#include <QStringList>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestJob>

namespace niratan::ui {

namespace {

constexpr char kScheme[] = "popup";

}  // namespace

PopupDocuments::PopupDocuments(QObject* parent) : QObject(parent) {}

PopupDocuments* PopupDocuments::instance() {
    static PopupDocuments* documents = new PopupDocuments();
    return documents;
}

PopupDocuments* PopupDocuments::create(QQmlEngine*, QJSEngine*) {
    PopupDocuments* documents = instance();
    QJSEngine::setObjectOwnership(documents, QJSEngine::CppOwnership);
    return documents;
}

QUrl PopupDocuments::publish(QObject* view, const QString& html) {
    if (view == nullptr) {
        return {};
    }
    // The views use the default profile; the handler is installed with the
    // first document so every window and test that shows a popup has it.
    QWebEngineProfile* profile = QWebEngineProfile::defaultProfile();
    if (profile->urlSchemeHandler(kScheme) == nullptr) {
        profile->installUrlSchemeHandler(kScheme, new PopupDocumentSchemeHandler(profile));
    }

    auto id = view_ids_.constFind(view);
    if (id == view_ids_.constEnd()) {
        id = view_ids_.insert(view, next_view_++);
        const quint64 view_id = *id;
        connect(view, &QObject::destroyed, this, [this, view, view_id] {
            view_ids_.remove(view);
            entries_.remove(view_id);
        });
    }
    Entry& entry = entries_[*id];
    entry.generation = next_generation_++;
    entry.html = html.toUtf8();
    QUrl url;
    url.setScheme(QString::fromLatin1(kScheme));
    url.setPath(QStringLiteral("%1/%2").arg(*id).arg(entry.generation));
    return url;
}

bool PopupDocuments::isDocument(QObject* view, const QUrl& url) const {
    const auto key = parse(url);
    const auto id = view_ids_.constFind(view);
    if (!key || id == view_ids_.constEnd() || *id != key->view) {
        return false;
    }
    const auto entry = entries_.constFind(key->view);
    return entry != entries_.constEnd() && entry->generation == key->generation;
}

std::optional<QByteArray> PopupDocuments::document(const QUrl& url) const {
    const auto key = parse(url);
    if (!key) {
        return std::nullopt;
    }
    const auto entry = entries_.constFind(key->view);
    if (entry == entries_.constEnd() || entry->generation != key->generation) {
        return std::nullopt;
    }
    return entry->html;
}

std::optional<PopupDocuments::Key> PopupDocuments::parse(const QUrl& url) {
    if (url.scheme() != QLatin1StringView(kScheme)) {
        return std::nullopt;
    }
    const QStringList parts = url.path().split(QLatin1Char('/'));
    if (parts.size() != 2) {
        return std::nullopt;
    }
    bool view_ok = false;
    bool generation_ok = false;
    Key key{parts.at(0).toULongLong(&view_ok), parts.at(1).toULongLong(&generation_ok)};
    if (!view_ok || !generation_ok) {
        return std::nullopt;
    }
    return key;
}

PopupDocumentSchemeHandler::PopupDocumentSchemeHandler(QObject* parent)
    : QWebEngineUrlSchemeHandler(parent) {}

void PopupDocumentSchemeHandler::requestStarted(QWebEngineUrlRequestJob* job) {
    if (job->requestMethod() != "GET") {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }
    const auto html = PopupDocuments::instance()->document(job->requestUrl());
    if (!html) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    auto* buffer = new QBuffer(job);
    buffer->setData(*html);
    buffer->open(QIODevice::ReadOnly);
    job->reply(QByteArrayLiteral("text/html"), buffer);
}

}  // namespace niratan::ui

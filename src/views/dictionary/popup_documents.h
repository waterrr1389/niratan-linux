// SPDX-License-Identifier: GPL-3.0-or-later
//
// Serves the generated lookup document to the web views that show it,
// standing in for PopupWebView.swift's loadHTMLString(_:baseURL:).
//
// WebEngine's loadHtml sends the document as a data: URL, which Chromium caps
// at 2 MB. A word found in many dictionaries passes that (この with twenty
// installed dictionaries is about 2.7 MB as a data URL); the load then fails
// without an error and the view keeps the previous lookup. publish() stores
// the document instead and returns a popup: URL that the scheme handler
// answers from memory, which has no such limit.
//
// Each view keeps only its latest document: publish replaces it, and the
// entry is dropped with the view. A URL names the view and the generation of
// its document, so isDocument accepts only the navigation to the document the
// view was last given; the page that owns the web channel is never another
// view's or an older one.

#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QUrl>
#include <QWebEngineUrlSchemeHandler>
#include <QtQml/qqmlregistration.h>

#include <optional>

class QJSEngine;
class QQmlEngine;

namespace niratan::ui {

class PopupDocuments : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

   public:
    static PopupDocuments* instance();
    static PopupDocuments* create(QQmlEngine* engine, QJSEngine* js_engine);

    // Stores html as view's current document and returns the URL to load.
    Q_INVOKABLE QUrl publish(QObject* view, const QString& html);
    // True when url is the document view was last given by publish.
    Q_INVOKABLE bool isDocument(QObject* view, const QUrl& url) const;

    // The bytes of the document url names, if it is still current.
    std::optional<QByteArray> document(const QUrl& url) const;

   private:
    explicit PopupDocuments(QObject* parent = nullptr);

    struct Entry {
        quint64 generation = 0;
        QByteArray html;
    };
    struct Key {
        quint64 view = 0;
        quint64 generation = 0;
    };
    static std::optional<Key> parse(const QUrl& url);

    QHash<QObject*, quint64> view_ids_;
    QHash<quint64, Entry> entries_;
    quint64 next_view_ = 1;
    quint64 next_generation_ = 1;
};

class PopupDocumentSchemeHandler : public QWebEngineUrlSchemeHandler {
    Q_OBJECT

   public:
    explicit PopupDocumentSchemeHandler(QObject* parent = nullptr);
    void requestStarted(QWebEngineUrlRequestJob* job) override;
};

}  // namespace niratan::ui

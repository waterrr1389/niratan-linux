// SPDX-License-Identifier: GPL-3.0-or-later
//
// Adapts PopupWebView.swift's ImageHandler to WebEngine. The dict: scheme
// resolves a dictionary name and media key; it never opens URL paths.

#pragma once

#include <QWebEngineUrlSchemeHandler>

#include "services/dictionary/dictionary_media.h"

namespace niratan::ui {

class DictionaryMediaSchemeHandler : public QWebEngineUrlSchemeHandler {
   public:
    explicit DictionaryMediaSchemeHandler(QObject* parent = nullptr);
    void requestStarted(QWebEngineUrlRequestJob* job) override;

   private:
    dictionary::DictionaryMediaService media_;
};

}  // namespace niratan::ui

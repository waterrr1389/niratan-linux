// SPDX-License-Identifier: GPL-3.0-or-later
//
// The popup stylesheet, scripts and bridge compiled into the core library,
// copied from Features/Popup/popup.css, popup.js and
// Features/Reader/ReaderWebView/selection.js at Niratan 609d2d7.
//
// selection is the reader's selection.js compiled a second time under the
// popup resource prefix, because PopupWebView.constructHtml loads the same
// script into the popup document. bridge is the QtWebChannel adapter and
// web_channel the QWebChannel client library.
//
// Loading the resources here keeps file IO out of the ViewModel: the caller
// passes the text to popup_shell_html. A missing resource yields an empty
// string rather than failing, which the shell tolerates as unstyled markup.

#pragma once

#include <string>

namespace niratan::dictionary {

struct PopupAssets {
    std::string css;
    std::string js;
    // The reader's selection.js, loaded from :/niratan/dictionary_popup/selection.js.
    std::string selection;
    std::string web_channel;
    std::string bridge;
};

PopupAssets load_popup_assets();

}  // namespace niratan::dictionary

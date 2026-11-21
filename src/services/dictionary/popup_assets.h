// SPDX-License-Identifier: GPL-3.0-or-later
//
// The popup stylesheet and script compiled into the core library, copied from
// Features/Popup/popup.css and popup.js at Niratan 609d2d7.
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
    std::string web_channel;
    std::string bridge;
};

PopupAssets load_popup_assets();

}  // namespace niratan::dictionary

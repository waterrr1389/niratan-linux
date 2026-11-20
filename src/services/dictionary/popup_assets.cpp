// SPDX-License-Identifier: GPL-3.0-or-later

#include "popup_assets.h"

#include <QFile>
#include <QString>

namespace {

std::string read_resource(const char* path) {
    QFile file(QString::fromLatin1(path));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll().toStdString();
}
}  // namespace

namespace niratan::dictionary {

PopupAssets load_popup_assets() {
    return {read_resource(":/niratan/dictionary_popup/popup.css"),
            read_resource(":/niratan/dictionary_popup/popup.js")};
}

}  // namespace niratan::dictionary

// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/popup_assets.h"

#include <QtTest>

#include <string>

using niratan::dictionary::PopupAssets;
using niratan::dictionary::load_popup_assets;

// Features/Popup/popup.css and popup.js are compiled as resources; the popup
// shell cannot render without them.
class PopupAssetsTest : public QObject {
    Q_OBJECT

   private slots:
    void the_stylesheet_and_script_are_available();
};

void PopupAssetsTest::the_stylesheet_and_script_are_available() {
    const PopupAssets assets = load_popup_assets();

    QVERIFY(!assets.css.empty());
    QVERIFY(assets.css.find("--background-color") != std::string::npos);
    QVERIFY(assets.css.find(".gloss-image") != std::string::npos);

    QVERIFY(!assets.js.empty());
    QVERIFY(assets.js.find("window.lookupEntries") != std::string::npos);
    QVERIFY(assets.js.find("renderPopup") != std::string::npos);

    const PopupAssets again = load_popup_assets();
    QCOMPARE(again.css, assets.css);
    QCOMPARE(again.js, assets.js);
}

QTEST_APPLESS_MAIN(PopupAssetsTest)
#include "tst_popup_assets.moc"

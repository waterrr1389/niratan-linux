// SPDX-License-Identifier: GPL-3.0-or-later
//
// Behaviour of the Niratan control style (src/views/style) that pages rely
// on, from problems the owner met in daily use:
// - a pop-up button's list is as wide as its widest item, as a macOS pop-up
//   menu is, and a long list has a scroll bar to drag;
// - dialog buttons sit inside the dialog's padding in the macOS order, the
//   cancelling button before the default one, including the Anki settings
//   alerts (src/views/pages/AnkiSettingsAlerts.qml).
#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"

#include <QFontMetricsF>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>
#include <memory>

namespace {

QString error_text(const QQmlComponent& component) {
    QStringList lines;
    for (const QQmlError& error : component.errors()) {
        lines.append(error.toString());
    }
    return lines.join('\n');
}

QQuickItem* item_of(QObject* object, const char* property) {
    return object->property(property).value<QQuickItem*>();
}

// The visible buttons of a dialog's footer, keyed by their text.
QHash<QString, QQuickItem*> footer_buttons(QObject* dialog) {
    QHash<QString, QQuickItem*> buttons;
    QQuickItem* footer = item_of(dialog, "footer");
    if (footer == nullptr) {
        return buttons;
    }
    for (QQuickItem* item : footer->findChildren<QQuickItem*>()) {
        if (item->inherits("QQuickAbstractButton") && item->isVisible()) {
            buttons.insert(item->property("text").toString(), item);
        }
    }
    return buttons;
}

}  // namespace

class ControlStyleTest : public QObject {
    Q_OBJECT

   private slots:
    void combo_list_fits_its_widest_item();
    void dialog_buttons_sit_inside_in_mac_order();
    void anki_alert_buttons_sit_inside_in_mac_order();
    void clicking_a_control_leaves_the_keyboard_alone_data();
    void clicking_a_control_leaves_the_keyboard_alone();
    void clicking_a_text_field_takes_the_keyboard();

   private:
    std::unique_ptr<QObject> create(const QByteArray& qml, QQuickWindow& window);
    void check_dialog_buttons(QObject* dialog, const QString& cancel, const QString& accept);

    QQmlEngine engine_;
};

std::unique_ptr<QObject> ControlStyleTest::create(const QByteArray& qml, QQuickWindow& window) {
    QQmlComponent component(&engine_);
    component.setData(qml, QUrl(QStringLiteral("qrc:/test/Scene.qml")));
    std::unique_ptr<QObject> object(component.beginCreate(engine_.rootContext()));
    if (!object) {
        qWarning().noquote() << error_text(component);
        return {};
    }
    if (auto* item = qobject_cast<QQuickItem*>(object.get())) {
        item->setParentItem(window.contentItem());
    }
    component.completeCreate();
    return object;
}

// A narrow pop-up button holding long deck names: the open list is at least
// as wide as the longest name plus the row's padding (or as wide as the
// window allows), and a list taller than the window has a draggable bar.
void ControlStyleTest::combo_list_fits_its_widest_item() {
    QQuickWindow window;
    window.resize(960, 640);
    window.show();

    const QString longest = QStringLiteral("JP::JP-N2_3::eggs and the longest deck name here");
    QStringList names{longest};
    for (int index = 0; index < 60; ++index) {
        names.append(QStringLiteral("おにぎり文法::N%1").arg(index));
    }
    QByteArray qml = "import QtQuick\nimport QtQuick.Controls\n"
                     "Item { width: 960; height: 640\n"
                     "  ComboBox { objectName: \"box\"; x: 760; y: 300; width: 120; model: [";
    for (const QString& name : names) {
        qml += "\"" + name.toUtf8() + "\",";
    }
    qml += "] } }";
    std::unique_ptr<QObject> scene = create(qml, window);
    QVERIFY(scene != nullptr);
    QObject* box = scene->findChild<QObject*>(QStringLiteral("box"));
    QVERIFY(box != nullptr);

    auto* popup = box->property("popup").value<QObject*>();
    QVERIFY(popup != nullptr);
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());

    const QFontMetricsF metrics(box->property("font").value<QFont>());
    const qreal needed = metrics.horizontalAdvance(longest) + 24 + 12 + 10;
    const qreal width = popup->property("width").toReal();
    QVERIFY2(width >= needed - 1 || width >= window.width() - 2 * 6 - 1,
             qPrintable(
                 QStringLiteral("list %1 wide, the longest item needs %2").arg(width).arg(needed)));

    QQuickItem* list = item_of(popup, "contentItem");
    QVERIFY(list != nullptr);
    bool has_bar = false;
    for (QObject* child : list->findChildren<QObject*>()) {
        has_bar = has_bar || child->inherits("QQuickScrollBar");
    }
    QVERIFY2(has_bar, "the long list has no scroll bar to drag");
}

void ControlStyleTest::check_dialog_buttons(QObject* dialog, const QString& cancel,
                                            const QString& accept) {
    QTRY_VERIFY(dialog->property("opened").toBool());
    const QHash<QString, QQuickItem*> buttons = footer_buttons(dialog);
    QQuickItem* cancel_button = buttons.value(cancel);
    QQuickItem* accept_button = buttons.value(accept);
    QVERIFY2(cancel_button && accept_button,
             qPrintable(QStringLiteral("buttons: ") + buttons.keys().join(QStringLiteral(", "))));

    QQuickItem* background = item_of(dialog, "background");
    QVERIFY(background != nullptr);
    QQuickItem* frame = background->parentItem();
    QVERIFY(frame != nullptr);
    // The footer measures its buttons in a later polish; judge the settled
    // dialog.
    QQuickItem* footer = item_of(dialog, "footer");
    QVERIFY(footer != nullptr);
    QTRY_VERIFY(footer->implicitWidth() > 0 && frame->width() >= footer->implicitWidth());

    QVERIFY2(cancel_button->mapToItem(frame, QPointF(0, 0)).x() <
                 accept_button->mapToItem(frame, QPointF(0, 0)).x(),
             "the cancelling button is not before the default one");

    constexpr qreal kInset = 12;
    for (QQuickItem* button : {cancel_button, accept_button}) {
        const QPointF bottom_right =
            button->mapToItem(frame, QPointF(button->width(), button->height()));
        QVERIFY2(bottom_right.x() <= frame->width() - kInset &&
                     bottom_right.y() <= frame->height() - kInset,
                 qPrintable(QStringLiteral("%1 reaches (%2, %3) in a %4x%5 dialog")
                                .arg(button->property("text").toString())
                                .arg(bottom_right.x())
                                .arg(bottom_right.y())
                                .arg(frame->width())
                                .arg(frame->height())));
    }
}

// A dialog with standard buttons: Cancel then OK, inset from the edges.
void ControlStyleTest::dialog_buttons_sit_inside_in_mac_order() {
    QQuickWindow window;
    window.resize(960, 640);
    window.show();

    std::unique_ptr<QObject> scene =
        create("import QtQuick\nimport QtQuick.Controls\n"
               "Item { width: 960; height: 640\n"
               "  Dialog { objectName: \"dialog\"; title: \"Question\"; modal: true\n"
               "    anchors.centerIn: parent; standardButtons: Dialog.Ok | Dialog.Cancel\n"
               "    Label { text: \"Body\" } } }",
               window);
    QVERIFY(scene != nullptr);
    QObject* dialog = scene->findChild<QObject*>(QStringLiteral("dialog"));
    QVERIFY(dialog != nullptr);
    QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
    check_dialog_buttons(dialog, QStringLiteral("Cancel"), QStringLiteral("OK"));
}

// The owner's report: "Fetch from Anki?" drew OK against the dialog's corner.
void ControlStyleTest::anki_alert_buttons_sit_inside_in_mac_order() {
    QQuickWindow window;
    window.resize(960, 640);
    window.show();

    QQmlComponent component(
        &engine_,
        QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/AnkiSettingsAlerts.qml")));
    std::unique_ptr<QObject> alerts(component.beginCreate(engine_.rootContext()));
    QVERIFY2(alerts != nullptr, qPrintable(error_text(component)));
    auto* item = qobject_cast<QQuickItem*>(alerts.get());
    QVERIFY(item != nullptr);
    item->setParentItem(window.contentItem());
    item->setWidth(960);
    item->setHeight(640);
    component.completeCreate();

    QVERIFY(QMetaObject::invokeMethod(alerts.get(), "openFetch"));
    QObject* fetch = nullptr;
    for (QObject* child : alerts->findChildren<QObject*>()) {
        if (child->inherits("QQuickDialog") &&
            child->property("title").toString() == QStringLiteral("Fetch from Anki?")) {
            fetch = child;
        }
    }
    QVERIFY(fetch != nullptr);
    check_dialog_buttons(fetch, QStringLiteral("Cancel"), QStringLiteral("OK"));
}

// macOS: a click on a button, switch, checkbox, slider or pop-up button does
// not move the keyboard focus (only Full Keyboard Access lets Tab reach
// them), so the view that held the keyboard, such as the video player with
// its shortcuts, keeps it. Tab still reaches the controls here.
void ControlStyleTest::clicking_a_control_leaves_the_keyboard_alone_data() {
    QTest::addColumn<QByteArray>("control");
    QTest::newRow("Button") << QByteArray("Button { text: \"Go\" }");
    QTest::newRow("ToolButton") << QByteArray("ToolButton { text: \"Go\" }");
    QTest::newRow("CheckBox") << QByteArray("CheckBox { text: \"On\" }");
    QTest::newRow("Switch") << QByteArray("Switch { text: \"On\" }");
    QTest::newRow("RadioButton") << QByteArray("RadioButton { text: \"On\" }");
    QTest::newRow("Slider") << QByteArray("Slider { width: 160 }");
    QTest::newRow("TabButton") << QByteArray("TabButton { text: \"Tab\"; width: 120 }");
    QTest::newRow("ComboBox") << QByteArray("ComboBox { model: [\"A\", \"B\"] }");
    // The control kit draws these on the bare templates, outside the style.
    QTest::newRow("FormButton") << QByteArray("FormButton { text: \"Go\" }");
    QTest::newRow("LinkButton") << QByteArray("LinkButton { text: \"Go\" }");
    QTest::newRow("ToggleSwitch") << QByteArray("ToggleSwitch { }");
    QTest::newRow("TrackSlider") << QByteArray("TrackSlider { width: 160 }");
    QTest::newRow("MenuPicker") << QByteArray("MenuPicker { model: [\"A\", \"B\"] }");
}

void ControlStyleTest::clicking_a_control_leaves_the_keyboard_alone() {
    QFETCH(QByteArray, control);
    QQuickWindow window;
    window.resize(400, 300);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const QByteArray qml = "import QtQuick\nimport QtQuick.Controls\nimport Niratan\n"
                           "Item {\n"
                           "    width: 400; height: 300\n"
                           "    Item { objectName: \"owner\"; focus: true }\n"
                           "    " +
        control.replace(" {", " { objectName: \"control\"; x: 40; y: 40;") + "\n}\n";
    auto scene = create(qml, window);
    QVERIFY(scene != nullptr);
    auto* owner = scene->findChild<QQuickItem*>(QStringLiteral("owner"));
    auto* clicked = scene->findChild<QQuickItem*>(QStringLiteral("control"));
    QVERIFY(owner != nullptr);
    QVERIFY(clicked != nullptr);
    owner->forceActiveFocus();
    QTRY_VERIFY(owner->hasActiveFocus());

    QTest::mouseClick(
        &window, Qt::LeftButton, {},
        clicked->mapToScene(QPointF(clicked->width() / 2, clicked->height() / 2)).toPoint());
    // A pop-up button's open menu holds the keyboard until it closes.
    if (!owner->hasActiveFocus()) {
        QTest::keyClick(&window, Qt::Key_Escape);
    }
    QTRY_VERIFY2(owner->hasActiveFocus(), "the click moved the keyboard focus to the control");
    QVERIFY(clicked->property("focusPolicy").toInt() & Qt::TabFocus);
}

// A text field still takes the keyboard on a click, as on macOS.
void ControlStyleTest::clicking_a_text_field_takes_the_keyboard() {
    QQuickWindow window;
    window.resize(400, 300);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto scene = create("import QtQuick\nimport QtQuick.Controls\n"
                        "Item {\n"
                        "    width: 400; height: 300\n"
                        "    Item { objectName: \"owner\"; focus: true }\n"
                        "    TextField { objectName: \"field\"; x: 40; y: 40; width: 160 }\n"
                        "}\n",
                        window);
    QVERIFY(scene != nullptr);
    auto* owner = scene->findChild<QQuickItem*>(QStringLiteral("owner"));
    auto* field = scene->findChild<QQuickItem*>(QStringLiteral("field"));
    QVERIFY(owner != nullptr);
    QVERIFY(field != nullptr);
    owner->forceActiveFocus();
    QTest::mouseClick(
        &window, Qt::LeftButton, {},
        field->mapToScene(QPointF(field->width() / 2, field->height() / 2)).toPoint());
    QTRY_VERIFY(field->hasActiveFocus());
}

int main(int argc, char* argv[]) {
    niratan::ui::register_popup_url_schemes();
    niratan::ui::register_reader_url_schemes();
    QtWebEngineQuick::initialize();

    QTemporaryDir data_root;
    if (data_root.isValid()) {
        const QByteArray base = data_root.path().toUtf8();
        qputenv("HOME", base);
        qputenv("XDG_DATA_HOME", base + "/data");
        qputenv("XDG_CACHE_HOME", base + "/cache");
    }

    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    ControlStyleTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_control_style.moc"

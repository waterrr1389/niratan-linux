// SPDX-License-Identifier: GPL-3.0-or-later
//
// The control kit in src/views/components and its sample sheet
// (src/views/theme/ControlKitSheet.qml), following
// NativeMac/NativeReuseViews.swift (NativeSettingsForm,
// NativeSettingsSectionCard.cardsBody, NativeSettingsRow,
// NativeSettingsSubtitledLabel, NativeSettingsToggle, NativeSettingsSliderRow,
// NativeGlassMenuPicker, NativeGlassSegmentedPicker, NativeSettingsButtonRow,
// NativeSettingsActionButtonStyle, NativeSettingsSeparator),
// NativeMac/NativeReaderInspector.swift (NativeReaderInspectorHeader) and
// Features/Video/VideoInspectorView.swift (body, inspectorSection, labeledRow,
// rowLabel, sliderContent, toggleRow, colorRow, rowDivider), with the style C
// text sizes and row heights of the task's decisions.
//
// Layout values are not frozen here: the tests assert relations between items
// and the Theme tokens, except where a decision fixes a value (the type scale,
// the 840 px form width). Every slot fails on any QML warning.

#include <QDesktopServices>
#include <QFont>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

#include <cmath>
#include <memory>

Q_IMPORT_QML_PLUGIN(NiratanPlugin)
Q_IMPORT_QML_PLUGIN(NiratanStylePlugin)

namespace {

QStringList g_warnings;
QtMessageHandler g_previous_handler = nullptr;

// QML reports compile errors and binding failures through the message
// handler, as tst_qml_load.cpp collects them.
void collect_qml_messages(QtMsgType type, const QMessageLogContext& context,
                          const QString& message) {
    if (type == QtWarningMsg || type == QtCriticalMsg) {
        const QString category = QString::fromUtf8(context.category ? context.category : "");
        if (category.startsWith(QStringLiteral("qml")) ||
            message.startsWith(QStringLiteral("QML")) || message.contains(QStringLiteral(".qml"))) {
            g_warnings.append(message);
        }
    }
    if (g_previous_handler != nullptr) {
        g_previous_handler(type, context, message);
    }
}

QString error_text(const QQmlComponent& component) {
    QStringList lines;
    for (const QQmlError& error : component.errors()) {
        lines.append(error.toString());
    }
    return lines.join('\n');
}

// True when object is an instance of the QML type name or of a type derived
// from it.
bool is_kit(const QObject* object, const QString& name) {
    const QString prefix = name + QStringLiteral("_QML");
    for (const QMetaObject* meta = object->metaObject(); meta != nullptr;
         meta = meta->superClass()) {
        if (QString::fromLatin1(meta->className()).startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

QList<QQuickItem*> kit_items(QObject* root, const QString& name) {
    QList<QQuickItem*> items;
    for (QQuickItem* item : root->findChildren<QQuickItem*>()) {
        if (is_kit(item, name)) {
            items.append(item);
        }
    }
    return items;
}

QQuickItem* item_property(QObject* object, const char* property) {
    return object->property(property).value<QQuickItem*>();
}

QRectF rect_in(QQuickItem* item, QQuickItem* reference) {
    return item->mapRectToItem(reference, QRectF(0, 0, item->width(), item->height()));
}

qreal baseline_in(QQuickItem* label, QQuickItem* reference) {
    return label->mapToItem(reference, QPointF(0, label->baselineOffset())).y();
}

QPoint centre_in_window(QQuickItem* item) {
    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
}

// The nearest enclosing FormSection of item, or null.
QQuickItem* section_of(QQuickItem* item) {
    for (QQuickItem* parent = item->parentItem(); parent != nullptr;
         parent = parent->parentItem()) {
        if (is_kit(parent, QStringLiteral("FormSection"))) {
            return parent;
        }
    }
    return nullptr;
}

bool inside_flickable(QQuickItem* item) {
    for (QQuickItem* parent = item->parentItem(); parent != nullptr;
         parent = parent->parentItem()) {
        if (parent->inherits("QQuickFlickable")) {
            return true;
        }
    }
    return false;
}

// True when item or one of its descendants is a visible checkmark glyph.
bool shows_checkmark(QQuickItem* item) {
    QList<QQuickItem*> items = item->findChildren<QQuickItem*>();
    items.prepend(item);
    for (QQuickItem* candidate : items) {
        if (is_kit(candidate, QStringLiteral("Icon")) &&
            candidate->property("name").toString() == QStringLiteral("checkmark") &&
            candidate->isVisible() && candidate->width() > 0 && candidate->opacity() > 0) {
            return true;
        }
    }
    return false;
}

const QByteArray kImports = "import QtQuick\nimport QtQuick.Controls\nimport Niratan\n";

}  // namespace

// Records the URLs Qt.openUrlExternally hands to QDesktopServices.
class UrlRecorder : public QObject {
    Q_OBJECT

   public:
    QList<QUrl> urls;

   public slots:
    void open(const QUrl& url) { urls.append(url); }
};

// Routes https URLs to a recorder for the lifetime of the object, so a test
// never opens a browser.
class ScopedUrlHandler {
   public:
    explicit ScopedUrlHandler(UrlRecorder* recorder) {
        QDesktopServices::setUrlHandler(QStringLiteral("https"), recorder, "open");
    }
    ~ScopedUrlHandler() { QDesktopServices::unsetUrlHandler(QStringLiteral("https")); }
    ScopedUrlHandler(const ScopedUrlHandler&) = delete;
    ScopedUrlHandler& operator=(const ScopedUrlHandler&) = delete;
};

class ControlKitTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void init();
    void cleanup();

    void components_load_data();
    void components_load();
    void type_scale();
    void styled_label();
    void row_density_follows_section();
    void trailing_column_is_shared_data();
    void trailing_column_is_shared();
    void labels_do_not_push_controls();
    void glyph_column();
    void row_breathing_room();
    void controls_emit_on_user_action();
    void editable_stepper_accepts_integers();
    void colour_well_opens_dialog();
    void links_open_their_url();
    void panel_header_closes();
    void segment_dividers();
    void panel_scrolls_with_bottom_padding();
    void section_header_and_footer();
    void sheet_fits();

   private:
    std::unique_ptr<QQuickWindow> make_window();
    std::unique_ptr<QObject> create(const QByteArray& body, QQuickWindow& window);
    std::unique_ptr<QObject> create_sheet(QQmlEngine& engine, QQuickWindow& window, QString& error);
    QObject* theme();
    qreal token(const char* name);
    QFont font_token(const char* name);
    QColor colour_token(const char* name);

    QQmlEngine engine_;
};

void ControlKitTest::initTestCase() {
    // The style is chosen before any component imports QtQuick.Controls.
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));
    qunsetenv("NIRATAN_THEME");
    g_previous_handler = qInstallMessageHandler(collect_qml_messages);
}

void ControlKitTest::init() {
    g_warnings.clear();
}

void ControlKitTest::cleanup() {
    QTest::qWait(20);
    const QString warnings = g_warnings.join('\n');
    g_warnings.clear();
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

std::unique_ptr<QQuickWindow> ControlKitTest::make_window() {
    auto window = std::make_unique<QQuickWindow>();
    window->resize(960, 640);
    window->show();
    if (!QTest::qWaitForWindowExposed(window.get())) {
        return {};
    }
    return window;
}

// Creates `body` (an item with its imports prepended) inside window.
std::unique_ptr<QObject> ControlKitTest::create(const QByteArray& body, QQuickWindow& window) {
    QQmlComponent component(&engine_);
    component.setData(kImports + body, QUrl(QStringLiteral("qrc:/test/Scene.qml")));
    std::unique_ptr<QObject> object(component.beginCreate(engine_.rootContext()));
    if (!object) {
        qWarning().noquote() << "QML scene:" << error_text(component);
        return {};
    }
    if (auto* item = qobject_cast<QQuickItem*>(object.get())) {
        item->setParentItem(window.contentItem());
    }
    component.completeCreate();
    QTest::qWait(50);
    return object;
}

std::unique_ptr<QObject> ControlKitTest::create_sheet(QQmlEngine& engine, QQuickWindow& window,
                                                      QString& error) {
    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/theme/ControlKitSheet.qml")));
    std::unique_ptr<QObject> sheet(component.beginCreate(engine.rootContext()));
    if (!sheet) {
        error = error_text(component);
        return {};
    }
    auto* item = qobject_cast<QQuickItem*>(sheet.get());
    if (item == nullptr) {
        error = QStringLiteral("ControlKitSheet is not an Item");
        return {};
    }
    item->setParentItem(window.contentItem());
    item->setWidth(window.width());
    item->setHeight(window.height());
    component.completeCreate();
    QTest::qWait(80);
    return sheet;
}

QObject* ControlKitTest::theme() {
    return engine_.singletonInstance<QObject*>(QStringLiteral("Niratan"), QStringLiteral("Theme"));
}

qreal ControlKitTest::token(const char* name) {
    QObject* object = theme();
    return object != nullptr ? object->property(name).toReal() : 0;
}

QFont ControlKitTest::font_token(const char* name) {
    QObject* object = theme();
    return object != nullptr ? object->property(name).value<QFont>() : QFont();
}

QColor ControlKitTest::colour_token(const char* name) {
    QObject* object = theme();
    return object != nullptr ? object->property(name).value<QColor>() : QColor();
}

// T1: every kit component loads from its file and is created with typical
// properties, rows inside a 600 wide FormSection.
void ControlKitTest::components_load_data() {
    QTest::addColumn<QString>("name");
    QTest::addColumn<QByteArray>("properties");
    QTest::addColumn<bool>("in_section");

    const auto add = [](const char* name, const QByteArray& properties, bool in_section) {
        QTest::newRow(name) << QString::fromLatin1(name) << properties << in_section;
    };
    add("StyledLabel", "text: \"Label\"", false);
    add("FormSection", "width: 600; title: \"Section\"; footer: \"Footer\"", false);
    add("MenuPicker", "model: [\"A\", \"B\"]; currentIndex: 0", false);
    add("SegmentedPicker", "model: [\"A\", \"B\"]; currentIndex: 0", false);
    add("StepperControl", "value: 1; from: 0; to: 2; stepSize: 1; valueText: \"1\"", false);
    add("ColorWell", "color: \"#80FF0000\"", false);
    add("FormButton", "text: \"Apply\"", false);
    add("LinkButton", "text: \"Site\"; url: \"https://example.com\"", false);
    add("PanelScaffold", "width: 340; height: 400", false);
    add("PanelHeader", "width: 340; title: \"Video\"; subtitle: \"Episode\"", false);
    add("FormRow", "label: \"Row\"", true);
    add("FormDivider", "", true);
    add("FormToggleRow", "label: \"Toggle\"; checked: true", true);
    add("FormSliderRow", "label: \"Size\"; from: 0; to: 10; value: 5; valueText: \"5\"", true);
    add("FormPickerRow", "label: \"Font\"; model: [\"Inter\"]; currentIndex: 0", true);
    add("FormSegmentedRow", "label: \"Mode\"; model: [\"A\", \"B\"]; currentIndex: 0", true);
    add("FormStepperRow",
        "label: \"Seek\"; from: 0; to: 60; value: 10; stepSize: 1; valueText: \"10 s\"", true);
    add("FormColorRow", "label: \"Colour\"; color: \"#FF0000\"", true);
    add("FormButtonRow", "FormButton { text: \"Apply\" }", true);
    add("FormLinkRow", "label: \"Site\"; url: \"https://example.com\"", true);
}

void ControlKitTest::components_load() {
    QFETCH(QString, name);
    QFETCH(QByteArray, properties);
    QFETCH(bool, in_section);

    QQmlComponent file(
        &engine_,
        QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/components/%1.qml").arg(name)));
    QVERIFY2(file.isReady(), qPrintable(error_text(file)));

    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    const QByteArray component =
        name.toLatin1() + " { objectName: \"component\"; " + properties + " }";
    const QByteArray body = in_section ? "FormSection { width: 600\n" + component + "\n}"
                                       : "Item { width: 960; height: 640\n" + component + "\n}";
    std::unique_ptr<QObject> scene = create(body, *window);
    QVERIFY(scene != nullptr);
    QObject* created = scene->findChild<QObject*>(QStringLiteral("component"));
    QVERIFY2(created != nullptr, qPrintable(name + QStringLiteral(" was not created")));
    QVERIFY(is_kit(created, name));
}

// T2: the style C type scale and the weight tokens.
void ControlKitTest::type_scale() {
    struct Style {
        const char* token;
        int size;
        const char* weight;
    };
    const Style styles[] = {
        {"largeTitleFont", 28, "weightRegular"}, {"title1Font", 24, "weightRegular"},
        {"title2Font", 19, "weightRegular"},     {"title3Font", 16, "weightRegular"},
        {"headlineFont", 14, "weightSemibold"},  {"bodyFont", 14, "weightRegular"},
        {"calloutFont", 13, "weightRegular"},    {"subheadlineFont", 12, "weightRegular"},
        {"footnoteFont", 11, "weightRegular"},   {"captionFont", 11, "weightRegular"},
        {"caption2Font", 11, "weightMedium"},
    };
    QVERIFY(theme() != nullptr);
    QCOMPARE(int(token("weightRegular")), int(QFont::Normal));
    QCOMPARE(int(token("weightMedium")), int(QFont::Medium));
    QCOMPARE(int(token("weightSemibold")), int(QFont::DemiBold));
    QCOMPARE(int(token("weightBold")), int(QFont::Bold));
    for (const Style& style : styles) {
        const QFont font = font_token(style.token);
        QVERIFY2(font.pixelSize() == style.size,
                 qPrintable(QStringLiteral("%1 is %2 px, not %3")
                                .arg(QLatin1String(style.token))
                                .arg(font.pixelSize())
                                .arg(style.size)));
        QVERIFY2(font.weight() == int(token(style.weight)),
                 qPrintable(QStringLiteral("%1 has weight %2, not %3")
                                .arg(QLatin1String(style.token))
                                .arg(font.weight())
                                .arg(QLatin1String(style.weight))));
    }
    const auto size = [this](const char* name) { return font_token(name).pixelSize(); };
    QVERIFY(size("largeTitleFont") > size("title1Font"));
    QVERIFY(size("title1Font") > size("title2Font"));
    QVERIFY(size("title2Font") > size("title3Font"));
    QVERIFY(size("title3Font") > size("bodyFont"));
    QCOMPARE(size("bodyFont"), size("headlineFont"));
    QVERIFY(size("bodyFont") > size("calloutFont"));
    QVERIFY(size("calloutFont") > size("subheadlineFont"));
    QVERIFY(size("subheadlineFont") > size("footnoteFont"));
    QCOMPARE(size("footnoteFont"), size("captionFont"));
    QCOMPARE(size("captionFont"), size("caption2Font"));
}

// B2: a StyledLabel takes size and weight from its text style, keeps the
// application font family, and draws secondary, tertiary and tabular digits.
void ControlKitTest::styled_label() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    std::unique_ptr<QObject> scene =
        create("Column {\n"
               "  StyledLabel { objectName: \"body\"; text: \"Body\" }\n"
               "  StyledLabel { objectName: \"caption\"; text: \"Caption\";"
               " textStyle: Theme.captionFont }\n"
               "  StyledLabel { objectName: \"heavy\"; text: \"Heavy\";"
               " textStyle: Theme.captionFont; weight: Theme.weightBold }\n"
               "  StyledLabel { objectName: \"secondary\"; text: \"Second\"; secondary: true }\n"
               "  StyledLabel { objectName: \"tertiary\"; text: \"Third\"; tertiary: true }\n"
               "  StyledLabel { objectName: \"digits\"; text: \"0123\"; monospacedDigits: true }\n"
               "  Label { objectName: \"plain\"; text: \"Plain\" }\n"
               "}",
               *window);
    QVERIFY(scene != nullptr);
    const auto label = [&](const char* name) {
        return scene->findChild<QQuickItem*>(QString::fromLatin1(name));
    };
    const auto font_of = [](QQuickItem* item) { return item->property("font").value<QFont>(); };
    const auto colour_of = [](QQuickItem* item) { return item->property("color").value<QColor>(); };
    for (const char* name : {"body", "caption", "heavy", "secondary", "tertiary", "digits"}) {
        QVERIFY2(label(name) != nullptr, name);
        QCOMPARE(font_of(label(name)).family(), font_of(label("plain")).family());
    }
    QCOMPARE(font_of(label("body")).pixelSize(), font_token("bodyFont").pixelSize());
    QCOMPARE(font_of(label("body")).weight(), font_token("bodyFont").weight());
    QCOMPARE(font_of(label("caption")).pixelSize(), font_token("captionFont").pixelSize());
    QCOMPARE(font_of(label("caption")).weight(), font_token("captionFont").weight());
    QCOMPARE(font_of(label("heavy")).weight(), int(token("weightBold")));
    QCOMPARE(colour_of(label("body")), colour_token("textPrimary"));
    QCOMPARE(colour_of(label("secondary")), colour_token("textSecondary"));
    QCOMPARE(colour_of(label("tertiary")), colour_token("textTertiary"));
    const QFont::Tag tabular("tnum");
    QCOMPARE(font_of(label("digits")).featureValue(tabular), 1u);
    QVERIFY(!font_of(label("body")).isFeatureSet(tabular));
}

// T3: a row takes the density of its section.
void ControlKitTest::row_density_follows_section() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    std::unique_ptr<QObject> scene = create("FormSection { objectName: \"section\"; width: 600\n"
                                            "  FormRow { objectName: \"row\"; label: \"Label\" }\n"
                                            "}",
                                            *window);
    QVERIFY(scene != nullptr);
    auto* row = scene->findChild<QQuickItem*>(QStringLiteral("row"));
    QVERIFY(row != nullptr);
    QVERIFY(token("rowMinHeight") > token("compactRowMinHeight"));
    QVERIFY(token("compactRowMinHeight") > 0);

    QCOMPARE(row->property("compact").toBool(), false);
    QQuickItem* label = item_property(row, "labelItem");
    QVERIFY2(label != nullptr, "the row has no labelItem");
    QFont font = label->property("font").value<QFont>();
    QCOMPARE(font.pixelSize(), font_token("bodyFont").pixelSize());
    QCOMPARE(font.weight(), int(token("weightMedium")));
    QVERIFY2(row->height() >= token("rowMinHeight"),
             qPrintable(QStringLiteral("cards row is %1 high").arg(row->height())));

    scene->setProperty("compact", true);
    QTRY_COMPARE(row->property("compact").toBool(), true);
    QTest::qWait(50);
    font = label->property("font").value<QFont>();
    QCOMPARE(font.pixelSize(), font_token("calloutFont").pixelSize());
    QCOMPARE(font.weight(), int(token("weightRegular")));
    QVERIFY2(row->height() >= token("compactRowMinHeight") && row->height() < token("rowMinHeight"),
             qPrintable(QStringLiteral("compact row is %1 high").arg(row->height())));
}

// T4: every trailing control ends on one column at the row padding and is
// centred in its row.
void ControlKitTest::trailing_column_is_shared_data() {
    QTest::addColumn<bool>("compact");
    QTest::newRow("cards") << false;
    QTest::newRow("compact") << true;
}

void ControlKitTest::trailing_column_is_shared() {
    QFETCH(bool, compact);
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    const QByteArray body =
        QByteArray("FormSection { objectName: \"section\"; width: 600; compact: ") +
        (compact ? "true" : "false") +
        "\n"
        "  FormToggleRow { objectName: \"toggle\"; label: \"Toggle\"; checked: true }\n"
        "  FormPickerRow { objectName: \"picker\"; label: \"Font\";"
        " model: [\"Inter\", \"Noto Sans\"]; currentIndex: 0 }\n"
        "  FormSegmentedRow { objectName: \"segmented\"; label: \"Mode\";"
        " model: [\"Vertical\", \"Horizontal\"]; currentIndex: 0 }\n"
        "  FormStepperRow { objectName: \"stepper\"; label: \"Seek\"; from: 1; to: 60;"
        " value: 10; stepSize: 1; valueText: \"10 s\" }\n"
        "  FormColorRow { objectName: \"colour\"; label: \"Colour\"; color: \"#FF0000\" }\n"
        "  FormLinkRow { objectName: \"link\"; label: \"Site\"; url: \"https://example.com\" }\n"
        "  FormSliderRow { objectName: \"slider\"; label: \"Size\"; from: 0; to: 10;"
        " value: 5; valueText: \"5\" }\n"
        "}";
    std::unique_ptr<QObject> scene = create(body, *window);
    QVERIFY(scene != nullptr);
    auto* section = qobject_cast<QQuickItem*>(scene.get());
    QVERIFY(section != nullptr);

    const qreal padding = token(compact ? "compactRowPadding" : "rowPadding");
    QVERIFY(padding > 0);
    const qreal column = section->width() - padding;
    for (const char* name :
         {"toggle", "picker", "segmented", "stepper", "colour", "link", "slider"}) {
        auto* row = scene->findChild<QQuickItem*>(QString::fromLatin1(name));
        QVERIFY2(row != nullptr, name);
        QVERIFY2(row->height() > 0, name);
        QQuickItem* trailing = item_property(row, "trailingItem");
        QVERIFY2(trailing != nullptr,
                 qPrintable(QStringLiteral("%1 has no trailingItem").arg(QLatin1String(name))));
        const QRectF bounds = rect_in(trailing, section);
        QVERIFY2(std::abs(bounds.right() - column) <= 0.5,
                 qPrintable(QStringLiteral("%1 ends at %2, the trailing column at %3")
                                .arg(QLatin1String(name))
                                .arg(bounds.right())
                                .arg(column)));
        if (qstrcmp(name, "slider") == 0) {
            QQuickItem* label = item_property(row, "labelItem");
            QQuickItem* value = item_property(row, "valueLabel");
            QVERIFY(label != nullptr && value != nullptr);
            QVERIFY2(value->isVisible() && value->width() > 0, "the slider value is not shown");
            QCOMPARE(value->property("text").toString(), QStringLiteral("5"));
            QVERIFY2(std::abs(rect_in(value, section).right() - column) <= 0.5,
                     qPrintable(QStringLiteral("the slider value ends at %1, the column at %2")
                                    .arg(rect_in(value, section).right())
                                    .arg(column)));
            QVERIFY2(std::abs(baseline_in(value, row) - baseline_in(label, row)) <= 1,
                     "the slider value is not on the label's baseline");
            continue;
        }
        const qreal centre = rect_in(trailing, row).center().y();
        QVERIFY2(std::abs(centre - row->height() / 2) <= 1,
                 qPrintable(QStringLiteral("%1 control centred at %2 in a %3 high row")
                                .arg(QLatin1String(name))
                                .arg(centre)
                                .arg(row->height())));
    }
}

// T5: a long label wraps and the row grows; the control stays on its column.
void ControlKitTest::labels_do_not_push_controls() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    const QByteArray long_label = QByteArray("word ").repeated(40);
    std::unique_ptr<QObject> scene =
        create("FormSection { objectName: \"section\"; width: 360; compact: true\n"
               "  FormToggleRow { objectName: \"long\"; label: \"" +
                   long_label.trimmed() +
                   "\" }\n"
                   "  FormToggleRow { objectName: \"short\"; label: \"Short\" }\n"
                   "}",
               *window);
    QVERIFY(scene != nullptr);
    auto* section = qobject_cast<QQuickItem*>(scene.get());
    auto* long_row = scene->findChild<QQuickItem*>(QStringLiteral("long"));
    auto* short_row = scene->findChild<QQuickItem*>(QStringLiteral("short"));
    QVERIFY(section != nullptr && long_row != nullptr && short_row != nullptr);
    QQuickItem* long_trailing = item_property(long_row, "trailingItem");
    QQuickItem* short_trailing = item_property(short_row, "trailingItem");
    QQuickItem* label = item_property(long_row, "labelItem");
    QVERIFY(long_trailing != nullptr && short_trailing != nullptr && label != nullptr);

    const qreal column = section->width() - token("compactRowPadding");
    QVERIFY(token("compactRowPadding") > 0);
    QVERIFY(std::abs(rect_in(long_trailing, section).right() - column) <= 0.5);
    QVERIFY(std::abs(rect_in(short_trailing, section).right() - column) <= 0.5);
    QVERIFY2(rect_in(label, long_row).right() < rect_in(long_trailing, long_row).left(),
             "the long label runs under the switch");
    QCOMPARE(label->property("lineCount").toInt(), 2);
    QVERIFY2(rect_in(label, long_row).top() >= 0 &&
                 rect_in(label, long_row).bottom() <= long_row->height() + 0.5,
             qPrintable(QStringLiteral("the two-line label spans %1..%2 in a %3 high row")
                            .arg(rect_in(label, long_row).top())
                            .arg(rect_in(label, long_row).bottom())
                            .arg(long_row->height())));
    QVERIFY2(long_row->height() >= short_row->height(),
             qPrintable(QStringLiteral("the long row is %1 high, a one-line row %2")
                            .arg(long_row->height())
                            .arg(short_row->height())));
}

// T6: labels of rows with glyphs start on one column after the glyph column;
// without a glyph the label starts at the row padding.
void ControlKitTest::glyph_column() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    std::unique_ptr<QObject> scene = create(
        "FormSection { width: 360; compact: true\n"
        "  FormRow { objectName: \"text\"; glyph: \"textformat\"; label: \"Text\" }\n"
        "  FormRow { objectName: \"clock\"; glyph: \"clock\"; label: \"Clock\" }\n"
        "  FormRow { objectName: \"palette\"; glyph: \"paintpalette\"; label: \"Palette\" }\n"
        "  FormRow { objectName: \"plain\"; label: \"Plain\" }\n"
        "}",
        *window);
    QVERIFY(scene != nullptr);
    const auto label_x = [&](const char* name) -> qreal {
        auto* row = scene->findChild<QQuickItem*>(QString::fromLatin1(name));
        QQuickItem* label = row != nullptr ? item_property(row, "labelItem") : nullptr;
        return label != nullptr ? rect_in(label, row).left() : -1;
    };
    const qreal padding = token("compactRowPadding");
    QVERIFY(padding > 0 && token("glyphColumnWidth") > 0);
    const qreal glyph_label = padding + token("glyphColumnWidth") + token("glyphSpacing");
    for (const char* name : {"text", "clock", "palette"}) {
        QVERIFY2(std::abs(label_x(name) - glyph_label) <= 0.5,
                 qPrintable(QStringLiteral("%1 label at %2, expected %3")
                                .arg(QLatin1String(name))
                                .arg(label_x(name))
                                .arg(glyph_label)));
    }
    QVERIFY2(std::abs(label_x("plain") - padding) <= 0.5,
             qPrintable(QStringLiteral("plain label at %1").arg(label_x("plain"))));
}

// T7: in the sample sheet no label touches its row's edges and every divider
// runs from the row padding to the section's right edge.
void ControlKitTest::row_breathing_room() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    QString error;
    std::unique_ptr<QObject> sheet = create_sheet(engine_, *window, error);
    QVERIFY2(sheet != nullptr, qPrintable(error));

    QList<QQuickItem*> rows = kit_items(sheet.get(), QStringLiteral("FormRow"));
    rows.append(kit_items(sheet.get(), QStringLiteral("FormSliderRow")));
    int checked_rows = 0;
    for (QQuickItem* row : rows) {
        QQuickItem* label = item_property(row, "labelItem");
        if (label == nullptr || !label->isVisible() ||
            label->property("text").toString().isEmpty()) {
            continue;
        }
        ++checked_rows;
        const QRectF bounds = rect_in(label, row);
        const QString text = label->property("text").toString();
        QVERIFY2(bounds.top() >= 8 - 0.01,
                 qPrintable(QStringLiteral("\"%1\" starts %2 below its row's top")
                                .arg(text)
                                .arg(bounds.top())));
        QVERIFY2(bounds.bottom() <= row->height() - 8 + 0.01,
                 qPrintable(QStringLiteral("\"%1\" ends %2 above its row's bottom")
                                .arg(text)
                                .arg(row->height() - bounds.bottom())));
    }
    QVERIFY2(checked_rows >= 10,
             qPrintable(QStringLiteral("the sheet shows %1 labelled rows").arg(checked_rows)));

    const QList<QQuickItem*> dividers = kit_items(sheet.get(), QStringLiteral("FormDivider"));
    QVERIFY2(dividers.size() >= 4, "the sheet has too few dividers");
    for (QQuickItem* divider : dividers) {
        QQuickItem* section = section_of(divider);
        QVERIFY2(section != nullptr, "a divider outside a FormSection");
        const qreal padding =
            token(section->property("compact").toBool() ? "compactRowPadding" : "rowPadding");
        const QRectF bounds = rect_in(divider, section);
        QVERIFY2(std::abs(bounds.left() - padding) <= 0.5 &&
                     std::abs(bounds.right() - section->width()) <= 0.5,
                 qPrintable(QStringLiteral("divider spans %1..%2 in a %3 wide section")
                                .arg(bounds.left())
                                .arg(bounds.right())
                                .arg(section->width())));
    }
}

void ControlKitTest::editable_stepper_accepts_integers() {
    auto window = make_window();
    QVERIFY(window != nullptr);
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.get()));
    auto scene =
        create("Item { width: 500; height: 300\n"
               "  FormStepperRow { objectName: \"row\"; width: parent.width; label: \"Size\";"
               "    from: 16; to: 40; value: 24; valueText: String(value); editable: true;"
               "    onStepped: next => value = next }\n"
               "  Item { objectName: \"other\"; y: 100 }\n"
               "}",
               *window);
    QVERIFY(scene != nullptr);
    auto* row = scene->findChild<QQuickItem*>(QStringLiteral("row"));
    auto* other = scene->findChild<QQuickItem*>(QStringLiteral("other"));
    QVERIFY(row != nullptr);
    QVERIFY(other != nullptr);
    auto* stepper = item_property(row, "trailingItem");
    QVERIFY(stepper != nullptr);
    auto* label = stepper->findChild<QQuickItem*>(QStringLiteral("stepperValueLabel"));
    auto* editor = stepper->findChild<QQuickItem*>(QStringLiteral("stepperValueEditor"));
    QVERIFY(label != nullptr);
    QVERIFY(editor != nullptr);
    QSignalSpy changes(row, SIGNAL(stepped(double)));

    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(label));
    QTRY_VERIFY(editor->hasActiveFocus());
    QTest::keyClick(window.get(), Qt::Key_3);
    QTest::keyClick(window.get(), Qt::Key_1);
    QTest::keyClick(window.get(), Qt::Key_Return);
    QCOMPARE(changes.size(), 1);
    QCOMPARE(row->property("value").toDouble(), 31.0);
    QVERIFY(!editor->isVisible());

    stepper->forceActiveFocus();
    QTest::keyClick(window.get(), Qt::Key_9);
    QTRY_VERIFY(editor->hasActiveFocus());
    QTest::keyClick(window.get(), Qt::Key_9);
    other->forceActiveFocus();
    QCOMPARE(changes.size(), 2);
    QCOMPARE(row->property("value").toDouble(), 40.0);
    QVERIFY(!editor->isVisible());

    for (const QString& rejected : {QStringLiteral("23"), QStringLiteral("20.5"), QString()}) {
        QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(label));
        QTRY_VERIFY(editor->hasActiveFocus());
        QVERIFY(editor->setProperty("text", rejected));
        QTest::keyClick(window.get(),
                        rejected == QStringLiteral("23") ? Qt::Key_Escape : Qt::Key_Return);
        QCOMPARE(changes.size(), 2);
        QCOMPARE(row->property("value").toDouble(), 40.0);
        QVERIFY(!editor->isVisible());
    }
}

// T8: controls emit on user actions only.
void ControlKitTest::controls_emit_on_user_action() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.get()));
    std::unique_ptr<QObject> scene =
        create("Item { width: 960; height: 640\n"
               "  FormSection { width: 600\n"
               "    FormToggleRow { objectName: \"toggle\"; label: \"Toggle\" }\n"
               "    FormSliderRow { objectName: \"slider\"; label: \"Size\"; from: 0; to: 10;"
               " stepSize: 1; value: 0; valueText: \"0\" }\n"
               "    FormStepperRow { objectName: \"stepper\"; label: \"Seek\"; from: 1; to: 60;"
               " stepSize: 1; value: 59; valueText: \"59\" }\n"
               "  }\n"
               "  SegmentedPicker { objectName: \"segments\"; x: 20; y: 400;"
               " model: [\"One\", \"Two\", \"Three\"]; currentIndex: 0 }\n"
               "  SegmentedPicker { objectName: \"limited\"; x: 20; y: 460;"
               " model: [\"One\", \"Two\", \"Three\"]; currentIndex: 0;"
               " enabledSegments: [true, false, true] }\n"
               "  MenuPicker { objectName: \"menu\"; x: 600; y: 300; model: [\"A\", \"B\", \"C\"];"
               " currentIndex: 1 }\n"
               "  FormButton { objectName: \"button\"; x: 20; y: 540; text: \"Apply\" }\n"
               "  FormButton { objectName: \"disabled\"; x: 200; y: 540; text: \"Reset\";"
               " enabled: false }\n"
               "}",
               *window);
    QVERIFY(scene != nullptr);
    const auto find = [&](const char* name) {
        return scene->findChild<QQuickItem*>(QString::fromLatin1(name));
    };

    // Toggle.
    QQuickItem* toggle = find("toggle");
    QVERIFY(toggle != nullptr);
    QQuickItem* track = item_property(toggle, "trailingItem");
    QVERIFY(track != nullptr);
    QSignalSpy toggled(toggle, SIGNAL(toggled(bool)));
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(track));
    QTRY_COMPARE(toggled.count(), 1);
    QCOMPARE(toggled.first().first().toBool(), true);
    toggle->setProperty("checked", false);
    toggle->setProperty("checked", true);
    QTest::qWait(20);
    QCOMPARE(toggled.count(), 1);

    // Slider.
    QQuickItem* slider_row = find("slider");
    QVERIFY(slider_row != nullptr);
    QQuickItem* slider = item_property(slider_row, "slider");
    QVERIFY(slider != nullptr && slider->width() > 0);
    QSignalSpy moved(slider_row, SIGNAL(moved(double)));
    const QPointF start = slider->mapToScene(QPointF(4, slider->height() / 2));
    const QPointF middle = slider->mapToScene(QPointF(slider->width() / 2, slider->height() / 2));
    QTest::mousePress(window.get(), Qt::LeftButton, {}, start.toPoint());
    for (int step = 1; step <= 8; ++step) {
        const QPointF point = start + (middle - start) * (step / 8.0);
        QTest::mouseMove(window.get(), point.toPoint());
    }
    QTest::mouseRelease(window.get(), Qt::LeftButton, {}, middle.toPoint());
    QTRY_VERIFY(moved.count() >= 1);
    const double last = moved.last().first().toDouble();
    QVERIFY2(last >= 4 && last <= 6 && std::abs(last - std::round(last)) < 1e-9,
             qPrintable(QStringLiteral("dragged to the middle, moved(%1)").arg(last)));

    // Stepper.
    QQuickItem* stepper_row = find("stepper");
    QVERIFY(stepper_row != nullptr);
    QQuickItem* stepper = item_property(stepper_row, "trailingItem");
    QVERIFY(stepper != nullptr);
    QQuickItem* up = item_property(stepper, "upButton");
    QQuickItem* down = item_property(stepper, "downButton");
    QVERIFY(up != nullptr && down != nullptr);
    QSignalSpy stepped(stepper_row, SIGNAL(stepped(double)));
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(up));
    QTRY_COMPARE(stepped.count(), 1);
    QCOMPARE(stepped.first().first().toDouble(), 60.0);
    stepper_row->setProperty("value", 60);
    QTRY_VERIFY(!up->isEnabled());
    QVERIFY(down->isEnabled());
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(up));
    QTest::qWait(20);
    QCOMPARE(stepped.count(), 1);
    stepper_row->setProperty("value", 2);
    QTRY_VERIFY(up->isEnabled() && down->isEnabled());
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(down));
    QTRY_COMPARE(stepped.count(), 2);
    QCOMPARE(stepped.last().first().toDouble(), 1.0);
    stepper_row->setProperty("value", 1);
    QTRY_VERIFY(!down->isEnabled());
    QVERIFY(up->isEnabled());
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(down));
    QTest::qWait(20);
    QCOMPARE(stepped.count(), 2);

    // Segmented picker.
    QQuickItem* segments = find("segments");
    QVERIFY(segments != nullptr);
    QSignalSpy activated(segments, SIGNAL(activated(int)));
    QQuickItem* third = nullptr;
    QVERIFY(QMetaObject::invokeMethod(segments, "segmentItem", Q_RETURN_ARG(QQuickItem*, third),
                                      Q_ARG(int, 2)));
    QVERIFY(third != nullptr);
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(third));
    QTRY_COMPARE(activated.count(), 1);
    QCOMPARE(activated.first().first().toInt(), 2);
    segments->setProperty("currentIndex", 2);
    segments->forceActiveFocus();
    QTRY_VERIFY(segments->hasActiveFocus() || window->activeFocusItem() != nullptr);
    QTest::keyClick(window.get(), Qt::Key_Right);
    QTest::qWait(20);
    QCOMPARE(activated.count(), 1);
    QTest::keyClick(window.get(), Qt::Key_Left);
    QTRY_COMPARE(activated.count(), 2);
    QCOMPARE(activated.last().first().toInt(), 1);

    QQuickItem* limited = find("limited");
    QVERIFY(limited != nullptr);
    QSignalSpy limited_activated(limited, SIGNAL(activated(int)));
    QQuickItem* second = nullptr;
    QVERIFY(QMetaObject::invokeMethod(limited, "segmentItem", Q_RETURN_ARG(QQuickItem*, second),
                                      Q_ARG(int, 1)));
    QVERIFY(second != nullptr);
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(second));
    QTest::qWait(20);
    QCOMPARE(limited_activated.count(), 0);

    // Menu picker.
    QQuickItem* picker = find("menu");
    QVERIFY(picker != nullptr && picker->width() > 0);
    QSignalSpy chosen(picker, SIGNAL(activated(int)));
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(picker));
    QObject* menu = picker->property("menu").value<QObject*>();
    QVERIFY2(menu != nullptr, "the picker has no menu");
    QTRY_VERIFY(menu->property("opened").toBool());
    QCOMPARE(menu->property("count").toInt(), 3);
    QList<QQuickItem*> items;
    for (int index = 0; index < 3; ++index) {
        QQuickItem* item = nullptr;
        QVERIFY(QMetaObject::invokeMethod(menu, "itemAt", Q_RETURN_ARG(QQuickItem*, item),
                                          Q_ARG(int, index)));
        QVERIFY(item != nullptr);
        items.append(item);
    }
    QCOMPARE(items[0]->property("text").toString(), QStringLiteral("A"));
    QCOMPARE(items[1]->property("text").toString(), QStringLiteral("B"));
    QCOMPARE(items[2]->property("text").toString(), QStringLiteral("C"));
    QVERIFY2(!shows_checkmark(items[0]), "A is checked");
    QVERIFY2(shows_checkmark(items[1]), "B is not checked");
    QVERIFY2(!shows_checkmark(items[2]), "C is checked");
    QTest::qWait(50);
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(items[2]));
    QTRY_COMPARE(chosen.count(), 1);
    QCOMPARE(chosen.first().first().toInt(), 2);

    // Buttons.
    QQuickItem* button = find("button");
    QQuickItem* disabled = find("disabled");
    QVERIFY(button != nullptr && disabled != nullptr);
    QVERIFY(button->width() > 0 && disabled->width() > 0);
    QSignalSpy clicked(button, SIGNAL(clicked()));
    QSignalSpy disabled_clicked(disabled, SIGNAL(clicked()));
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(button));
    QTRY_COMPARE(clicked.count(), 1);
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(disabled));
    QTest::qWait(20);
    QCOMPARE(disabled_clicked.count(), 0);
}

// B12: the colour well opens a colour dialog with alpha on the current
// colour; accepting emits picked once, rejecting emits nothing.
void ControlKitTest::colour_well_opens_dialog() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    std::unique_ptr<QObject> scene = create(
        "FormSection { width: 600\n"
        "  FormColorRow { objectName: \"colour\"; label: \"Colour\"; color: \"#80ff0000\" }\n"
        "}",
        *window);
    QVERIFY(scene != nullptr);
    auto* row = scene->findChild<QQuickItem*>(QStringLiteral("colour"));
    QVERIFY(row != nullptr);
    QQuickItem* well = item_property(row, "trailingItem");
    QVERIFY(well != nullptr && well->width() > 0);
    QSignalSpy picked(row, SIGNAL(picked(QColor)));

    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(well));
    QObject* dialog = scene->findChild<QObject*>(QStringLiteral("colorDialog"));
    QVERIFY2(dialog != nullptr, "the well has no colorDialog");
    QTRY_VERIFY(dialog->property("visible").toBool());
    QCOMPARE(dialog->property("selectedColor").value<QColor>(), QColor("#80ff0000"));
    // ColorDialog.ShowAlphaChannel.
    QVERIFY((dialog->property("options").toInt() & 0x1) != 0);

    dialog->setProperty("selectedColor", QColor("#400000ff"));
    QVERIFY(QMetaObject::invokeMethod(dialog, "accept"));
    QTRY_COMPARE(picked.count(), 1);
    QCOMPARE(picked.first().first().value<QColor>(), QColor("#400000ff"));
    QTRY_VERIFY(!dialog->property("visible").toBool());

    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(well));
    QTRY_VERIFY(dialog->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(dialog, "reject"));
    QTRY_VERIFY(!dialog->property("visible").toBool());
    QTest::qWait(20);
    QCOMPARE(picked.count(), 1);
}

// B13: a link button and a whole link row open their URL externally.
void ControlKitTest::links_open_their_url() {
    UrlRecorder recorder;
    const ScopedUrlHandler handler(&recorder);
    const QUrl url(QStringLiteral("https://jimaku.cc/account"));
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    std::unique_ptr<QObject> scene =
        create("Item { width: 960; height: 640\n"
               "  LinkButton { objectName: \"link\"; x: 20; y: 20; text: \"Create API Key\";"
               " url: \"https://jimaku.cc/account\" }\n"
               "  FormSection { y: 100; width: 400\n"
               "    FormLinkRow { objectName: \"row\"; width: 400; label: \"Create API Key\";"
               " url: \"https://jimaku.cc/account\" }\n"
               "  }\n"
               "}",
               *window);
    QVERIFY(scene != nullptr);
    auto* link = scene->findChild<QQuickItem*>(QStringLiteral("link"));
    auto* row = scene->findChild<QQuickItem*>(QStringLiteral("row"));
    QVERIFY(link != nullptr && row != nullptr);
    QVERIFY(link->width() > 0 && row->height() > 0);

    QSignalSpy link_clicked(link, SIGNAL(clicked()));
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(link));
    QTRY_COMPARE(link_clicked.count(), 1);
    QTRY_COMPARE(recorder.urls.size(), 1);
    QCOMPARE(recorder.urls.first(), url);

    QSignalSpy row_clicked(row, SIGNAL(clicked()));
    const QPoint inside = row->mapToScene(QPointF(200, row->height() / 2)).toPoint();
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, inside);
    QTRY_COMPARE(row_clicked.count(), 1);
    QTRY_COMPARE(recorder.urls.size(), 2);
    QCOMPARE(recorder.urls.last(), url);
}

// B16: the panel header's close button is named and emits closeRequested.
void ControlKitTest::panel_header_closes() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    std::unique_ptr<QObject> scene =
        create("PanelHeader { width: 340; title: \"Video\"; subtitle: \"Episode\" }", *window);
    QVERIFY(scene != nullptr);
    QQuickItem* close = nullptr;
    for (QQuickItem* button : kit_items(scene.get(), QStringLiteral("IconButton"))) {
        if (button->property("icon").toString() == QStringLiteral("xmark")) {
            close = button;
        }
    }
    QVERIFY2(close != nullptr, "the header has no xmark IconButton");
    QCOMPARE(close->property("accessibleName").toString(), QStringLiteral("Close"));
    QSignalSpy close_requested(scene.get(), SIGNAL(closeRequested()));
    QTest::mouseClick(window.get(), Qt::LeftButton, {}, centre_in_window(close));
    QTRY_COMPARE(close_requested.count(), 1);
}

// T9: dividers next to the selected segment are hidden.
void ControlKitTest::segment_dividers() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    std::unique_ptr<QObject> scene =
        create("SegmentedPicker { model: [\"One\", \"Two\", \"Three\", \"Four\"];"
               " currentIndex: 1 }",
               *window);
    QVERIFY(scene != nullptr);
    const auto opacity = [&](int index) -> qreal {
        QQuickItem* divider = nullptr;
        QMetaObject::invokeMethod(scene.get(), "dividerItem", Q_RETURN_ARG(QQuickItem*, divider),
                                  Q_ARG(int, index));
        return divider != nullptr ? divider->opacity() : -1;
    };
    QVERIFY2(std::abs(opacity(0)) < 0.01, qPrintable(QString::number(opacity(0))));
    QVERIFY2(std::abs(opacity(1)) < 0.01, qPrintable(QString::number(opacity(1))));
    QVERIFY2(std::abs(opacity(2) - 0.42) < 0.01, qPrintable(QString::number(opacity(2))));

    scene->setProperty("currentIndex", 3);
    QTRY_VERIFY(std::abs(opacity(2)) < 0.01);
    QVERIFY2(std::abs(opacity(0) - 0.42) < 0.01, qPrintable(QString::number(opacity(0))));
    QVERIFY2(std::abs(opacity(1) - 0.42) < 0.01, qPrintable(QString::number(opacity(1))));
}

// T10: a panel scrolls only when its content is taller, keeps the bottom
// padding at the end, never scrolls sideways, and a cards form is centred at
// its maximum width.
void ControlKitTest::panel_scrolls_with_bottom_padding() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    QVERIFY(token("panelPaddingTop") > 0 && token("panelPaddingBottom") > 0);

    std::unique_ptr<QObject> tall =
        create("PanelScaffold { compact: true; width: 340; height: 400\n"
               "  Item { height: 400 }\n"
               "  Item { height: 400 }\n"
               "  Item { objectName: \"last\"; height: 400 }\n"
               "}",
               *window);
    QVERIFY(tall != nullptr);
    auto* panel = qobject_cast<QQuickItem*>(tall.get());
    QQuickItem* flickable = item_property(panel, "flickable");
    QQuickItem* column = item_property(panel, "column");
    QQuickItem* bar = item_property(panel, "scrollBar");
    auto* last = tall->findChild<QQuickItem*>(QStringLiteral("last"));
    QVERIFY2(flickable != nullptr && column != nullptr && bar != nullptr && last != nullptr,
             "the scaffold lacks its flickable, column or scroll bar");
    QVERIFY(column->height() >= 1200);
    const qreal content_height = flickable->property("contentHeight").toReal();
    QVERIFY2(std::abs(content_height -
                      (column->height() + token("panelPaddingTop") +
                       token("panelPaddingBottom"))) <= 0.5,
             qPrintable(QStringLiteral("contentHeight %1 for a %2 high column")
                            .arg(content_height)
                            .arg(column->height())));
    QVERIFY(std::abs(flickable->width() - panel->width()) <= 0.5);
    QVERIFY(std::abs(flickable->property("contentWidth").toReal() - flickable->width()) <= 0.5);
    QVERIFY2(bar->isVisible(), "a taller content shows no scroll bar");

    flickable->setProperty("contentY", content_height - flickable->height());
    QTest::qWait(50);
    const qreal bottom = last->mapToItem(panel, QPointF(0, last->height())).y();
    QVERIFY2(std::abs(bottom - (panel->height() - token("panelPaddingBottom"))) <= 0.5,
             qPrintable(QStringLiteral("scrolled to the end, the last section ends at %1 of %2")
                            .arg(bottom)
                            .arg(panel->height())));
    tall.reset();

    std::unique_ptr<QObject> short_panel =
        create("PanelScaffold { compact: true; width: 340; height: 400\n"
               "  Item { height: 100 }\n"
               "}",
               *window);
    QVERIFY(short_panel != nullptr);
    flickable = item_property(short_panel.get(), "flickable");
    bar = item_property(short_panel.get(), "scrollBar");
    QVERIFY(flickable != nullptr && bar != nullptr);
    QVERIFY(flickable->property("contentHeight").toReal() <= flickable->height());
    QVERIFY2(!bar->isVisible(), "a short content shows a scroll bar");
    short_panel.reset();

    std::unique_ptr<QObject> cards = create("PanelScaffold { width: 960; height: 640\n"
                                            "  Item { height: 100 }\n"
                                            "}",
                                            *window);
    QVERIFY(cards != nullptr);
    column = item_property(cards.get(), "column");
    QVERIFY(column != nullptr);
    const QRectF bounds = rect_in(column, qobject_cast<QQuickItem*>(cards.get()));
    const qreal width =
        std::min(960 - 2 * token("formPaddingHorizontal"), token("formMaxContentWidth"));
    QCOMPARE(width, 840.0);
    QVERIFY2(std::abs(bounds.width() - width) <= 0.5 &&
                 std::abs(bounds.left() - (960 - width) / 2) <= 0.5,
             qPrintable(QStringLiteral("cards content at x %1, %2 wide")
                            .arg(bounds.left())
                            .arg(bounds.width())));
}

// T11: header and footer styles per density; empty ones take no space.
void ControlKitTest::section_header_and_footer() {
    std::unique_ptr<QQuickWindow> window = make_window();
    QVERIFY(window != nullptr);
    std::unique_ptr<QObject> scene =
        create("Item { width: 960; height: 640\n"
               "  FormSection { objectName: \"empty\"; width: 400\n"
               "    FormRow { label: \"Row\" } }\n"
               "  FormSection { objectName: \"cards\"; y: 120; width: 400;"
               " title: \"Header\"; footer: \"Footer\"\n"
               "    FormRow { label: \"Row\" } }\n"
               "  FormSection { objectName: \"compact\"; y: 320; width: 400; compact: true;"
               " title: \"Header\"; footer: \"Footer\"\n"
               "    FormRow { label: \"Row\" } }\n"
               "}",
               *window);
    QVERIFY(scene != nullptr);

    auto* empty = scene->findChild<QQuickItem*>(QStringLiteral("empty"));
    QVERIFY(empty != nullptr);
    QQuickItem* surface = item_property(empty, "surface");
    QVERIFY2(surface != nullptr, "the section has no surface");
    QVERIFY(surface->height() > 0);
    QVERIFY(std::abs(rect_in(surface, empty).top()) <= 0.5);
    QVERIFY2(std::abs(empty->height() - surface->height()) <= 0.5,
             qPrintable(QStringLiteral("section %1 high around a %2 high body")
                            .arg(empty->height())
                            .arg(surface->height())));

    for (const char* name : {"cards", "compact"}) {
        const bool compact = qstrcmp(name, "compact") == 0;
        auto* section = scene->findChild<QQuickItem*>(QString::fromLatin1(name));
        QVERIFY(section != nullptr);
        QQuickItem* header = item_property(section, "headerLabel");
        QQuickItem* footer = item_property(section, "footerLabel");
        surface = item_property(section, "surface");
        QVERIFY2(header != nullptr && footer != nullptr && surface != nullptr, name);
        const QFont header_font = header->property("font").value<QFont>();
        QCOMPARE(header_font.pixelSize(), font_token("subheadlineFont").pixelSize());
        QCOMPARE(header_font.weight(), int(token("weightSemibold")));
        QCOMPARE(header->property("color").value<QColor>(),
                 colour_token(compact ? "textPrimary" : "textSecondary"));
        QCOMPARE(footer->property("font").value<QFont>().pixelSize(),
                 font_token(compact ? "captionFont" : "footnoteFont").pixelSize());
        QCOMPARE(footer->property("color").value<QColor>(), colour_token("textSecondary"));
        QVERIFY2(rect_in(header, section).bottom() <= rect_in(surface, section).top(), name);
        QVERIFY2(rect_in(footer, section).top() >= rect_in(surface, section).bottom(), name);
    }
}

// T12: the sample sheet shows every kit component and fits 960 x 640 in light
// and dark.
void ControlKitTest::sheet_fits() {
    for (const char* scheme : {"light", "dark"}) {
        qputenv("NIRATAN_THEME", scheme);
        QQmlEngine engine;
        std::unique_ptr<QQuickWindow> window = make_window();
        QVERIFY(window != nullptr);
        QString error;
        std::unique_ptr<QObject> sheet = create_sheet(engine, *window, error);
        QVERIFY2(sheet != nullptr, qPrintable(error));
        auto* state = engine.singletonInstance<QObject*>(QStringLiteral("Niratan"),
                                                         QStringLiteral("ThemeState"));
        QVERIFY(state != nullptr);
        QCOMPARE(state->property("dark").toBool(), qstrcmp(scheme, "dark") == 0);

        for (const char* name :
             {"FormSection", "FormRow", "FormDivider", "FormToggleRow", "FormSliderRow",
              "FormPickerRow", "FormSegmentedRow", "FormStepperRow", "FormColorRow", "FormLinkRow",
              "FormButtonRow", "FormButton", "LinkButton", "PanelHeader"}) {
            QVERIFY2(!kit_items(sheet.get(), QString::fromLatin1(name)).isEmpty(),
                     qPrintable(QStringLiteral("the sheet shows no %1").arg(QLatin1String(name))));
        }
        const QList<QQuickItem*> scaffolds =
            kit_items(sheet.get(), QStringLiteral("PanelScaffold"));
        QCOMPARE(scaffolds.size(), 2);
        QQuickItem* compact_panel = nullptr;
        for (QQuickItem* scaffold : scaffolds) {
            if (scaffold->property("compact").toBool()) {
                compact_panel = scaffold;
            }
        }
        QVERIFY2(compact_panel != nullptr, "the sheet has no compact panel");
        QQuickItem* flickable = item_property(compact_panel, "flickable");
        QVERIFY(flickable != nullptr);
        QVERIFY2(flickable->property("contentHeight").toReal() > flickable->height(),
                 "the compact panel is not tall enough to scroll");

        // Rows of each section are separated by dividers.
        for (QQuickItem* section : kit_items(sheet.get(), QStringLiteral("FormSection"))) {
            int rows = 0;
            int dividers = 0;
            for (QQuickItem* item : section->findChildren<QQuickItem*>()) {
                if (section_of(item) != section) {
                    continue;
                }
                if (is_kit(item, QStringLiteral("FormRow")) ||
                    is_kit(item, QStringLiteral("FormSliderRow")) ||
                    is_kit(item, QStringLiteral("FormButtonRow"))) {
                    ++rows;
                } else if (is_kit(item, QStringLiteral("FormDivider"))) {
                    ++dividers;
                }
            }
            QVERIFY2(rows > 0 && dividers == rows - 1,
                     qPrintable(QStringLiteral("section \"%1\": %2 rows, %3 dividers")
                                    .arg(section->property("title").toString())
                                    .arg(rows)
                                    .arg(dividers)));
        }

        bool japanese = false;
        for (QQuickItem* item : sheet->findChildren<QQuickItem*>()) {
            if (!item->isVisible() || item->width() <= 0 || item->height() <= 0) {
                continue;
            }
            const QRectF bounds = rect_in(item, window->contentItem());
            QVERIFY2(bounds.left() >= -0.5 && bounds.right() <= window->width() + 0.5,
                     qPrintable(QStringLiteral("%1 \"%2\" spans x %3..%4")
                                    .arg(QLatin1String(item->metaObject()->className()))
                                    .arg(item->property("text").toString())
                                    .arg(bounds.left())
                                    .arg(bounds.right())));
            if (!is_kit(item, QStringLiteral("StyledLabel"))) {
                continue;
            }
            const QString text = item->property("text").toString();
            japanese = japanese || text.contains(QStringLiteral("日本語"));
            if (!inside_flickable(item)) {
                QVERIFY2(bounds.bottom() <= window->height() + 0.5,
                         qPrintable(
                             QStringLiteral("\"%1\" ends at y %2").arg(text).arg(bounds.bottom())));
            }
            if (item->property("elide").toInt() == 0 && item->property("wrapMode").toInt() == 0) {
                QVERIFY2(item->implicitWidth() <= item->width() + 0.5,
                         qPrintable(QStringLiteral("\"%1\" needs %2 px and has %3")
                                        .arg(text)
                                        .arg(item->implicitWidth())
                                        .arg(item->width())));
            }
        }
        QVERIFY2(japanese, "no label shows Japanese text");

        QTest::qWait(20);
        const QString warnings = g_warnings.join('\n');
        g_warnings.clear();
        QVERIFY2(warnings.isEmpty(), qPrintable(QString::fromLatin1(scheme) + ":\n" + warnings));
        sheet.reset();
    }
    qunsetenv("NIRATAN_THEME");
}

QTEST_MAIN(ControlKitTest)

#include "tst_control_kit.moc"

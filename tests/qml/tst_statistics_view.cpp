// SPDX-License-Identifier: GPL-3.0-or-later
//
// The statistics dashboard page, following
// Features/Bookshelf/StatisticsDashboardView.swift: `body` (the fixed header
// above the scrolling cards), `header`, `dashboardLayout`,
// `fullWidthTrendSection`, `trendSection`, `todaySection`,
// `targetSettingsSection`, `weekSection`, `calendarSection`,
// `selectedRangeSection`, `speedSummarySection`, `bookRankingSection`,
// `shelfComparisonSection`, `emptyDashboardState`,
// `loadingDashboardPlaceholder`, `dashboardLayoutWithLoadingOverlay` and
// `isInitialSnapshotLoading`. NativeMac/NativeReuseViews.swift
// `NativeBookshelfReuseView.body` and `toolbarContent` place the dashboard in
// the bookshelf and supply the back toolbar item, and
// NativeMac/NativeMacRootView.swift gives the window toolbar.
//
// Only load, the default-window single-column order, loading/empty state and
// wiring are asserted; no pixel size or colour is. The data root is temporary
// and written directly in the stored format, so no test reads the developer's
// library.
//
// The dashboard's StatisticsViewModel is a process singleton, so one temporary
// root lives for the whole executable and every page shares it. The loading
// test runs first on the empty root; later tests install the fixture and
// refresh the same model.

#include "../viewmodels/data_root.h"

#include "services/books/book_storage.h"
#include "services/settings/global_settings.h"
#include "services/storage/app_paths.h"
#include "viewmodels/statistics/statistics_view_model.h"
#include "views/dictionary/audio_scheme_handler.h"
#include "views/reader/reader_scheme_handler.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMetaObject>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTime>
#include <QTimeZone>
#include <QUrl>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <clocale>
#include <functional>
#include <memory>

namespace {

using niratan::viewmodels::StatisticsViewModel;

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 640;
// The default window minus the 240-wide section sidebar; the dashboard page
// itself is this wide, which is below the 840 two-column breakpoint.
constexpr int kDetailWidth = 720;

QStringList g_warnings;
QtMessageHandler g_previous_handler = nullptr;

QString error_text(const QQmlComponent& component) {
    if (!component.isError()) {
        return {};
    }
    QStringList lines;
    for (const QQmlError& error : component.errors()) {
        lines.append(error.toString());
    }
    return lines.join('\n');
}

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

QString warnings_after(const QString& label) {
    QTest::qWait(50);
    QString report;
    if (!g_warnings.isEmpty()) {
        report = label + QStringLiteral(":\n") + g_warnings.join('\n');
    }
    g_warnings.clear();
    return report;
}

void collect_visual_texts(QQuickItem* item, QStringList& texts) {
    const QVariant text = item->property("text");
    if (text.isValid() && !text.toString().isEmpty()) {
        texts.append(text.toString());
    }
    for (QQuickItem* child : item->childItems()) {
        collect_visual_texts(child, texts);
    }
}

QStringList visual_texts(QQuickItem* page) {
    QStringList texts;
    collect_visual_texts(page, texts);
    return texts;
}

// Qt.formatDate and QLocale agree on the numeric day form in ordinary locales;
// accept the plain numeric fallback too so the test does not depend on locale
// digits.
bool has_day_title(const QStringList& texts, const QDate& start) {
    const QStringList forms{
        QLocale().toString(start, QStringLiteral("M/d")),
        start.toString(QStringLiteral("M/d")),
        QStringLiteral("%1/%2").arg(start.month()).arg(start.day()),
    };
    for (const QString& form : forms) {
        if (texts.contains(form)) {
            return true;
        }
    }
    return false;
}

void collect_named(QQuickItem* item, const QString& name, QList<QQuickItem*>& found) {
    if (item->objectName() == name) {
        found.append(item);
    }
    for (QQuickItem* child : item->childItems()) {
        collect_named(child, name, found);
    }
}

QList<QQuickItem*> items_named(QQuickItem* root, const QString& name) {
    QList<QQuickItem*> found;
    if (root != nullptr) {
        collect_named(root, name, found);
    }
    return found;
}

QQuickItem* item_named(QQuickItem* root, const QString& name) {
    const auto found = items_named(root, name);
    return found.isEmpty() ? nullptr : found.first();
}

bool shown(const QQuickItem* item) {
    return item != nullptr && item->isVisible() && item->opacity() > 0 && item->width() > 0 &&
        item->height() > 0;
}

QQuickItem* shown_item_named(QQuickItem* root, const QString& name) {
    for (QQuickItem* item : items_named(root, name)) {
        if (shown(item)) {
            return item;
        }
    }
    return nullptr;
}

QQuickItem* shown_control(QQuickItem* item, const char* type, const QString& text) {
    if (item->inherits(type) && item->property("text").toString() == text && shown(item)) {
        return item;
    }
    for (QQuickItem* child : item->childItems()) {
        if (QQuickItem* found = shown_control(child, type, text)) {
            return found;
        }
    }
    return nullptr;
}

QQuickItem* item_with_property(QQuickItem* item, const char* name, const QVariant& value) {
    if (item->property(name) == value && shown(item)) {
        return item;
    }
    for (QQuickItem* child : item->childItems()) {
        if (QQuickItem* found = item_with_property(child, name, value)) {
            return found;
        }
    }
    return nullptr;
}

void click(QQuickItem* item) {
    QVERIFY(item != nullptr);
    QVERIFY(QMetaObject::invokeMethod(item, "clicked"));
}

// The first kit StepperControl under root: the item exposing `upButton`.
QQuickItem* stepper_control(QQuickItem* root) {
    if (root->property("upButton").value<QQuickItem*>() != nullptr) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = stepper_control(child)) {
            return found;
        }
    }
    return nullptr;
}

bool write_bytes(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(bytes) == static_cast<qint64>(bytes.size());
}

bool write_json(const QString& path, const QJsonDocument& document) {
    return write_bytes(path, document.toJson(QJsonDocument::Indented));
}

// A one-pixel PNG so the ranking cover loads instead of warning.
QByteArray cover_png() {
    return QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YA"
        "AAAASUVORK5CYII=");
}

qint64 noon_ms(const QDate& date) {
    return QDateTime(date, QTime(12, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
}

QJsonObject daily_row(const QString& title, const QDate& date, int characters, double seconds,
                      int speed) {
    return {
        {QStringLiteral("title"), title},
        {QStringLiteral("dateKey"), date.toString(QStringLiteral("yyyy-MM-dd"))},
        {QStringLiteral("charactersRead"), characters},
        {QStringLiteral("readingTime"), seconds},
        {QStringLiteral("minReadingSpeed"), speed},
        {QStringLiteral("altMinReadingSpeed"), speed},
        {QStringLiteral("lastReadingSpeed"), speed},
        {QStringLiteral("maxReadingSpeed"), speed},
        {QStringLiteral("lastStatisticModified"), static_cast<double>(noon_ms(date))},
    };
}

bool write_book(const QString& books_directory, const QString& folder, const QString& title,
                const QJsonArray& rows) {
    const QString directory = books_directory + QLatin1Char('/') + folder;
    const QJsonObject metadata{
        {QStringLiteral("id"),
         QStringLiteral("00000000-0000-4000-8000-") +
             QString::number(qHash(folder), 16).rightJustified(12, QLatin1Char('0'))},
        {QStringLiteral("title"), title},
        {QStringLiteral("folder"), folder},
        {QStringLiteral("bookLanguage"), QStringLiteral("ja")},
        {QStringLiteral("characterCount"), 50000},
        {QStringLiteral("lastAccess"), 0.0},
        {QStringLiteral("cover"), QStringLiteral("Books/") + folder + QStringLiteral("/cover.png")},
    };
    return write_json(directory + QStringLiteral("/metadata.json"), QJsonDocument(metadata)) &&
        write_json(directory + QStringLiteral("/statistics.json"), QJsonDocument(rows)) &&
        write_bytes(directory + QStringLiteral("/cover.png"), cover_png());
}

// The fixture holds two recent days in two books: enough for the trend chart,
// the calendar and the ranking cards without depending on the calendar year.
bool ensure_fixture() {
    const auto books_directory =
        niratan::books::BookStorage(niratan::storage::data_root()).books_directory();
    const QString books = QString::fromStdString(books_directory.string());
    const QDate first = QDate::currentDate().addDays(-1);
    const QDate second = QDate::currentDate().addDays(-2);
    const QJsonArray first_rows{
        daily_row(QStringLiteral("統計の本"), first, 1200, 600.0, 120),
        daily_row(QStringLiteral("統計の本"), second, 800, 300.0, 160),
    };
    const QJsonArray second_rows{
        daily_row(QStringLiteral("Second Book"), first, 600, 300.0, 100),
    };
    return write_book(books, QStringLiteral("statistics-one"), QStringLiteral("統計の本"),
                      first_rows) &&
        write_book(books, QStringLiteral("statistics-two"), QStringLiteral("Second Book"),
                   second_rows);
}

bool write_statistics_enabled() {
    const QJsonObject settings{{QStringLiteral("enableStatistics"), true}};
    const QString path =
        QString::fromStdString((niratan::storage::data_root() / "settings.json").string());
    return write_json(path, QJsonDocument(settings));
}

}  // namespace

class StatisticsViewTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void loading_and_empty_state();
    void page_loads_with_a_fixed_header();
    void cards_follow_the_swift_order();
    void main_window_owns_the_toolbar();
    void controls_write_the_view_model();

   private:
    std::unique_ptr<QObject> create_page(QQuickWindow& window, const QSize& page_size,
                                         QString& error);

    std::unique_ptr<niratan::test::TempDataRoot> root_;
    QQmlApplicationEngine engine_;
};

void StatisticsViewTest::initTestCase() {
    root_ = std::make_unique<niratan::test::TempDataRoot>();
    QVERIFY(root_->isValid());
    g_previous_handler = qInstallMessageHandler(collect_qml_messages);
    // Construct the singleton while this execution's data root is active; its
    // storage paths are fixed for the rest of the process.
    QVERIFY(StatisticsViewModel::instance() != nullptr);
}

std::unique_ptr<QObject> StatisticsViewTest::create_page(QQuickWindow& window,
                                                         const QSize& page_size, QString& error) {
    window.resize(kWindowWidth, kWindowHeight);
    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) {
        error = QStringLiteral("the test window was not exposed");
        return {};
    }
    window.requestActivate();
    if (!QTest::qWaitForWindowActive(&window)) {
        error = QStringLiteral("the test window was not activated");
        return {};
    }

    const QUrl url(QStringLiteral("qrc:/qt/qml/Niratan/src/views/pages/StatisticsPage.qml"));
    QQmlComponent component(&engine_, url);
    if (component.isError()) {
        error = QStringLiteral("StatisticsPage: ") + error_text(component);
        return {};
    }

    std::unique_ptr<QObject> object(component.beginCreate(engine_.rootContext()));
    if (!object) {
        error = QStringLiteral("StatisticsPage: ") + error_text(component);
        return {};
    }

    auto* item = qobject_cast<QQuickItem*>(object.get());
    if (item == nullptr) {
        error = QStringLiteral("StatisticsPage is not an Item");
        return {};
    }
    item->setParentItem(window.contentItem());
    item->setWidth(page_size.width());
    item->setHeight(page_size.height());
    component.completeCreate();
    return object;
}

// B3: before the first snapshot only the loading card is shown, and the empty
// state waits for a load that found no days.
void StatisticsViewTest::loading_and_empty_state() {
    auto* model = StatisticsViewModel::instance();
    QVERIFY(!model->has_loaded());

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object =
        create_page(window, QSize(kDetailWidth, kWindowHeight), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);

    QVERIFY(!model->has_loaded());
    QVERIFY2(shown_item_named(page, QStringLiteral("loadingCard")) != nullptr,
             "the loading card is missing before the first snapshot");
    QVERIFY2(shown_item_named(page, QStringLiteral("emptyStateCard")) == nullptr,
             "the empty state is drawn before the first snapshot");

    QTRY_VERIFY(model->has_loaded());
    QTRY_VERIFY(shown_item_named(page, QStringLiteral("emptyStateCard")) != nullptr);
    QVERIFY2(shown_item_named(page, QStringLiteral("goalCard")) != nullptr,
             "the target settings are missing from the empty state");
    QVERIFY2(shown_item_named(page, QStringLiteral("trendCard")) == nullptr,
             "the dashboard is drawn with no reading records");

    QVERIFY(ensure_fixture());
    model->refresh();
    QTRY_VERIFY(model->has_statistics());
    QTRY_VERIFY(shown_item_named(page, QStringLiteral("trendCard")) != nullptr);
    QVERIFY2(shown_item_named(page, QStringLiteral("emptyStateCard")) == nullptr,
             "the empty state survives a load with records");

    model->refresh();
    QVERIFY2(shown_item_named(page, QStringLiteral("loadingCard")) == nullptr,
             "the initial loading card returned on a reload");
    QVERIFY2(shown_item_named(page, QStringLiteral("trendCard")) != nullptr,
             "the dashboard disappeared on a reload");
    QTRY_VERIFY(!model->loading());

    const QString warnings = warnings_after(QStringLiteral("StatisticsPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B1: the header is a direct child of the page and stays at the page top while
// the cards scroll.
void StatisticsViewTest::page_loads_with_a_fixed_header() {
    QVERIFY(ensure_fixture());
    auto* model = StatisticsViewModel::instance();
    model->refresh();

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object =
        create_page(window, QSize(kDetailWidth, kWindowHeight), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QTRY_VERIFY(model->has_statistics());

    QQuickItem* header = item_named(page, QStringLiteral("statisticsHeader"));
    QVERIFY2(header != nullptr, "the statistics header is missing");
    QCOMPARE(header->parentItem(), page);
    QTRY_VERIFY(shown(header));
    QCOMPARE(header->mapToItem(page, QPointF()).y(), 0.0);

    QQuickItem* scroll = item_named(page, QStringLiteral("statisticsScroll"));
    QVERIFY2(scroll != nullptr, "the statistics scroll view is missing");
    auto* flickable = qobject_cast<QQuickItem*>(scroll->property("contentItem").value<QObject*>());
    QVERIFY2(flickable != nullptr, "the statistics scroll view has no content item");
    qreal maximum = 0;
    QTRY_VERIFY((maximum = scroll->property("contentHeight").toReal() -
                     scroll->property("height").toReal()) > 0);
    flickable->setProperty("contentY", maximum);
    QTRY_VERIFY(flickable->property("contentY").toReal() > 0);
    QVERIFY(shown(header));
    QCOMPARE(header->mapToItem(page, QPointF()).y(), 0.0);

    QSignalSpy back(page, SIGNAL(backRequested()));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                      header->mapToScene(QPointF(32, 24)).toPoint());
    QTRY_COMPARE(back.count(), 1);

    const QString warnings = warnings_after(QStringLiteral("StatisticsPage"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
}

// B2: at the default window width the cards are Swift's single-column order,
// Range & Trend is full width first, and Selected Range has its own card.
void StatisticsViewTest::cards_follow_the_swift_order() {
    QVERIFY(ensure_fixture());
    auto* model = StatisticsViewModel::instance();
    model->refresh();

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object =
        create_page(window, QSize(kDetailWidth, kWindowHeight), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QTRY_VERIFY(model->has_statistics());

    const QStringList names{
        QStringLiteral("trendCard"),    QStringLiteral("todayCard"),
        QStringLiteral("goalCard"),     QStringLiteral("weekCard"),
        QStringLiteral("calendarCard"), QStringLiteral("selectedRangeCard"),
        QStringLiteral("speedCard"),    QStringLiteral("rankingCard"),
        QStringLiteral("shelfCard"),
    };

    QList<QQuickItem*> cards;
    for (const QString& name : names) {
        QQuickItem* card = nullptr;
        QTRY_VERIFY2((card = shown_item_named(page, name)) != nullptr,
                     qPrintable(QStringLiteral("missing card: ") + name));
        cards.append(card);
    }
    for (qsizetype index = 1; index < cards.size(); ++index) {
        const qreal previous = cards[index - 1]->mapToItem(page, QPointF()).y();
        const qreal current = cards[index]->mapToItem(page, QPointF()).y();
        QVERIFY2(
            current > previous,
            qPrintable(QStringLiteral("%1 is not below %2").arg(names[index], names[index - 1])));
    }

    const QStringList trend = visual_texts(cards.first());
    QVERIFY(trend.contains(QStringLiteral("Range & Trend")));
    QVERIFY2(shown_item_named(cards.first(), QStringLiteral("trendChart")) != nullptr,
             "the trend chart is missing");
    QCOMPARE(items_named(cards.first(), QStringLiteral("metricTile")).size(), 0);

    QQuickItem* selected = cards[5];
    QCOMPARE(items_named(selected, QStringLiteral("metricTile")).size(), 4);
    const QStringList selected_texts = visual_texts(selected);
    QVERIFY(selected_texts.contains(QStringLiteral("Selected Range")));
    for (const QString& label :
         {QStringLiteral("Duration"), QStringLiteral("Characters"), QStringLiteral("Speed")}) {
        QVERIFY2(selected_texts.contains(label), qPrintable(QStringLiteral("missing ") + label));
    }
    QVERIFY2(selected_texts.contains(QStringLiteral("Days Met")) ||
                 selected_texts.contains(QStringLiteral("Goal Progress")),
             "the range progress metric is missing");
}

// B1 shell half: the bookshelf's Statistics overlay owns the toolbar row at
// every stack depth and receives the collapsed-sidebar inset.
void StatisticsViewTest::main_window_owns_the_toolbar() {
    QVERIFY(ensure_fixture());
    QVERIFY(write_statistics_enabled());

    engine_.loadFromModule(QStringLiteral("Niratan"), QStringLiteral("MainWindow"));
    QVERIFY2(!engine_.rootObjects().isEmpty(), "MainWindow was not created");
    auto* window = qobject_cast<QQuickWindow*>(engine_.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickItem* statistics_button = nullptr;
    QTRY_VERIFY((statistics_button = item_with_property(window->contentItem(), "help",
                                                        QStringLiteral("Statistics"))) != nullptr);
    click(statistics_button);

    QQuickItem* header = nullptr;
    QTRY_VERIFY((header = item_named(window->contentItem(), QStringLiteral("statisticsHeader"))) !=
                nullptr);
    QQuickItem* page = header->parentItem();
    QVERIFY2(page != nullptr, "the header has no page parent");
    QTRY_COMPARE(page->mapToItem(window->contentItem(), QPointF()).y(), 0.0);
    QTRY_COMPARE(page->property("toolbarInset").toReal(), 0.0);

    window->setProperty("sidebarVisible", false);
    QTRY_COMPARE(page->property("toolbarInset").toReal(), 44.0);
    QTRY_COMPARE(page->mapToItem(window->contentItem(), QPointF()).y(), 0.0);

    const QString warnings = warnings_after(QStringLiteral("MainWindow"));
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings));
    engine_.clearComponentCache();
}

// B4: representative controls still write the view model and its settings.
void StatisticsViewTest::controls_write_the_view_model() {
    QVERIFY(ensure_fixture());
    auto* model = StatisticsViewModel::instance();
    model->setTargetType(0);
    model->refresh();

    QQuickWindow window;
    QString error;
    std::unique_ptr<QObject> object =
        create_page(window, QSize(kDetailWidth, kWindowHeight), error);
    QVERIFY2(object != nullptr, qPrintable(error));
    auto* page = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(page != nullptr);
    QTRY_VERIFY(model->has_statistics());

    QQuickItem* range = item_named(page, QStringLiteral("rangeSegments"));
    QVERIFY2(range != nullptr, "the Range segments are missing");
    // The range picker is the kit SegmentedPicker; a segment carries its
    // title as `text` and a `clicked` signal.
    QQuickItem* day = shown_control(range, "QQuickItem", QStringLiteral("Day"));
    QVERIFY2(day != nullptr, "the Day range segment is missing");
    click(day);
    QTRY_COMPARE(model->range_mode(), 3);

    QQuickItem* header = item_named(page, QStringLiteral("statisticsHeader"));
    QVERIFY(header != nullptr);
    const QDate range_start = model->range_start();
    QTRY_VERIFY2(has_day_title(visual_texts(header), range_start),
                 "the header subtitle did not switch to the day form");

    QQuickItem* stepper = item_named(page, QStringLiteral("characterTargetStepper"));
    QVERIFY2(stepper != nullptr, "the Character Target stepper is missing");
    const int before = model->character_target();
    QQuickItem* control = stepper_control(stepper);
    QVERIFY2(control != nullptr && shown(control),
             "the Character Target stepper control is missing");
    auto* increase = control->property("upButton").value<QQuickItem*>();
    QVERIFY2(increase != nullptr && shown(increase) && increase->isEnabled(),
             "the Character Target increase half is missing or disabled");

    // Scroll the stepper into the dashboard viewport, then press its
    // increase half with the mouse.
    QQuickItem* scroll = item_named(page, QStringLiteral("statisticsScroll"));
    QVERIFY(scroll != nullptr);
    auto* flickable = qobject_cast<QQuickItem*>(scroll->property("contentItem").value<QObject*>());
    QVERIFY(flickable != nullptr);
    const QPointF in_content =
        increase->mapToItem(flickable->property("contentItem").value<QQuickItem*>(),
                            QPointF(increase->width() / 2, increase->height() / 2));
    const qreal maximum =
        qMax(0.0, flickable->property("contentHeight").toReal() - flickable->height());
    flickable->setProperty("contentY",
                           qBound(0.0, in_content.y() - flickable->height() / 2, maximum));
    const QPoint centre =
        increase->mapToScene(QPointF(increase->width() / 2, increase->height() / 2)).toPoint();
    QVERIFY2(QRect(QPoint(0, 0), window.size()).contains(centre),
             "the increase half is outside the window after scrolling");
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centre);
    QTRY_COMPARE(model->character_target(), before + 500);

    object.reset();
    std::unique_ptr<QObject> recreated =
        create_page(window, QSize(kDetailWidth, kWindowHeight), error);
    QVERIFY2(recreated != nullptr, qPrintable(error));
    QTRY_COMPARE(model->character_target(), before + 500);
    const auto targets = niratan::settings::GlobalSettings::at_data_root().statistics_targets();
    QCOMPARE(targets.daily_character_target, before + 500);
}

int main(int argc, char* argv[]) {
    niratan::ui::register_popup_url_schemes();
    niratan::ui::register_reader_url_schemes();
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    StatisticsViewTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_statistics_view.moc"

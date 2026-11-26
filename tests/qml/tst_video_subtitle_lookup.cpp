// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Video/Subtitles/InteractiveSubtitleTextView.swift
// (ClickableSubtitleTextView.mouseDown): a click looks up from the character
// under the pointer, found with layoutManager.glyphIndex(for:in:), the glyph
// whose box holds the point, or the nearest glyph beside the text. Clicking
// the right half of a character must not start the lookup at the next one,
// a click right of a short line stays on that line, and a supplementary
// character is never split.

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

#include <clocale>
#include <memory>

Q_IMPORT_QML_PLUGIN(NiratanPlugin)
Q_IMPORT_QML_PLUGIN(NiratanStylePlugin)

class VideoSubtitleLookupTest : public QObject {
    Q_OBJECT

   private slots:
    void a_click_looks_up_the_character_under_the_pointer();
    void a_click_right_of_a_short_line_stays_on_it();
    void a_supplementary_character_is_not_split();
};

namespace {

constexpr char kScene[] = R"(
import QtQuick
import Niratan

Item {
    property string cueText
    width: 800
    height: 300

    MpvItem { id: mpv }
    PlayerLookupViewModel { id: lookup }

    VideoSubtitleText {
        objectName: "row"
        y: 60
        width: 800
        maxWidth: 800
        cue: ({ id: "cue", text: parent.cueText })
        engine: mpv
        lookupModel: lookup
    }
}
)";

QString error_text(const QQmlComponent& component) {
    QStringList lines;
    for (const QQmlError& error : component.errors()) {
        lines.append(error.toString());
    }
    return lines.join('\n');
}

// One subtitle row showing text in a shown window, with its clicks recorded.
class Row {
   public:
    explicit Row(const QString& text) {
        window_.resize(800, 300);
        window_.show();
        if (!QTest::qWaitForWindowExposed(&window_)) {
            return;
        }
        QQmlComponent component(&engine_);
        component.setData(kScene, QUrl(QStringLiteral("qrc:/test/Scene.qml")));
        scene_.reset(component.beginCreate(engine_.rootContext()));
        auto* root = qobject_cast<QQuickItem*>(scene_.get());
        if (root == nullptr) {
            qWarning().noquote() << error_text(component);
            return;
        }
        root->setProperty("cueText", text);
        root->setParentItem(window_.contentItem());
        component.completeCreate();
        row_ = root->findChild<QQuickItem*>(QStringLiteral("row"));
        if (row_ != nullptr) {
            spy_ = std::make_unique<QSignalSpy>(row_, SIGNAL(clicked(int, QRectF)));
        }
    }

    bool ready() const { return row_ != nullptr && spy_ != nullptr && row_->height() > 0; }

    // The caret rectangle at offset, in the row's coordinates.
    QRectF rect(int offset) const {
        QVariant rect;
        QMetaObject::invokeMethod(row_, "characterRect", Q_RETURN_ARG(QVariant, rect),
                                  Q_ARG(QVariant, offset));
        return rect.toRectF();
    }

    // The offset a click at (x, y) of the row looks up, or -1 without one.
    int click(qreal x, qreal y) {
        const qsizetype before = spy_->count();
        QTest::mouseClick(&window_, Qt::LeftButton, {}, row_->mapToScene(QPointF(x, y)).toPoint());
        if (!QTest::qWaitFor([&] { return spy_->count() > before; })) {
            return -1;
        }
        return spy_->last().at(0).toInt();
    }

   private:
    QQuickWindow window_;
    QQmlEngine engine_;
    std::unique_ptr<QObject> scene_;
    QQuickItem* row_ = nullptr;
    std::unique_ptr<QSignalSpy> spy_;
};

}  // namespace

void VideoSubtitleLookupTest::a_click_looks_up_the_character_under_the_pointer() {
    Row row(QStringLiteral("じゃあね"));
    QTRY_VERIFY(row.ready());
    const qreal first = row.rect(0).x();
    const qreal second = row.rect(1).x();
    const qreal third = row.rect(2).x();
    QVERIFY2(second > first && third > second, "the characters have no width");
    const qreal y = row.rect(0).center().y();

    QCOMPARE(row.click(first + (second - first) * 0.8, y), 0);
    QCOMPARE(row.click(second + (third - second) * 0.2, y), 1);
    QCOMPARE(row.click(second + (third - second) * 0.8, y), 1);
}

void VideoSubtitleLookupTest::a_click_right_of_a_short_line_stays_on_it() {
    // The short first line ends in a line break, the last line ends the text.
    Row row(QStringLiteral("うん\nじゃあね、また"));
    QTRY_VERIFY(row.ready());
    const QRectF line_end = row.rect(2);
    QVERIFY2(row.rect(3).y() > line_end.y(), "the cue did not break into two lines");
    QCOMPARE(row.click(line_end.x() + 12, line_end.center().y()), 1);

    const QRectF text_end = row.rect(10);
    QCOMPARE(row.click(text_end.x() + 4, text_end.center().y()), 9);
}

void VideoSubtitleLookupTest::a_supplementary_character_is_not_split() {
    // A supplementary character takes two UTF-16 units: offsets 0 and 1.
    Row row(QStringLiteral("\U0001F600星"));
    QTRY_VERIFY(row.ready());
    const qreal start = row.rect(0).x();
    const qreal next = row.rect(2).x();
    QVERIFY2(next > start, "the supplementary character has no width");
    QCOMPARE(row.click(start + (next - start) * 0.8, row.rect(0).center().y()), 0);
}

int main(int argc, char* argv[]) {
    // Keep profile, cache and settings paths out of the developer's home.
    QTemporaryDir data_root;
    if (data_root.isValid()) {
        const QByteArray base = data_root.path().toUtf8();
        qputenv("HOME", base);
        qputenv("XDG_DATA_HOME", base + "/data");
        qputenv("XDG_CACHE_HOME", base + "/cache");
        qputenv("XDG_CONFIG_HOME", base + "/config");
    }

    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));

    VideoSubtitleLookupTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_video_subtitle_lookup.moc"

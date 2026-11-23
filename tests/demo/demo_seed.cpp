// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fills a data root with a demo library for offscreen renders of the views.
//
//   XDG_DATA_HOME=<dir> XDG_CACHE_HOME=<dir> niratan_demo_seed
//       [--videos <folder>] [<epub>...]
//
// The data root and the cache root must be empty so a real library is never
// touched. Each EPUB is imported through BookStorage::import_book, the path
// the bookshelf uses. Placeholder books with drawn covers and spread reading
// progress are added so the grid, the reading row and the finished badge all
// have content; they have no EPUB and cannot be opened. A video folder is
// added as a library source, scanned, and a few of its items get playback
// progress so Continue Watching is populated; every video item also gets a
// drawn poster at its thumbnail cache path, because the library page reads
// the cache and never runs ffmpeg for the demo. Media files are only read,
// never written, so an empty placeholder video is enough for a render.

#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QSet>
#include <QTextStream>
#include <QUuid>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "services/books/book_storage.h"
#include "services/player/playback_history.h"
#include "services/storage/app_paths.h"
#include "services/video/library_catalog.h"
#include "services/video/thumbnail_store.h"

namespace {

namespace fs = std::filesystem;

struct Placeholder {
    const char* title;
    double progress;
    int hue;
};

// Titles are invented; progress covers unread, reading and finished books.
constexpr std::array kPlaceholders{
    Placeholder{"夜明けの図書館", 0.42, 210},   Placeholder{"星降る町の探偵", 0.08, 260},
    Placeholder{"雨音とカフェラテ", 1.0, 20},   Placeholder{"放課後の魔法使い", 0.67, 330},
    Placeholder{"風の旅人", 0.0, 140},          Placeholder{"硝子の迷宮", 0.0, 190},
    Placeholder{"君と見た夏の海", 0.23, 175},   Placeholder{"異世界の料理人", 0.0, 35},
    Placeholder{"月下の剣士", 1.0, 0},          Placeholder{"小さな恋のうた", 0.0, 300},
    Placeholder{"機械仕掛けの少女", 0.91, 230},
};

constexpr int kCoverWidth = 300;
constexpr int kCoverHeight = 426;
constexpr int kCharacterCount = 120000;

QString new_id() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
}

void draw_cover(const QString& title, int hue, const fs::path& file) {
    QImage image(kCoverWidth, kCoverHeight, QImage::Format_RGB32);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    QLinearGradient gradient(0, 0, kCoverWidth, kCoverHeight);
    gradient.setColorAt(0, QColor::fromHsl(hue, 120, 190));
    gradient.setColorAt(1, QColor::fromHsl((hue + 40) % 360, 140, 90));
    painter.fillRect(image.rect(), gradient);

    QFont font(QStringLiteral("Noto Serif CJK JP"));
    font.setPixelSize(40);
    font.setWeight(QFont::Bold);
    painter.setFont(font);
    painter.setPen(Qt::white);
    // Vertical title, one character per line, as Japanese covers set it.
    int y = 40;
    for (const QChar character : title) {
        painter.drawText(QRect(kCoverWidth - 80, y, 56, 48), Qt::AlignCenter, character);
        y += 46;
    }
    painter.end();
    image.save(QString::fromStdString(file.string()), "PNG");
}

// Audio-only items have no frame to capture, so their card keeps the film
// placeholder (Decision D2).
bool is_audio_only(const QString& path) {
    static const QSet<QString> kAudioOnly{
        QStringLiteral("m4b"),  QStringLiteral("m4a"),  QStringLiteral("mp3"),
        QStringLiteral("flac"), QStringLiteral("opus"), QStringLiteral("ogg"),
        QStringLiteral("oga"),  QStringLiteral("weba"), QStringLiteral("wav"),
        QStringLiteral("aac"),  QStringLiteral("aiff"), QStringLiteral("aif"),
        QStringLiteral("ape"),  QStringLiteral("wv")};
    return kAudioOnly.contains(QFileInfo(path).suffix().toLower());
}

void draw_thumbnail(qsizetype index, const fs::path& file) {
    constexpr int kWidth = 384;
    constexpr int kHeight = 216;
    QImage image(kWidth, kHeight, QImage::Format_RGB32);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const int hue = static_cast<int>((200 + index * 57) % 360);
    QLinearGradient gradient(0, 0, kWidth, kHeight);
    gradient.setColorAt(0, QColor::fromHsl(hue, 110, 165));
    gradient.setColorAt(1, QColor::fromHsl((hue + 55) % 360, 140, 60));
    painter.fillRect(image.rect(), gradient);
    // A soft glow reads as a frame of a scene rather than a flat card.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 26));
    painter.drawEllipse(QPointF(kWidth * 0.72, kHeight * 0.3), 70, 70);
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.drawRect(QRect(0, kHeight - 48, kWidth, 48));
    painter.end();
    image.save(QString::fromStdString(file.string()), "PNG");
}

bool add_placeholder(const niratan::books::BookStorage& storage, const Placeholder& placeholder,
                     double last_access) {
    const std::string folder = niratan::books::sanitize_file_name(placeholder.title);
    const fs::path directory = storage.books_directory() / folder;
    std::error_code error;
    fs::create_directories(directory, error);
    if (error) {
        return false;
    }
    draw_cover(QString::fromUtf8(placeholder.title), placeholder.hue, directory / "cover.png");

    niratan::books::BookMetadata book;
    book.id = new_id().toStdString();
    book.title = placeholder.title;
    book.cover = "Books/" + folder + "/cover.png";
    book.folder = folder;
    book.last_access = last_access;
    book.book_language = "ja";
    book.character_count = kCharacterCount;
    if (!storage.save_metadata(book)) {
        return false;
    }
    niratan::books::Bookmark bookmark;
    bookmark.progress = placeholder.progress;
    bookmark.character_count = static_cast<int>(placeholder.progress * kCharacterCount);
    return storage.save_bookmark(folder, bookmark);
}

void seed_videos(const fs::path& folder, QTextStream& out) {
    niratan::video::LibraryStore store;
    const auto source = store.add_source(folder);
    QString error;
    if (!store.scan_source(source.id, &error)) {
        out << "video scan failed: " << error << '\n';
        return;
    }
    const auto& items = store.catalog().items;
    auto history = niratan::player::PlaybackHistory::at_data_root();
    constexpr std::array kFractions{0.15, 0.55, 0.8, 0.35, 0.9};
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (qsizetype index = 0; index < items.size() && index < qsizetype(kFractions.size());
         ++index) {
        constexpr double kDuration = 1440.0;
        history.save(items[index].path, kFractions[index] * kDuration, kDuration,
                     now.addSecs(-3600 * index));
    }

    const fs::path cache = niratan::video::default_thumbnail_cache_directory();
    if (!cache.empty()) {
        std::error_code cache_error;
        fs::create_directories(cache, cache_error);
        if (cache_error) {
            out << "thumbnail cache directory failed\n";
            return;
        }
        for (qsizetype index = 0; index < items.size(); ++index) {
            if (is_audio_only(items[index].path)) {
                continue;
            }
            const auto request = niratan::video::ThumbnailRequest::for_item(items[index]);
            const auto file =
                cache / (niratan::video::thumbnail_cache_key(request).toStdString() + ".png");
            draw_thumbnail(index, file);
        }
    }
    out << "videos: " << items.size() << '\n';
}

}  // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const fs::path root = niratan::storage::data_root();
    std::error_code error;
    if (root.empty() || qEnvironmentVariableIsEmpty("XDG_DATA_HOME")) {
        err << "set XDG_DATA_HOME to an empty demo directory\n";
        return 1;
    }
    if (fs::exists(root, error) && !fs::is_empty(root, error)) {
        err << "refusing to seed a non-empty data root: " << QString::fromStdString(root.string())
            << '\n';
        return 1;
    }
    const fs::path cache = niratan::storage::cache_root();
    if (cache.empty() || qEnvironmentVariableIsEmpty("XDG_CACHE_HOME")) {
        err << "set XDG_CACHE_HOME to an empty demo directory\n";
        return 1;
    }
    if (fs::exists(cache, error) && !fs::is_empty(cache, error)) {
        err << "refusing to seed a non-empty cache root: " << QString::fromStdString(cache.string())
            << '\n';
        return 1;
    }

    fs::path videos;
    std::vector<std::string> epubs;
    const QStringList arguments = app.arguments().mid(1);
    for (qsizetype index = 0; index < arguments.size(); ++index) {
        if (arguments[index] == QStringLiteral("--videos") && index + 1 < arguments.size()) {
            videos = arguments[++index].toStdString();
        } else {
            epubs.push_back(arguments[index].toStdString());
        }
    }

    const niratan::books::BookStorage storage(root);
    const double now = niratan::books::reference_now();
    for (const std::string& epub : epubs) {
        const auto result = storage.import_book(epub);
        if (!result.book.has_value()) {
            err << "import failed: " << QString::fromStdString(epub) << ": "
                << QString::fromStdString(result.error) << '\n';
            return 1;
        }
        auto book = *result.book;
        book.last_access = now;
        storage.save_metadata(book);
        niratan::books::Bookmark bookmark;
        bookmark.progress = 0.31;
        bookmark.character_count =
            static_cast<int>(bookmark.progress * book.character_count.value_or(0));
        storage.save_bookmark(book.folder, bookmark);
    }
    int offset = 1;
    for (const Placeholder& placeholder : kPlaceholders) {
        if (!add_placeholder(storage, placeholder, now - 3600.0 * offset++)) {
            err << "could not write placeholder " << placeholder.title << '\n';
            return 1;
        }
    }
    out << "books: " << epubs.size() + kPlaceholders.size() << '\n';

    if (!videos.empty()) {
        seed_videos(videos, out);
    }
    return 0;
}

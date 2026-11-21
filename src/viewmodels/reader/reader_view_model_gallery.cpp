// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_view_model_state.h"

#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <set>

#include "services/reader/character_normalizer.h"

namespace niratan::viewmodels {

QVariantList ReaderViewModel::gallery_images() const { return state_->gallery_images; }

bool ReaderViewModel::gallery_loading() const { return state_->gallery_loading; }

void ReaderViewModel::clear_gallery() {
    ++state_->gallery_revision;
    state_->gallery_images.clear();
    state_->gallery_paths.clear();
    state_->gallery_loading = false;
    state_->gallery_server.set_book_images({}, {});
    emit galleryChanged();
}

void ReaderViewModel::loadGallery() {
    if (!state_->session) {
        return;
    }
    const std::size_t revision = ++state_->gallery_revision;
    const QString route = QStringLiteral("/book-images/%1/").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    state_->gallery_server.set_book_images(route.toStdString(), [this](std::string_view path)
        -> std::optional<audio::LocalFileServer::BookImage> {
        if (!state_->session || !state_->gallery_paths.contains(path)) {
            return std::nullopt;
        }
        const auto image = state_->session->resource(path);
        if (!image) {
            return std::nullopt;
        }
        return std::pair{image->mime_type, QByteArray::fromStdString(image->bytes)};
    });
    state_->gallery_images.clear();
    state_->gallery_paths.clear();
    state_->gallery_loading = true;
    emit galleryChanged();
    QTimer::singleShot(0, this, [this, revision, route] { scan_gallery_chapter(0, revision, route); });
}

void ReaderViewModel::scan_gallery_chapter(int index, std::size_t revision, const QString& route) {
    if (revision != state_->gallery_revision) {
        return;
    }
    if (!state_->session || index >= state_->session->spine_count() || !state_->gallery_server.port()) {
        state_->gallery_loading = false;
        emit galleryChanged();
        return;
    }

    // Match quoted attributes in spine order, as ReaderImageGalleryIndex.imageEntries does.
    static const QRegularExpression tag_expression(
        QStringLiteral(R"re(<!--.*?-->|<script\b[^>]*>.*?</script\s*>|<img(?=\s|/?>)(?:[^'">]|"[^"]*"|'[^']*')*>)re"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression attribute_expression(
        QStringLiteral(R"re(\s([A-Za-z_:][A-Za-z0-9_.:-]*)\s*=\s*(?:"([^"]*)"|'([^']*)'))re"));
    const auto& session = *state_->session;
    QStringList directory = QString::fromStdString(session.spine_path(index)).split(QLatin1Char('/'));
    directory.removeLast();
    const QString document = QString::fromStdString(session.chapter_document(index, {}, {}));
    auto tags = tag_expression.globalMatch(document);
    std::set<std::string> seen;
    while (tags.hasNext()) {
        const QString tag = tags.next().captured();
        if (!tag.startsWith(QStringLiteral("<img"), Qt::CaseInsensitive)) {
            continue;
        }
        QString source;
        QString classes;
        auto attributes = attribute_expression.globalMatch(tag);
        while (attributes.hasNext()) {
            const auto attribute = attributes.next();
            const QString name = attribute.captured(1).toLower();
            const QString value = attribute.captured(attribute.hasCaptured(2) ? 2 : 3);
            if (name == QStringLiteral("src") && source.isNull()) {
                source = value;
            } else if (name == QStringLiteral("class") && classes.isNull()) {
                classes = value;
            }
        }
        if (classes.simplified().split(QLatin1Char(' ')).contains(QStringLiteral("gaiji"), Qt::CaseInsensitive)) {
            continue;
        }
        const QUrl reference(QString::fromStdString(reader::decoded_character_references(source.toStdString())).trimmed());
        const QString path = reference.path(QUrl::FullyDecoded);
        if (!reference.isValid() || !reference.isRelative() || !reference.authority().isEmpty() ||
            path.isEmpty() || path.startsWith(QLatin1Char('/'))) {
            continue;
        }
        QStringList components = directory;
        bool escapes_container = false;
        for (const QString& component : path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
            if (component == QStringLiteral("..")) {
                if (components.isEmpty()) {
                    escapes_container = true;
                    break;
                }
                components.removeLast();
            } else if (component != QStringLiteral(".")) {
                components.append(component);
            }
        }
        if (escapes_container) {
            continue;
        }
        const QString image_path = components.join(QLatin1Char('/'));
        const auto extension = image_path.section(QLatin1Char('.'), -1).toLower();
        const std::string resource_path = image_path.toStdString();
        if ((extension != QStringLiteral("jpg") && extension != QStringLiteral("jpeg") &&
             extension != QStringLiteral("png")) || !seen.insert(resource_path).second ||
            !session.resource(resource_path)) {
            continue;
        }
        QUrl url(QStringLiteral("http://127.0.0.1"));
        url.setPort(state_->gallery_server.port());
        url.setPath(route + image_path, QUrl::DecodedMode);
        state_->gallery_paths.insert(resource_path);
        state_->gallery_images.append(QVariantMap{
            {QStringLiteral("chapter"), QString::fromStdString(session.chapter_title(index))},
            {QStringLiteral("chapterIndex"), index},
            {QStringLiteral("path"), image_path},
            {QStringLiteral("url"), url},
        });
    }
    // Yield between chapters to keep the drawer responsive during the scan.
    QTimer::singleShot(0, this, [this, index, revision, route] {
        scan_gallery_chapter(index + 1, revision, route);
    });
}

}  // namespace niratan::viewmodels

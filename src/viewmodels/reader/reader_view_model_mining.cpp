// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_view_model_state.h"

#include <QJsonObject>

#include <cmath>
#include <limits>
#include <string_view>
#include <utility>

#include "services/reader/document_context.h"

namespace niratan::viewmodels {

void ReaderViewModel::State::BookMediaServer::set_book_images(std::string route,
                                                              ImageLookup lookup) {
    if (!lookup) {
        fixed_.set_book_images({}, {});
        fixed_.clear_media();
        gallery_fallback_.reset();
        return;
    }

    fixed_.set_book_images(route, lookup);
    if (fixed_.is_listening()) {
        gallery_fallback_.reset();
        return;
    }

    fixed_.set_book_images({}, {});
    if (!gallery_fallback_) {
        gallery_fallback_.emplace(std::string{}, 0);
    }
    gallery_fallback_->set_book_images(std::move(route), std::move(lookup));
}

quint16 ReaderViewModel::State::BookMediaServer::port() const {
    return gallery_fallback_ ? gallery_fallback_->port() : fixed_.port();
}

bool ReaderViewModel::State::BookMediaServer::serve_cover(std::optional<QByteArray> bytes) {
    // Keep published gallery URLs on their listener until the next gallery load.
    if (gallery_fallback_ && gallery_fallback_->is_listening()) {
        return false;
    }
    // An unreadable cover still has a URL if the fixed port is available.
    // Probe the bind without leaving placeholder bytes on the cover route.
    fixed_.set_cover(bytes ? std::move(*bytes) : QByteArray{});
    const bool available = fixed_.is_listening();
    if (!bytes || !available) {
        fixed_.clear_media();
    }
    return available;
}

QString ReaderViewModel::document_title() const {
    const std::string_view epub_title = state_->session
        ? std::string_view(state_->session->book().metadata().title)
        : std::string_view{};
    return QString::fromStdString(reader::document_title(epub_title, state_->book));
}

QString ReaderViewModel::cover_path() const {
    const auto path = reader::document_cover_path(state_->storage, state_->book);
    return path ? QString::fromStdString(path->string()) : QString{};
}

QString ReaderViewModel::cover_url() const {
    const auto path = reader::document_cover_path(state_->storage, state_->book);
    if (!path || path->empty() ||
        !state_->gallery_server.serve_cover(reader::read_document_cover(*path))) {
        return {};
    }
    return QString::fromStdString(reader::document_cover_url(*path));
}

int ReaderViewModel::lookup_offset() const {
    return state_->lookup_offset;
}

void ReaderViewModel::handle_text_selected(const QJsonObject& data) {
    const auto text = data.value(QStringLiteral("text"));
    const auto sentence = data.value(QStringLiteral("sentence"));
    const auto rect = data.value(QStringLiteral("rect")).toObject();
    const auto x = rect.value(QStringLiteral("x"));
    const auto y = rect.value(QStringLiteral("y"));
    const auto width = rect.value(QStringLiteral("width"));
    const auto height = rect.value(QStringLiteral("height"));
    if (!text.isString() || !sentence.isString() || !x.isDouble() || !y.isDouble() ||
        !width.isDouble() || !height.isDouble()) {
        return;
    }
    const QString lookup_text = text.toString();
    if (lookup_text.trimmed().isEmpty() || width.toDouble() <= 0 || height.toDouble() <= 0) {
        return;
    }

    // SelectionData.normalizedOffset is an Int; another JSON type has none.
    int lookup_offset = -1;
    const QJsonValue normalized = data.value(QStringLiteral("normalizedOffset"));
    if (normalized.isDouble()) {
        const double value = normalized.toDouble();
        if (std::isfinite(value) && std::floor(value) == value &&
            value >= static_cast<double>(std::numeric_limits<int>::min()) &&
            value <= static_cast<double>(std::numeric_limits<int>::max())) {
            lookup_offset = static_cast<int>(value);
        }
    }
    if (state_->lookup_offset != lookup_offset) {
        state_->lookup_offset = lookup_offset;
        emit lookupOffsetChanged();
    }

    emit lookupRequested(lookup_text, sentence.toString(),
                         QRectF(x.toDouble(), y.toDouble(), width.toDouble(), height.toDouble()));
}

}  // namespace niratan::viewmodels

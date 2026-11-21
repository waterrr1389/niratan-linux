// SPDX-License-Identifier: GPL-3.0-or-later
//
// Matching, cue list and match JSON of SasayakiViewModel, following
// Features/Sasayaki/SasayakiMatcher.swift (match(rootURL:cues:searchWindow:)),
// SasayakiPlayer.updateMatchData, cues(for:) and the match reads in
// Core/BookStorage.swift. The worker opens the reader's EPUB, selects chapters
// with services/sasayaki/match_chapters.h and aligns the loaded cues with
// services/sasayaki/matcher.h; only a nonempty, uncancelled result replaces
// sasayaki_match.json.

#include "sasayaki_view_model.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

#include <algorithm>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "models/sasayaki/sasayaki.h"
#include "sasayaki_view_model_state.h"
#include "services/books/book_storage.h"
#include "services/reader/book_session.h"
#include "services/sasayaki/match_chapters.h"
#include "services/sasayaki/matcher.h"
#include "services/storage/app_paths.h"
#include "viewmodels/reader/reader_view_model.h"

namespace niratan::viewmodels {
namespace {

using niratan::sasayaki::SasayakiCue;
using niratan::sasayaki::SasayakiMatchData;

// Default search window from SasayakiMatchView's 50...1000 range.
constexpr int kDefaultSearchWindow = 200;
constexpr int kMinimumSearchWindow = 50;
constexpr int kMaximumSearchWindow = 1000;
constexpr int kSearchWindowIncrement = 50;

QVariantMap cue_entry(const SasayakiCue& cue) {
    QVariantMap entry;
    entry.insert(QStringLiteral("id"), QString::fromStdString(cue.id));
    entry.insert(QStringLiteral("start"), cue.start_time);
    entry.insert(QStringLiteral("end"), cue.end_time);
    entry.insert(QStringLiteral("text"), QString::fromStdString(cue.text));
    return entry;
}

niratan::books::BookStorage book_storage() {
    return niratan::books::BookStorage(niratan::storage::data_root());
}

}  // namespace

bool SasayakiViewModel::matching() const {
    return state_->match_worker.joinable();
}

double SasayakiViewModel::match_progress() const {
    return state_->match_progress;
}

int SasayakiViewModel::matched_count() const {
    return state_->match ? static_cast<int>(state_->match->matches.size()) : 0;
}

int SasayakiViewModel::unmatched_count() const {
    return state_->match ? state_->match->unmatched : 0;
}

int SasayakiViewModel::inferred_count() const {
    if (!state_->match) {
        return 0;
    }
    return static_cast<int>(
        std::ranges::count_if(state_->match->matches, &sasayaki::SasayakiMatch::context_inferred));
}

QString SasayakiViewModel::match_rate_label() const {
    const int matched = matched_count();
    const auto total = static_cast<qint64>(matched) + unmatched_count();
    const double percentage = total > 0 ? 100.0 * matched / static_cast<double>(total) : 0.0;
    return tr("%1/%2 (%3%)").arg(matched).arg(total).arg(percentage, 0, 'f', 1);
}

QString SasayakiViewModel::match_error() const {
    return state_->match_error;
}

int SasayakiViewModel::match_search_window() const {
    return state_->match_search_window;
}

void SasayakiViewModel::set_match_search_window(int value) {
    const int clamped = std::clamp(value, kMinimumSearchWindow, kMaximumSearchWindow);
    const int rounded =
        ((clamped + kSearchWindowIncrement / 2) / kSearchWindowIncrement) * kSearchWindowIncrement;
    if (rounded != state_->match_search_window) {
        state_->match_search_window = rounded;
        emit matchSearchWindowChanged();
    }
}

void SasayakiViewModel::reset_match_search_window() {
    set_match_search_window(kDefaultSearchWindow);
}

void SasayakiViewModel::matchSubtitles(int searchWindow) {
    if (matching()) {
        // A programmatic retry supersedes the current run without overlapping it.
        cancelSubtitleMatch();
        state_->pending_match_window = searchWindow;
        return;
    }
    if (state_->subtitle_cues.empty()) {
        set_match_error(tr("Load a subtitle file first."));
        return;
    }
    if (!state_->has_root) {
        set_match_error(tr("The EPUB for the open book could not be found."));
        return;
    }

    set_match_search_window(searchWindow);
    const int window = state_->match_search_window;
    const auto folder = ReaderViewModel::instance()->book_folder().toStdString();
    const auto data_root = niratan::storage::data_root();
    set_match_error({});
    set_match_progress(0.0);

    try {
        state_->match_worker = std::jthread([this, folder, data_root, window,
                                             cues = state_->subtitle_cues](std::stop_token stop) {
            std::optional<SasayakiMatchData> result;
            QString error;
            try {
                const niratan::books::BookStorage storage(data_root);
                const auto metadata = storage.load_metadata(folder);
                const auto epub = metadata ? storage.epub_path(*metadata) : std::nullopt;
                const auto book = epub && !stop.stop_requested()
                    ? niratan::reader::BookSession::open(*epub)
                    : std::nullopt;
                if (!book) {
                    error = tr("The EPUB for the open book could not be opened.");
                } else {
                    std::vector<sasayaki::MatchChapter> chapters = sasayaki::match_chapters(
                        book->book(), [&stop]() { return stop.stop_requested(); });
                    if (!stop.stop_requested()) {
                        QMetaObject::invokeMethod(
                            this,
                            [this, stop]() {
                                if (!stop.stop_requested()) {
                                    set_match_progress(0.5);
                                }
                            },
                            Qt::QueuedConnection);
                        result = sasayaki::match_cues(cues, chapters, window,
                                                      [&stop]() { return stop.stop_requested(); });
                    }
                }
            } catch (const std::exception&) {
                error = tr("Could not match subtitles.");
            }

            QMetaObject::invokeMethod(
                this,
                [this, stop, result = std::move(result), error = std::move(error)]() {
                    // Keep one worker at a time even while it is unwinding
                    // after cancellation. Never save a stale result.
                    if (!stop.stop_requested()) {
                        if (!result || !error.isEmpty()) {
                            set_match_error(error.isEmpty() ? tr("Could not match subtitles.")
                                                            : error);
                        } else {
                            apply_match_result(*result);
                        }
                    }
                    if (state_->match_progress != 1.0) {
                        set_match_progress(0.0);
                    }
                    state_->match_worker.join();
                    emit matchingChanged();
                    if (const auto pending =
                            std::exchange(state_->pending_match_window, std::nullopt)) {
                        matchSubtitles(*pending);
                    }
                },
                Qt::QueuedConnection);
        });
    } catch (const std::system_error&) {
        set_match_error(tr("Could not start subtitle matching."));
        return;
    }
    emit matchingChanged();
}

void SasayakiViewModel::cancelSubtitleMatch() {
    state_->pending_match_window.reset();
    state_->match_worker.request_stop();
    set_match_progress(0.0);
}

void SasayakiViewModel::apply_match_result(const SasayakiMatchData& result) {
    if (result.matches.empty()) {
        set_match_error(tr("No subtitles matched the book. Existing matches were kept."));
        return;
    }
    std::string reason;
    if (!book_storage().save_sasayaki_match(state_->folder, result, &reason)) {
        set_match_error(
            tr("Could not save subtitle matches: %1").arg(QString::fromStdString(reason)));
    } else if (!load_match()) {
        set_match_error(tr("Could not reload saved subtitle matches."));
    } else {
        rebuild_timeline(true);
        emit hasMatchChanged();
        emit matchDataChanged();
        set_match_progress(1.0);
    }
}

QString SasayakiViewModel::applyCuesJson() const {
    QJsonArray ranges;
    if (state_->match.has_value()) {
        const int chapter = ReaderViewModel::instance()->chapter_index();
        for (const niratan::sasayaki::SasayakiMatch& match : state_->match->matches) {
            if (match.chapter_index != chapter) {
                continue;
            }
            QJsonObject entry;
            entry.insert(QStringLiteral("id"), QString::fromStdString(match.id));
            entry.insert(QStringLiteral("start"), match.start);
            entry.insert(QStringLiteral("length"), match.length);
            ranges.append(entry);
        }
    }
    return QString::fromUtf8(QJsonDocument(ranges).toJson(QJsonDocument::Compact));
}

bool SasayakiViewModel::load_match() {
    auto decoded = book_storage().load_sasayaki_match(state_->folder);
    if (!decoded.has_value()) {
        return false;
    }
    state_->match_cues.clear();
    state_->match_cues.reserve(decoded->matches.size());
    for (const niratan::sasayaki::SasayakiMatch& match : decoded->matches) {
        SasayakiCue cue;
        cue.id = match.id;
        cue.start_time = match.start_time;
        cue.end_time = match.end_time;
        cue.text = match.text;
        state_->match_cues.push_back(std::move(cue));
    }
    state_->has_match = !decoded->matches.empty();
    state_->match = std::move(decoded);
    return state_->has_match;
}

void SasayakiViewModel::rebuild_cue_list() {
    QVariantList list;
    if (state_->has_match) {
        const int chapter = ReaderViewModel::instance()->chapter_index();
        for (const niratan::sasayaki::SasayakiMatch& match : state_->match->matches) {
            if (match.chapter_index != chapter) {
                continue;
            }
            SasayakiCue cue;
            cue.id = match.id;
            cue.start_time = match.start_time;
            cue.end_time = match.end_time;
            cue.text = match.text;
            list.append(cue_entry(cue));
        }
    } else {
        for (const SasayakiCue& cue : state_->subtitle_cues) {
            list.append(cue_entry(cue));
        }
    }
    state_->cue_list = std::move(list);
    emit cuesChanged();
}

void SasayakiViewModel::set_match_error(const QString& message) {
    if (state_->match_error != message) {
        state_->match_error = message;
        emit matchErrorChanged();
    }
}

void SasayakiViewModel::set_match_progress(double progress) {
    if (state_->match_progress != progress) {
        state_->match_progress = progress;
        emit matchProgressChanged();
    }
}

}  // namespace niratan::viewmodels

// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_settings_view_model.h"
#include "dictionary_settings_view_model_catalog.h"

#include <QFutureWatcher>
#include <QPromise>
#include <QStringList>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "models/dictionary/dictionary_config.h"
#include "models/dictionary/dictionary_recommendation.h"
#include "models/profiles/content_language.h"
#include "services/dictionary/dictionary_config_store.h"
#include "services/dictionary/dictionary_store.h"
#include "services/storage/app_paths.h"

namespace niratan::viewmodels {
namespace {

using niratan::anki::DictionaryCategory;
using niratan::dictionary::DictionaryConfig;
using niratan::dictionary::DictionaryEntry;
using niratan::dictionary::DictionaryInfo;
using niratan::dictionary::DictionaryStore;
using niratan::dictionary::DictionaryType;
using niratan::profiles::ContentLanguage;

QString type_text(DictionaryType type) {
    const std::string_view name = dictionary::dictionary_type_name(type);
    return QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size()));
}

std::optional<DictionaryType> type_from(const QString& text) {
    if (text == QStringLiteral("Term")) {
        return DictionaryType::term;
    }
    if (text == QStringLiteral("Frequency")) {
        return DictionaryType::frequency;
    }
    if (text == QStringLiteral("Pitch")) {
        return DictionaryType::pitch;
    }
    if (text == QStringLiteral("Kanji")) {
        return DictionaryType::kanji;
    }
    return std::nullopt;
}

QString category_text(DictionaryCategory category) {
    switch (category) {
        case DictionaryCategory::none:
            return QStringLiteral("none");
        case DictionaryCategory::monolingual:
            return QStringLiteral("monolingual");
        case DictionaryCategory::bilingual:
            return QStringLiteral("bilingual");
        case DictionaryCategory::exclude:
            return QStringLiteral("exclude");
    }
    return QStringLiteral("none");
}

std::optional<DictionaryCategory> category_from(const QString& text) {
    if (text == QStringLiteral("none")) {
        return DictionaryCategory::none;
    }
    if (text == QStringLiteral("monolingual")) {
        return DictionaryCategory::monolingual;
    }
    if (text == QStringLiteral("bilingual")) {
        return DictionaryCategory::bilingual;
    }
    if (text == QStringLiteral("exclude")) {
        return DictionaryCategory::exclude;
    }
    return std::nullopt;
}

using dictionary_settings::Context;
using dictionary_settings::list_for;
using dictionary_settings::Lists;
using dictionary_settings::load_context;
using dictionary_settings::mutable_list_for;

QVariantList rows_for(const std::vector<DictionaryInfo>& dictionaries) {
    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(dictionaries.size()));
    for (const DictionaryInfo& dictionary : dictionaries) {
        QVariantMap row;
        row.insert(QStringLiteral("title"), QString::fromStdString(dictionary.title));
        row.insert(QStringLiteral("enabled"), dictionary.is_enabled);
        row.insert(QStringLiteral("category"), category_text(dictionary.category));
        rows.append(row);
    }
    return rows;
}

QStringList frequency_titles_for(const std::vector<DictionaryInfo>& dictionaries) {
    QStringList titles;
    for (const DictionaryInfo& dictionary : dictionaries) {
        if (dictionary.is_enabled) {
            titles.append(QString::fromStdString(dictionary.title));
        }
    }
    return titles;
}

QVariantList recommended_for(ContentLanguage language) {
    QVariantList recommended;
    for (const dictionary::DictionaryRecommendation& entry :
         dictionary::dictionary_recommendations_for(language)) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), QString::fromStdString(entry.id));
        map.insert(QStringLiteral("name"), QString::fromStdString(entry.name));
        map.insert(QStringLiteral("type"), type_text(entry.type));
        const std::string_view language_id = profiles::language_id(entry.language);
        map.insert(
            QStringLiteral("language"),
            QString::fromLatin1(language_id.data(), static_cast<qsizetype>(language_id.size())));
        map.insert(QStringLiteral("indexUrl"),
                   entry.index_url.has_value() ? QString::fromStdString(*entry.index_url)
                                               : QString());
        map.insert(QStringLiteral("downloadUrl"),
                   entry.download_url.has_value() ? QString::fromStdString(*entry.download_url)
                                                  : QString());
        recommended.append(map);
    }
    return recommended;
}

std::vector<DictionaryEntry> entries_for(const std::vector<DictionaryInfo>& dictionaries) {
    std::vector<DictionaryEntry> entries;
    entries.reserve(dictionaries.size());
    for (const DictionaryInfo& dictionary : dictionaries) {
        entries.push_back(DictionaryEntry{dictionary.title, dictionary.is_enabled, dictionary.order,
                                          dictionary.category});
    }
    return entries;
}

bool save_lists(const Context& context, const Lists& lists) {
    const auto directory = context.repository.profile_directory(context.profile.id);
    if (!directory.has_value()) {
        return false;
    }
    DictionaryConfig config;
    config.term_dictionaries = entries_for(lists.term_dictionaries);
    config.frequency_dictionaries = entries_for(lists.frequency_dictionaries);
    config.pitch_dictionaries = entries_for(lists.pitch_dictionaries);
    // saveDictionaryConfig always writes the kanji array.
    config.kanji_dictionaries = entries_for(lists.kanji_dictionaries);
    return dictionary::save_dictionary_config(*directory / "dictionary_config.json", config);
}

void renumber(std::vector<DictionaryInfo>& dictionaries) {
    for (std::size_t index = 0; index < dictionaries.size(); ++index) {
        dictionaries[index].order = static_cast<std::int64_t>(index);
    }
}

void reload_rows(QVariantList& rows, QVariantList& recommended, QStringList& frequency_titles,
                 const QString& type) {
    rows.clear();
    recommended.clear();
    frequency_titles.clear();
    const std::optional<Context> context = load_context();
    if (!context.has_value()) {
        return;
    }
    const std::optional<DictionaryType> selected = type_from(type);
    rows = rows_for(list_for(context->lists, selected.value_or(DictionaryType::term)));
    recommended = recommended_for(context->profile.language());
    frequency_titles = frequency_titles_for(context->lists.frequency_dictionaries);
}

}  // namespace

struct DictionarySettingsViewModel::State {
    QString dictionary_type = QStringLiteral("Term");
    QVariantList rows;
    QVariantList recommended;
    QStringList frequency_titles;
    QString error;
    quint64 load_generation = 0;
};

DictionarySettingsViewModel::DictionarySettingsViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {}

DictionarySettingsViewModel::~DictionarySettingsViewModel() = default;

QString DictionarySettingsViewModel::dictionary_type() const {
    return state_->dictionary_type;
}

QVariantList DictionarySettingsViewModel::rows() const {
    return state_->rows;
}

QVariantList DictionarySettingsViewModel::recommended() const {
    return state_->recommended;
}

QStringList DictionarySettingsViewModel::frequencyDictionaryTitles() const {
    return state_->frequency_titles;
}

QString DictionarySettingsViewModel::error() const {
    return state_->error;
}

void DictionarySettingsViewModel::openAsync() {
    if (!state_->error.isEmpty()) {
        state_->error.clear();
        emit errorChanged();
    }
    struct Result {
        QVariantList rows;
        QVariantList recommended;
        QStringList frequency_titles;
    };
    const QString type = state_->dictionary_type;
    const quint64 generation = ++state_->load_generation;
    auto* watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != state_->load_generation) {
            return;
        }
        Result result = watcher->result();
        state_->rows = std::move(result.rows);
        state_->recommended = std::move(result.recommended);
        state_->frequency_titles = std::move(result.frequency_titles);
        emit rowsChanged();
    });
    QPromise<Result> promise;
    watcher->setFuture(promise.future());
    QThreadPool::globalInstance()->start([work = std::move(promise), type]() mutable {
        work.start();
        Result result;
        reload_rows(result.rows, result.recommended, result.frequency_titles, type);
        work.addResult(std::move(result));
        work.finish();
    });
}

void DictionarySettingsViewModel::refresh() {
    ++state_->load_generation;
    if (!state_->error.isEmpty()) {
        state_->error.clear();
        emit errorChanged();
    }
    reload_rows(state_->rows, state_->recommended, state_->frequency_titles,
                state_->dictionary_type);
    emit rowsChanged();
}

void DictionarySettingsViewModel::selectType(const QString& type) {
    const std::optional<DictionaryType> parsed = type_from(type);
    if (!parsed.has_value()) {
        return;
    }
    ++state_->load_generation;
    state_->dictionary_type = type_text(*parsed);
    reload_rows(state_->rows, state_->recommended, state_->frequency_titles,
                state_->dictionary_type);
    emit rowsChanged();
}

void DictionarySettingsViewModel::setEnabled(const QString& title, bool enabled) {
    std::optional<Context> context = load_context();
    const std::optional<DictionaryType> type = type_from(state_->dictionary_type);
    if (!context.has_value() || !type.has_value()) {
        return;
    }
    std::vector<DictionaryInfo>& dictionaries = mutable_list_for(context->lists, *type);
    const auto found = std::ranges::find_if(dictionaries, [&title](const DictionaryInfo& info) {
        return QString::fromStdString(info.title) == title;
    });
    if (found == dictionaries.end()) {
        return;
    }
    found->is_enabled = enabled;
    save_lists(*context, context->lists);
    ++state_->load_generation;
    reload_rows(state_->rows, state_->recommended, state_->frequency_titles,
                state_->dictionary_type);
    emit rowsChanged();
}

void DictionarySettingsViewModel::setCategory(const QString& title, const QString& category) {
    if (type_from(state_->dictionary_type) != DictionaryType::term) {
        return;
    }
    const std::optional<DictionaryCategory> parsed = category_from(category);
    std::optional<Context> context = load_context();
    if (!context.has_value() || !parsed.has_value()) {
        return;
    }
    std::vector<DictionaryInfo>& dictionaries = context->lists.term_dictionaries;
    const auto found = std::ranges::find_if(dictionaries, [&title](const DictionaryInfo& info) {
        return QString::fromStdString(info.title) == title;
    });
    if (found == dictionaries.end()) {
        return;
    }
    found->category = *parsed;
    save_lists(*context, context->lists);
    ++state_->load_generation;
    reload_rows(state_->rows, state_->recommended, state_->frequency_titles,
                state_->dictionary_type);
    emit rowsChanged();
}

void DictionarySettingsViewModel::move(const QString& title, const QString& target_title) {
    std::optional<Context> context = load_context();
    const std::optional<DictionaryType> type = type_from(state_->dictionary_type);
    if (!context.has_value() || !type.has_value()) {
        return;
    }
    std::vector<DictionaryInfo>& dictionaries = mutable_list_for(context->lists, *type);
    const auto source = std::ranges::find_if(dictionaries, [&title](const DictionaryInfo& info) {
        return QString::fromStdString(info.title) == title;
    });
    const auto target =
        std::ranges::find_if(dictionaries, [&target_title](const DictionaryInfo& info) {
            return QString::fromStdString(info.title) == target_title;
        });
    if (source == dictionaries.end() || target == dictionaries.end() || source == target) {
        return;
    }
    const auto source_index = static_cast<std::int64_t>(source - dictionaries.begin());
    const auto target_index = static_cast<std::int64_t>(target - dictionaries.begin());
    // DictionaryReorder.destinationOffset: a target after the source means
    // insert after it, before it otherwise.
    const std::int64_t destination = target_index > source_index ? target_index + 1 : target_index;
    DictionaryInfo moved = std::move(*source);
    dictionaries.erase(dictionaries.begin() + source_index);
    const auto insert_at =
        static_cast<std::ptrdiff_t>(destination > source_index ? destination - 1 : destination);
    dictionaries.insert(dictionaries.begin() + insert_at, std::move(moved));
    renumber(dictionaries);
    save_lists(*context, context->lists);
    ++state_->load_generation;
    reload_rows(state_->rows, state_->recommended, state_->frequency_titles,
                state_->dictionary_type);
    emit rowsChanged();
}

void DictionarySettingsViewModel::remove(const QString& title) {
    std::optional<Context> context = load_context();
    const std::optional<DictionaryType> type = type_from(state_->dictionary_type);
    if (!context.has_value() || !type.has_value()) {
        return;
    }
    std::vector<DictionaryInfo>& dictionaries = mutable_list_for(context->lists, *type);
    const auto found = std::ranges::find_if(dictionaries, [&title](const DictionaryInfo& info) {
        return QString::fromStdString(info.title) == title;
    });
    if (found == dictionaries.end()) {
        return;
    }

    context->repository.remove_dictionary_references(title.toStdString(), title.toStdString());
    DictionaryStore(niratan::storage::dictionaries_root()).remove(*type, title.toStdString());
    dictionaries.erase(found);
    renumber(dictionaries);
    save_lists(*context, context->lists);
    ++state_->load_generation;
    reload_rows(state_->rows, state_->recommended, state_->frequency_titles,
                state_->dictionary_type);
    emit rowsChanged();
}

void DictionarySettingsViewModel::importFiles(const QVariantList& urls) {
    std::vector<QString> imported_titles;
    QStringList failed;
    const DictionaryStore store(niratan::storage::dictionaries_root());

    for (const QVariant& value : urls) {
        const QUrl url = value.toUrl();
        const QString file_name = url.fileName();
        if (!url.isLocalFile() || url.toLocalFile().isEmpty()) {
            failed.append(file_name);
            continue;
        }
        const std::string path = url.toLocalFile().toStdString();
        const dictionary::ImportOutcome outcome = store.import_zip(path);
        if (!outcome.ok) {
            QString failure = file_name;
            if (!outcome.error.empty()) {
                failure += QStringLiteral(": ") + QString::fromStdString(outcome.error);
            }
            failed.append(failure);
            continue;
        }
        const QString title = outcome.directory.empty()
            ? QString()
            : QString::fromStdString(outcome.directory.filename().string());
        if (!title.isEmpty()) {
            imported_titles.push_back(title);
        }
    }

    if (!imported_titles.empty()) {
        std::optional<Context> context = load_context();
        if (context.has_value()) {
            for (const QString& title : imported_titles) {
                for (std::vector<DictionaryInfo>* dictionaries :
                     {&context->lists.term_dictionaries, &context->lists.frequency_dictionaries,
                      &context->lists.pitch_dictionaries, &context->lists.kanji_dictionaries}) {
                    for (DictionaryInfo& info : *dictionaries) {
                        if (QString::fromStdString(info.title) == title) {
                            info.is_enabled = true;
                        }
                    }
                }
            }
            save_lists(*context, context->lists);
        }
        ++state_->load_generation;
        reload_rows(state_->rows, state_->recommended, state_->frequency_titles,
                    state_->dictionary_type);
        emit rowsChanged();
    }

    if (imported_titles.empty()) {
        state_->error = failed.isEmpty()
            ? tr("Failed to import dictionary")
            : tr("Failed to import dictionary:\n%1").arg(failed.join('\n'));
    } else if (!failed.isEmpty()) {
        state_->error = tr("Some dictionaries could not be imported:\n%1").arg(failed.join('\n'));
    } else {
        state_->error.clear();
    }
    emit errorChanged();
}

}  // namespace niratan::viewmodels

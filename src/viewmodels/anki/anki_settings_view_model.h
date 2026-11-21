// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Settings/AnkiView.swift (body and lifecycle), AnkiConnectView.swift
// (API key binding), Core/AnkiManager.swift (loadTransport, loadProfile,
// ensureAnkiConnectURL, save, setAnkiConnectURL, setAnkiConnectAPIKey,
// setFieldMapping, setTags, refreshAnkiConnectStatus, fetchAnkiConnect,
// applyFetchedAnkiMetadata, autofillFieldMappings, applyDefaultFieldMappings)
// and Models/Anki.swift (DuplicateScope, AnkiFieldTemplate).
//
// open() loads the active profile and the shared transport and lists, saves
// any changed mappings from template autofill, then pings. Text edits save
// immediately, verbatim except that a whitespace-only field mapping becomes
// empty. A model change uses the same template autofill; for an unknown
// template it preserves mappings, unlike fetch, which always prunes.
// Connect repairs a blank address and saves before pinging. Fetch pings before
// requesting decks, models and each model's fields, in order. Wrong list
// shapes stop silently; wrong field shapes skip that model. Lists and
// selections are replaced together only if there is usable metadata.
//
// Linux decisions: page-scoped connection status with no background retry,
// no live profile reload while open, a persisted API key, and save failures
// surfaced as errors. Media switches are Compress Images and Compact Glossaries.

#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::viewmodels {

class AnkiSettingsViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY settingsChanged)
    Q_PROPERTY(QString apiKey READ api_key WRITE setApiKey NOTIFY settingsChanged)
    Q_PROPERTY(QString selectedDeck READ selected_deck WRITE setSelectedDeck NOTIFY settingsChanged)
    Q_PROPERTY(QString selectedNoteType READ selected_note_type WRITE setSelectedNoteType NOTIFY
                   settingsChanged)
    Q_PROPERTY(QStringList availableDecks READ available_decks NOTIFY settingsChanged)
    Q_PROPERTY(QStringList availableNoteTypes READ available_note_types NOTIFY settingsChanged)
    Q_PROPERTY(bool hasSelectedNoteType READ has_selected_note_type NOTIFY settingsChanged)
    Q_PROPERTY(QStringList fields READ fields NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap fieldMappings READ field_mappings NOTIFY settingsChanged)
    Q_PROPERTY(QStringList mappingOptions READ mapping_options NOTIFY settingsChanged)
    Q_PROPERTY(bool hasFieldDefaults READ has_field_defaults NOTIFY settingsChanged)
    Q_PROPERTY(QString tags READ tags WRITE setTags NOTIFY settingsChanged)
    Q_PROPERTY(
        bool allowDuplicates READ allow_duplicates WRITE setAllowDuplicates NOTIFY settingsChanged)
    Q_PROPERTY(
        QString duplicateScope READ duplicate_scope WRITE setDuplicateScope NOTIFY settingsChanged)
    Q_PROPERTY(
        bool checkAllModels READ check_all_models WRITE setCheckAllModels NOTIFY settingsChanged)
    Q_PROPERTY(
        bool compressImages READ compress_images WRITE setCompressImages NOTIFY settingsChanged)
    Q_PROPERTY(bool compactGlossaries READ compact_glossaries WRITE setCompactGlossaries NOTIFY
                   settingsChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorMessage READ error_message NOTIFY errorMessageChanged)

   public:
    explicit AnkiSettingsViewModel(QObject* parent = nullptr);
    ~AnkiSettingsViewModel() override;

    QString address() const;
    QString api_key() const;
    QString selected_deck() const;
    QString selected_note_type() const;
    QStringList available_decks() const;
    QStringList available_note_types() const;
    bool has_selected_note_type() const;
    QStringList fields() const;
    QVariantMap field_mappings() const;
    QStringList mapping_options() const;
    bool has_field_defaults() const;
    QString tags() const;
    bool allow_duplicates() const;
    QString duplicate_scope() const;
    bool check_all_models() const;
    bool compress_images() const;
    bool compact_glossaries() const;
    bool connected() const;
    bool busy() const;
    QString error_message() const;

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void connectToAnki();
    Q_INVOKABLE void fetchMetadata();
    Q_INVOKABLE void applyDefaults();
    Q_INVOKABLE void dismissError();
    Q_INVOKABLE void setFieldMapping(const QString& field, const QString& mapping);

    void setAddress(const QString& address);
    void setApiKey(const QString& api_key);
    void setSelectedDeck(const QString& deck);
    void setSelectedNoteType(const QString& note_type);
    void setTags(const QString& tags);
    void setAllowDuplicates(bool allow);
    void setDuplicateScope(const QString& scope);
    void setCheckAllModels(bool check);
    void setCompressImages(bool compress);
    void setCompactGlossaries(bool compact);

   signals:
    void settingsChanged();
    void connectedChanged();
    void busyChanged();
    void errorMessageChanged();

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::viewmodels

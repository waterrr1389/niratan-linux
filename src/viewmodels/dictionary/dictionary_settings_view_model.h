// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Dictionaries settings page state and commands, following
// Features/Settings/DictionaryView.swift (body, dictionaryTypePicker,
// deleteDictionary, reorderDictionary, DictionaryCategoryPicker) and
// Core/DictionaryManager.swift (loadDictionaries,
// applyActiveProfileDictionaryConfiguration, toggleDictionary,
// setDictionaryCategory, moveDictionary, updateOrder, deleteDictionary,
// importDictionary, enableDictionaries, saveDictionaryConfig,
// saveCollapsedDictionaries), with Core/ProfileRepository.swift
// (removeDictionaryReferences) and Models/Dictionary.swift
// (DictionaryType, DictionaryCategory, DictionaryReorder.destinationOffset).
//
// This page manages the dictionary library; the lookup behaviour settings stay
// in SettingsPage's Dictionary section. It is a pushed page reached from the
// Settings rail.
//
// Rows are the selected type's resolved list of the active profile, in list
// order, as {"title", "enabled", "category"}; a new page starts on Term and
// the type picker accepts the Swift raw values Term, Frequency, Pitch and
// Kanji. Every command resolves the active profile again, reads its
// dictionary_config.json, scans the typed store and resolves each type; it
// writes the four lists through save_dictionary_config, so the selected list
// changes and the other lists keep their entries and orders.
//
// setEnabled and setCategory change the selected type only; setCategory is a
// no-op for every type but Term because only term entries carry a category.
// move uses DictionaryReorder.destinationOffset (a target after the source
// means insert after it) and renumbers the selected list to 0..n-1; the other
// lists are not renumbered. remove follows deleteDictionary: it removes the
// title from every profile and collapsed set through
// remove_dictionary_references, deletes the selected type's installed
// directory only, drops and renumbers the selected list, then saves. A copy
// under another type stays installed and stays in the active profile's config.
//
// importFiles imports local zips synchronously through DictionaryStore and
// keeps one message for the failures; when at least one file imports, every
// imported title is enabled in all four lists and the config is saved once.
// Swift imports in a detached task with a loading overlay and an isImporting
// flag; the port has no busy property (the import is synchronous) and no
// in-app downloads (Decision O3). A dictionary row shows its directory name
// only: the M1 list carries no revision caption. Reordering is exposed as Up
// and Down buttons instead of Swift's drag gesture. There is no live watcher;
// refresh() reloads after external edits.
//
// frequencyDictionaryTitles follows DictionaryView.frequencyDictionaryTitles:
// the active profile's resolved Frequency list filtered to enabled entries, in
// list order, as titles only. The Settings page's frequency sort picker offers
// them. Every command and refresh() recompute it from the same context, so
// enabling, disabling, importing and removing update it with rows.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::viewmodels {

class DictionarySettingsViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString dictionaryType READ dictionary_type NOTIFY rowsChanged)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(QVariantList recommended READ recommended NOTIFY rowsChanged)
    Q_PROPERTY(
        QStringList frequencyDictionaryTitles READ frequencyDictionaryTitles NOTIFY rowsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

   public:
    explicit DictionarySettingsViewModel(QObject* parent = nullptr);
    ~DictionarySettingsViewModel() override;

    QString dictionary_type() const;
    QVariantList rows() const;
    QVariantList recommended() const;
    QStringList frequencyDictionaryTitles() const;
    QString error() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectType(const QString& type);
    Q_INVOKABLE void importFiles(const QVariantList& urls);
    Q_INVOKABLE void setEnabled(const QString& title, bool enabled);
    Q_INVOKABLE void setCategory(const QString& title, const QString& category);
    Q_INVOKABLE void move(const QString& title, const QString& target_title);
    Q_INVOKABLE void remove(const QString& title);

   signals:
    void rowsChanged();
    void errorChanged();

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::viewmodels

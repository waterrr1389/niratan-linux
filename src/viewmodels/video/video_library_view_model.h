// SPDX-License-Identifier: GPL-3.0-or-later
//
// State and commands for the local video library page, following
// Features/Video/VideoLibraryViewModel.swift: `VideoLibraryDisplayMode`
// (`titleKey`, `usesCollapsibleSections`), `VideoLibrarySortOption`,
// `VideoLibraryLayoutMode`, `VideoLibraryRow`, `VideoLibrarySection`,
// `VideoLibraryItemOrganization`, `VideoLibrarySourceSummary`, and the view
// model's `load`, `refreshPlaybackHistory`, `addFolders`,
// `refreshAllSources`, `refreshSource`, `removeSource`, `select`,
// `clearSelection`, `openURL`, `openFromBeginningURL`, `markWatched`,
// `clearProgress`, `setDisplayTitle`, `setFavorite`, `setTags`, `bindSubtitle`,
// `createCollection`, `createSmartCollection`, `updateSmartCollection`,
// `smartCollectionPreviewRows`, `setCollectionMembership`,
// `removeCollection`, `markSelectedWatched`, `clearSelectedProgress`,
// `removeMissingItems`, `sections`, `modeCounts`, `queriedRows`,
// `isFiltering`, `sortRows`, `recentPlaybackSort`, `titleSort`, `seriesSort`,
// `groupedSections`, `organization`, `subtitleCandidateURL`,
// `subtitleURLForOpening`, `sourceSummaries`, `emptyTitleKey`,
// `emptyDescriptionKey`, `hasSources`, `playbackHistoryRevision` and
// `VideoLibraryRow.matches`. Read-only playback state comes from the existing
// `services/player/playback_history.h`.
//
// Mode, sort and layout are the Swift case names as strings:
// displayMode one of continueWatching, favorites, unwatched, finished,
// missing, recent, all, needsReview, series, folders, collections; sortOption
// one of recentPlayback, title, modifiedDate, folder; layoutMode "list" or
// "posters". The page binds to these strings.
//
// `id` is the item's absolute path everywhere. Rows are QVariantMaps with the
// camelCase members of VideoLibraryRow; `sections` holds {id, title, rows,
// collectionId}, `modeCounts` is keyed by the Swift case names and leaves
// needsReview out, and `sourceSummaries` holds {id, name, path, itemCount,
// inProgressCount, missingCount, lastScannedAt, lastError}. Absent optionals
// are invalid QVariants, dates are ISO 8601 strings and UUIDs are uppercase.
//
// Selection is one id plus the batch list. `selectedItemIDs` is writable so
// the page can mark several rows; replacing it leaves `selectedItemID`
// unchanged. The two library layout values are stored in
// <data root>/settings.json through player/video_settings.h:
// videoLibraryLayoutMode defaults to "posters", videoLibraryPosterWidth to
// 160 and is clamped to 110...240. displayMode and sortOption are not stored.
//
// Two constructors are not needed: the store, the playback history and the
// settings document all resolve from the data root, so a test points
// XDG_DATA_HOME at a temporary directory as the other view models do.
// Everything is read on construction; commands keep the store and the history
// cache in step.

#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "services/player/playback_history.h"
#include "services/video/library_catalog.h"

namespace niratan::viewmodels {

class VideoLibraryViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(QString sortOption READ sortOption WRITE setSortOption NOTIFY sortOptionChanged)
    Q_PROPERTY(QString layoutMode READ layoutMode WRITE setLayoutMode NOTIFY layoutModeChanged)
    Q_PROPERTY(double posterSize READ posterSize WRITE setPosterSize NOTIFY posterSizeChanged)
    Q_PROPERTY(double posterWidth READ posterWidth NOTIFY posterSizeChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(bool showUnfinishedOnly READ showUnfinishedOnly WRITE setShowUnfinishedOnly NOTIFY
                   showUnfinishedOnlyChanged)
    Q_PROPERTY(QString selectedItemID READ selectedItemID NOTIFY selectionChanged)
    Q_PROPERTY(QStringList selectedItemIDs READ selectedItemIDs WRITE setSelectedItemIDs NOTIFY
                   selectionChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(bool hasSources READ hasSources NOTIFY hasSourcesChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString emptyTitle READ emptyTitle NOTIFY emptyTitleChanged)
    Q_PROPERTY(QString emptyDescription READ emptyDescription NOTIFY emptyDescriptionChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY sectionsChanged)
    Q_PROPERTY(QVariantMap modeCounts READ modeCounts NOTIFY modeCountsChanged)
    Q_PROPERTY(QVariantList sourceSummaries READ sourceSummaries NOTIFY sourceSummariesChanged)
    Q_PROPERTY(int playbackHistoryRevision READ playbackHistoryRevision NOTIFY
                   playbackHistoryRevisionChanged)

   public:
    explicit VideoLibraryViewModel(QObject* parent = nullptr);
    ~VideoLibraryViewModel() override;

    QString displayMode() const { return display_mode_; }
    void setDisplayMode(const QString& mode);
    QString sortOption() const { return sort_option_; }
    void setSortOption(const QString& option);
    QString layoutMode() const { return layout_mode_; }
    void setLayoutMode(const QString& mode);
    double posterSize() const { return poster_size_; }
    void setPosterSize(double width);
    double posterWidth() const;
    QString searchText() const { return search_text_; }
    void setSearchText(const QString& text);
    bool showUnfinishedOnly() const { return show_unfinished_only_; }
    void setShowUnfinishedOnly(bool enabled);
    QString selectedItemID() const { return selected_item_id_; }
    QStringList selectedItemIDs() const { return selected_item_ids_; }
    void setSelectedItemIDs(const QStringList& ids);
    bool isScanning() const { return scanning_; }
    bool hasSources() const;
    QString errorMessage() const { return error_message_; }
    QString emptyTitle() const;
    QString emptyDescription() const;
    QVariantList sections() const;
    QVariantMap modeCounts() const;
    QVariantList sourceSummaries() const;
    int playbackHistoryRevision() const { return playback_history_revision_; }

    Q_INVOKABLE void load();
    Q_INVOKABLE void refreshPlaybackHistory();
    Q_INVOKABLE void addFolders(const QStringList& folders);
    Q_INVOKABLE void refreshAllSources();
    Q_INVOKABLE void refreshSource(const QString& id);
    Q_INVOKABLE void removeSource(const QString& id);
    Q_INVOKABLE void select(const QString& id);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QString openUrl(const QString& id);
    Q_INVOKABLE QString openFromBeginningUrl(const QString& id);
    Q_INVOKABLE QString subtitleUrlForOpening(const QString& id);
    Q_INVOKABLE void markWatched(const QString& id);
    Q_INVOKABLE void clearProgress(const QString& id);
    Q_INVOKABLE void setDisplayTitle(const QString& id, const QString& title);
    Q_INVOKABLE void setFavorite(const QString& id, bool favorite);
    Q_INVOKABLE void setTags(const QString& id, const QStringList& tags);
    Q_INVOKABLE void bindSubtitle(const QString& id, const QString& path);
    Q_INVOKABLE QVariantMap createCollection(const QString& name, const QStringList& ids);
    Q_INVOKABLE QVariantMap createSmartCollection(const QString& name, const QVariantList& rules);
    Q_INVOKABLE void updateSmartCollection(const QString& id, const QString& name,
                                           const QVariantList& rules);
    Q_INVOKABLE void setCollectionMembership(const QString& id, const QString& collection_id,
                                             bool included);
    Q_INVOKABLE void removeCollection(const QString& collection_id);
    Q_INVOKABLE QVariantList smartCollectionPreviewRows(const QVariantList& rules,
                                                        int limit = 8) const;
    Q_INVOKABLE void markSelectedWatched();
    Q_INVOKABLE void clearSelectedProgress();
    // An empty source id cleans every source, as the Swift optional does.
    Q_INVOKABLE int removeMissingItems(const QString& source_id);

   signals:
    void displayModeChanged();
    void sortOptionChanged();
    void layoutModeChanged();
    void posterSizeChanged();
    void searchTextChanged();
    void showUnfinishedOnlyChanged();
    void selectionChanged();
    void scanningChanged();
    void hasSourcesChanged();
    void errorMessageChanged();
    void emptyTitleChanged();
    void emptyDescriptionChanged();
    void sectionsChanged();
    void modeCountsChanged();
    void sourceSummariesChanged();
    void playbackHistoryRevisionChanged();

   private:
    QVariantList sectionsData() const;
    QVariantMap modeCountsData() const;
    QVariantList sourceSummariesData() const;
    QVariantList previewRows(const QVariantList& rules, int limit) const;

    void rebuildPlaybackHistoryCache();
    void synchronizePlaybackHistoryCache(const QStringList& keys, bool force);
    void notifyRows();
    void showError(const QString& message);

    video::LibraryStore store_;
    player::PlaybackHistory history_;
    QString display_mode_ = QStringLiteral("continueWatching");
    QString sort_option_ = QStringLiteral("title");
    QString layout_mode_;
    double poster_size_ = 160;
    QString search_text_;
    bool show_unfinished_only_ = false;
    QString selected_item_id_;
    QStringList selected_item_ids_;
    bool scanning_ = false;
    QString error_message_;
    int playback_history_revision_ = 0;
    QHash<QString, player::PlaybackState> playback_states_;
    QSet<QString> cached_playback_keys_;
};

}  // namespace niratan::viewmodels

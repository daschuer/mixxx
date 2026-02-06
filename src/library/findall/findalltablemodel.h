#pragma once

#include "library/trackset/tracksettablemodel.h"
#include "util/duration.h"

class FindAllTableModel final : public TrackSetTableModel {
    Q_OBJECT

  public:
    FindAllTableModel(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            const char* settingsNamespace);
    ~FindAllTableModel() final = default;

    void setTableModel();

    bool appendTrack(TrackId trackId);

    void shuffleTracks(const QModelIndexList& shuffle = QModelIndexList(),
            const QModelIndex& exclude = QModelIndex());
    void orderTracksByCurrPos();

    bool isColumnInternal(int column) final;
    bool isColumnHiddenByDefault(int column) final;
    /// This function should only be used by AUTODJ

    /// Get the total duration of all tracks referenced by the given model indices
    mixxx::Duration getTotalDuration(const QModelIndexList& indices);
    const QList<int> getSelectedPositions(const QModelIndexList& indices) const override;

    Capabilities getCapabilities() const final;

    QString modelKey(bool noSearch) const override;

    void select();

    void search(const QString& searchText);

  private slots:
    // void playlistsChanged(const QSet<int>& playlistIds);

  signals:
    void firstTrackChanged();

  private:
    void initSortColumnMapping() override;

    QString m_searchText;
};

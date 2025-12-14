#pragma once

#include <QHash>
#include <QObject>
#include <QSet>

#include "library/dao/dao.h"
#include "track/trackid.h"
#include "util/class.h"

class AutoDJProcessor;
class QSqlDatabase;

class IndexDAO : public QObject, public virtual DAO {
    Q_OBJECT
  public:
    enum HiddenType {
        PLHT_NOT_HIDDEN = 0,
        PLHT_AUTO_DJ = 1,
        PLHT_SET_LOG = 2,
        PLHT_UNKNOWN = -1
    };

    enum class AutoDJSendLoc {
        TOP,
        BOTTOM,
        REPLACE,
    };

    IndexDAO();
    ~IndexDAO() override = default;

    void initialize(const QSqlDatabase& database) override;

    // Create a playlist, fails with -1 if already exists
    bool createPlaylist(const QString& name, const HiddenType type = PLHT_NOT_HIDDEN);
    // Create a playlist, appends "(n)" if already exists, name becomes the new name
    int createUniquePlaylist(QString* pName, const HiddenType type = PLHT_NOT_HIDDEN);
    // Delete a playlist

    QList<TrackId> getTrackIds(const int playlistId) const;
    QList<TrackId> getTrackIdsInPlaylistOrder(const int playlistId) const;
    // Returns the maximum position of the given playlist
    int getMaxPosition(const int playlistId) const;
    // Insert a track into a specific position in a playlist
    // Append all the tracks in the source playlist to the target playlist.
    bool copyPlaylistTracks(const int sourcePlaylistID, const int targetPlaylistID);
    // Returns the number of tracks in the given playlist.
    int tracksInPlaylist(const int playlistId) const;
    // This receives a track list that represents the current order (sorted by BPM for example)
    // and adopts this order for `position` in the playlist.
    // Returns true on success.
    void orderTracksByCurrPos(const int playlistId, QList<std::pair<TrackId, int>>& newOrder);
    bool isTrackInPlaylist(TrackId trackId, const int playlistId) const;

    void getPlaylistsTrackIsIn(TrackId trackId, QSet<int>* playlistSet) const;

  signals:
    void added(int playlistId);
    void deleted(int playlistId);
    void renamed(int playlistId, const QString& newName);
    void lockChanged(const QSet<int>& playlistIds);
    void trackAdded(int playlistId, TrackId trackId, int position);
    void trackRemoved(int playlistId, TrackId trackId, int position);
    // added / removed / un/locked. Triggers playlist features to update the sidebar
    void playlistContentChanged(const QSet<int>& playlistIds);
    // Separate signals for PlaylistTableModel
    void tracksAdded(const QSet<int>& playlistIds);
    void tracksMoved(const QSet<int>& playlistIds);
    void tracksRemoved(const QSet<int>& playlistIds);
    void tracksRemovedFromPlayedHistory(const QSet<TrackId>& playedTrackIds);

  private:
    bool removeTracksFromPlaylist(int playlistId, int startIndex);
    void removeTracksFromPlaylistInner(int playlistId, int position);
    void removeTracksFromPlaylistByIdInner(int playlistId, TrackId trackId);
    void searchForDuplicateTrack(const int fromPosition,
            const int toPosition,
            TrackId trackID,
            const int excludePosition,
            const int otherTrackPosition,
            const QHash<int, TrackId>* pTrackPositionIds,
            int* pTrackDistance);
    void populatePlaylistMembershipCache();

    QMultiHash<TrackId, int> m_playlistsTrackIsIn;
    AutoDJProcessor* m_pAutoDJProcessor;
    DISALLOW_COPY_AND_ASSIGN(IndexDAO);
};

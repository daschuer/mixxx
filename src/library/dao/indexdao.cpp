#include "library/dao/indexdao.h"

#include <QRandomGenerator>
#include <QtDebug>

#include "library/autodj/autodjprocessor.h"
#include "library/dao/trackschema.h"
#include "library/queryutil.h"
#include "moc_indexdao.cpp"
#include "util/db/dbconnection.h"
#include "util/db/fwdsqlquery.h"
#include "util/make_const_iterator.h"
#include "util/math.h"

IndexDAO::IndexDAO() {
}

void IndexDAO::initialize(const QSqlDatabase& pDatabase) {
    DAO::initialize(pDatabase);
    populatePlaylistMembershipCache();
}

void IndexDAO::populatePlaylistMembershipCache() {
    // Minor optimization: reserve space in m_playlistsTrackIsIn.
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT COUNT(*) from " PLAYLIST_TRACKS_TABLE));
    if (query.exec() && query.next()) {
        m_playlistsTrackIsIn.reserve(query.value(0).toInt());
    } else {
        LOG_FAILED_QUERY(query);
    }

    // now fetch all Tracks from all playlists and insert them into the hashmap
    query.prepare(QStringLiteral(
            "SELECT track_id, playlist_id from " PLAYLIST_TRACKS_TABLE));
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    const int trackIdColumn = query.record().indexOf(PLAYLISTTRACKSTABLE_TRACKID);
    const int playlistIdColumn = query.record().indexOf(PLAYLISTTRACKSTABLE_PLAYLISTID);
    while (query.next()) {
        m_playlistsTrackIsIn.insert(TrackId(query.value(trackIdColumn)),
                query.value(playlistIdColumn).toInt());
    }
}

bool IndexDAO::createPlaylist(const QString& name, const HiddenType hidden) {
    // qDebug() << "PlaylistDAO::createPlaylist"
    //          << QThread::currentThread()
    //          << m_database.connectionName();
    //  Start the transaction
    ScopedTransaction transaction(m_database);

    // Find out the highest position for the existing playlists so we know what
    // position this playlist should have.
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT max(position) as posmax FROM Playlists"));

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    // Get the id of the last playlist.
    int position = 0;
    if (query.next()) {
        position = query.value(query.record().indexOf("posmax")).toInt();
        position++; // Append after the last playlist.
    }

    // qDebug() << "Inserting playlist" << name << "at position" << position;

    query.prepare(QStringLiteral(
            "INSERT INTO Playlists (name, position, hidden, date_created, date_modified) "
            "VALUES (:name, :position, :hidden,  CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"));
    query.bindValue(":name", name);
    query.bindValue(":position", position);
    query.bindValue(":hidden", static_cast<int>(hidden));

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    int playlistId = query.lastInsertId().toInt();
    // Commit the transaction
    transaction.commit();
    emit added(playlistId);
    return true;
}

QList<TrackId> IndexDAO::getTrackIds(const int playlistId) const {
    QList<TrackId> trackIds;

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT DISTINCT track_id FROM PlaylistTracks "
            "WHERE playlist_id = :id"));
    query.bindValue(":id", playlistId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return trackIds;
    }

    const int trackIdColumn = query.record().indexOf(PLAYLISTTRACKSTABLE_TRACKID);
    while (query.next()) {
        trackIds.append(TrackId(query.value(trackIdColumn)));
    }
    return trackIds;
}

QList<TrackId> IndexDAO::getTrackIdsInPlaylistOrder(const int playlistId) const {
    QList<TrackId> trackIds;

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT track_id FROM PlaylistTracks "
            "WHERE playlist_id = :id ORDER BY position ASC"));
    query.bindValue(":id", playlistId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return trackIds;
    }

    const int trackIdColumn = query.record().indexOf("track_id");
    while (query.next()) {
        trackIds.append(TrackId(query.value(trackIdColumn)));
    }
    return trackIds;
}

bool IndexDAO::copyPlaylistTracks(const int sourcePlaylistID, const int targetPlaylistID) {
    // Start the transaction
    ScopedTransaction transaction(m_database);

    // Copy the new tracks after the last track in the target playlist.
    int positionOffset = getMaxPosition(targetPlaylistID);

    // Copy the tracks from one playlist to another, adjusting the position of
    // each copied track, and preserving the date/time added.
    // INSERT INTO PlaylistTracks (playlist_id, track_id, position,
    // pl_datetime_added) SELECT :target_plid, track_id, position +
    // :position_offset, pl_datetime_added FROM PlaylistTracks WHERE playlist_id
    // = :source_plid;
    QSqlQuery query(m_database);
    query.prepare(
            QStringLiteral(
                    "INSERT INTO " PLAYLIST_TRACKS_TABLE
                    " (%1, %2, %3, %4) SELECT :target_plid, %2, "
                    "%3 + :position_offset, %4 FROM " PLAYLIST_TRACKS_TABLE
                    " WHERE %1 = :source_plid")
                    .arg(
                            PLAYLISTTRACKSTABLE_PLAYLISTID,
                            PLAYLISTTRACKSTABLE_TRACKID,
                            PLAYLISTTRACKSTABLE_POSITION,
                            PLAYLISTTRACKSTABLE_DATETIMEADDED));
    query.bindValue(":position_offset", positionOffset);
    query.bindValue(":source_plid", sourcePlaylistID);
    query.bindValue(":target_plid", targetPlaylistID);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    // Query each added track and its new position.
    // SELECT track_id, position FROM PlaylistTracks WHERE playlist_id =
    // :target_plid AND position > :position_offset;
    query.prepare(
            QStringLiteral(
                    "SELECT %2, %3 FROM " PLAYLIST_TRACKS_TABLE
                    " WHERE %1 = :target_plid AND %3 > :position_offset")
                    .arg(
                            PLAYLISTTRACKSTABLE_PLAYLISTID,
                            PLAYLISTTRACKSTABLE_TRACKID,
                            PLAYLISTTRACKSTABLE_POSITION));
    query.bindValue(":target_plid", targetPlaylistID);
    query.bindValue(":position_offset", positionOffset);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    // Commit the transaction
    transaction.commit();

    // Let subscribers know about each added track.
    while (query.next()) {
        TrackId copiedTrackId(query.value(0));
        int copiedPosition = query.value(1).toInt();
        m_playlistsTrackIsIn.insert(copiedTrackId, targetPlaylistID);
        emit trackAdded(targetPlaylistID, copiedTrackId, copiedPosition);
    }
    emit tracksAdded(QSet<int>{targetPlaylistID});
    emit playlistContentChanged(QSet<int>{targetPlaylistID});
    return true;
}

int IndexDAO::getMaxPosition(const int playlistId) const {
    // Find out the highest position existing in the playlist so we know what
    // position this track should have.
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT max(position) as position FROM PlaylistTracks "
            "WHERE playlist_id = :id"));
    query.bindValue(":id", playlistId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    // Get the position of the highest track in the playlist.
    int position = 0;
    if (query.next()) {
        position = query.value(query.record().indexOf("position")).toInt();
    }
    return position;
}

int IndexDAO::tracksInPlaylist(const int playlistId) const {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT COUNT(id) AS count FROM PlaylistTracks "
            "WHERE playlist_id = :playlist_id"));
    query.bindValue(":playlist_id", playlistId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query) << "Couldn't get the number of tracks in playlist"
                                << playlistId;
        return -1;
    }
    int count = -1;
    const int countColumn = query.record().indexOf("count");
    while (query.next()) {
        count = query.value(countColumn).toInt();
    }
    return count;
}

void IndexDAO::orderTracksByCurrPos(const int playlistId,
        QList<std::pair<TrackId, int>>& newOrder) {
    if (newOrder.isEmpty() ||
            newOrder.size() != tracksInPlaylist(playlistId)) {
        return;
    }

    ScopedTransaction transaction(m_database);
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "UPDATE PlaylistTracks "
            "SET position=:new_pos "
            "WHERE position=:old_pos AND "
            "track_id=:track_id AND "
            "playlist_id=:pl_id"));
    int newPos = 1;
    for (auto [trackId, oldPos] : newOrder) {
        VERIFY_OR_DEBUG_ASSERT(trackId.isValid()) {
            return;
        }
        query.bindValue(":new_pos", newPos++);
        query.bindValue(":old_pos", oldPos);
        query.bindValue(":track_id", trackId.toVariant());
        query.bindValue(":pl_id", playlistId);
        if (!query.exec()) {
            // We temporarily have duplicate positions, so abort the entire operation
            // to not leave the playlist with an invalid state.
            LOG_FAILED_QUERY(query);
            return;
        }
    }

    transaction.commit();

    emit tracksMoved(QSet<int>{playlistId});
}

bool IndexDAO::isTrackInPlaylist(TrackId trackId, const int playlistId) const {
    return m_playlistsTrackIsIn.contains(trackId, playlistId);
}

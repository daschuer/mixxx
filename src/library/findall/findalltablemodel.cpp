#include "library/findall/findalltablemodel.h"

#include "library/dao/playlistdao.h"
#include "library/dao/trackschema.h"
#include "library/queryutil.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "moc_findalltablemodel.cpp"

namespace {

} // anonymous namespace

FindAllTableModel::FindAllTableModel(QObject* parent,
        TrackCollectionManager* pTrackCollectionManager,
        const char* settingsNamespace)
        : TrackSetTableModel(parent, pTrackCollectionManager, settingsNamespace) {
}

void FindAllTableModel::initSortColumnMapping() {
    // Add a bijective mapping between the SortColumnIds and column indices
    for (int i = 0; i < static_cast<int>(TrackModel::SortColumnId::IdMax); ++i) {
        m_columnIndexBySortColumnId[i] = -1;
    }
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Artist)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ARTIST);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Title)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TITLE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Album)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ALBUM);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::AlbumArtist)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ALBUMARTIST);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Year)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_YEAR);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Genre)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_GENRE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Composer)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COMPOSER);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Grouping)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_GROUPING);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::TrackNumber)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TRACKNUMBER);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::FileType)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_FILETYPE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::NativeLocation)] =
            fieldIndex(ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Comment)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COMMENT);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Duration)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_DURATION);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::BitRate)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_BITRATE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Bpm)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_BPM);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::ReplayGain)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_REPLAYGAIN);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::DateTimeAdded)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_DATETIMEADDED);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::TimesPlayed)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::LastPlayedAt)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_LAST_PLAYED_AT);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Rating)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_RATING);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Key)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_KEY);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Preview)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_PREVIEW);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Color)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COLOR);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::CoverArt)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Position)] =
            fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_POSITION);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::PlaylistDateTimeAdded)] =
            fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_DATETIMEADDED);

    m_sortColumnIdByColumnIndex.clear();
    for (int i = static_cast<int>(TrackModel::SortColumnId::IdMin);
            i < static_cast<int>(TrackModel::SortColumnId::IdMax);
            ++i) {
        TrackModel::SortColumnId sortColumn = static_cast<TrackModel::SortColumnId>(i);
        m_sortColumnIdByColumnIndex.insert(
                m_columnIndexBySortColumnId[static_cast<int>(sortColumn)],
                sortColumn);
    }
}

void FindAllTableModel::setTableModel() {
    QSqlQuery ftsCreateQuery(m_database);
    ftsCreateQuery.prepare(
            "CREATE VIRTUAL TABLE IF NOT EXISTS library_fts USING "
            "fts5(track_id UNINDEXED, artist, title, album, genre)");
    if (!ftsCreateQuery.exec()) {
        qDebug() << ftsCreateQuery.executedQuery() << ftsCreateQuery.lastError();
    }

    QSqlQuery ftsPopulateQuery(m_database);
    ftsPopulateQuery.prepare(
            "INSERT OR REPLACE INTO library_fts (track_id, artist, title, album, genre) "
            "SELECT id, artist, title, album, genre FROM library");
    if (!ftsPopulateQuery.exec()) {
        qDebug() << ftsPopulateQuery.executedQuery() << ftsPopulateQuery.lastError();
    }

    const QString tableName("hidden_songs");

    QStringList columns;
    columns << "library." + LIBRARYTABLE_ID;

    QSqlQuery query(m_database);
    query.prepare(
            "CREATE TEMPORARY VIEW IF NOT EXISTS " + tableName +
            " AS SELECT " + columns.join(",") +
            " FROM library "
            "INNER JOIN track_locations "
            "ON library.location=track_locations.id "
            "WHERE mixxx_deleted=1");
    if (!query.exec()) {
        qDebug() << query.executedQuery() << query.lastError();
    }

    // Print out any SQL error, if there was one.
    if (query.lastError().isValid()) {
        qDebug() << __FILE__ << __LINE__ << query.lastError();
    }

    QStringList tableColumns;
    tableColumns << LIBRARYTABLE_ID;
    setTable(tableName,
            LIBRARYTABLE_ID,
            std::move(tableColumns),
            m_pTrackCollectionManager->internalCollection()->getTrackSource());
    setDefaultSort(fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ARTIST), Qt::AscendingOrder);
    setSearch("");

    // qDebug() << "FindAllTableModel::selectPlaylist" << playlistId;

    /*

if (m_iPlaylistId == playlistId) {
    qDebug() << "Already focused on playlist " << playlistId;
    return;
}
// Store search text
QString currSearch = currentSearch();
if (m_iPlaylistId != kInvalidPlaylistId) {
    if (!currSearch.trimmed().isEmpty()) {
        m_searchTexts.insert(m_iPlaylistId, currSearch);
    } else {
        m_searchTexts.remove(m_iPlaylistId);
    }
}

if (!m_keepHiddenTracks) {
    // From Mixxx 2.1 we drop tracks that have been explicitly deleted
    // in the library (mixxx_deleted = 0) from playlists.
    // These invisible tracks, consuming a playlist position number were
    // a source user of confusion in the past.
    m_pTrackCollectionManager->internalCollection()->getPlaylistDAO().removeHiddenTracks(m_iPlaylistId);
}

// Note: don't set m_iPlaylistId = playlistId before removing hidden tracks,
// else (if tracks are removed) PlaylistDAO::tracksRemoved would trigger
// playlistsChanged() which would call select() -- which is simply not required
// because we'll trigger that ourselves later on.
m_iPlaylistId = playlistId;

QString playlistTableName = "playlist_" + QString::number(m_iPlaylistId);
QSqlQuery query(m_database);
FieldEscaper escaper(m_database);

QStringList columns;
columns << PLAYLISTTRACKSTABLE_TRACKID + " AS " + LIBRARYTABLE_ID
        << PLAYLISTTRACKSTABLE_POSITION
        << PLAYLISTTRACKSTABLE_DATETIMEADDED
        << "'' AS " + LIBRARYTABLE_PREVIEW
        // For sorting the cover art column we give LIBRARYTABLE_COVERART
        // the same value as the cover digest.
        << LIBRARYTABLE_COVERART_DIGEST + " AS " + LIBRARYTABLE_COVERART;

QString queryString = QString(
        "CREATE TEMPORARY VIEW IF NOT EXISTS %1 AS "
        "SELECT %2 FROM PlaylistTracks "
        "INNER JOIN library ON library.id = PlaylistTracks.track_id "
        "WHERE PlaylistTracks.playlist_id = %3")
                              .arg(escaper.escapeString(playlistTableName),
                                      columns.join(","),
                                      QString::number(playlistId));
query.prepare(queryString);
if (!query.exec()) {
    LOG_FAILED_QUERY(query);
}

columns[0] = LIBRARYTABLE_ID;
// columns[1] = PLAYLISTTRACKSTABLE_POSITION from above
// columns[2] = PLAYLISTTRACKSTABLE_DATETIMEADDED from above
columns[3] = LIBRARYTABLE_PREVIEW;
columns[4] = LIBRARYTABLE_COVERART;
setTable(playlistTableName,
        LIBRARYTABLE_ID,
        columns,
        m_pTrackCollectionManager->internalCollection()->getTrackSource());

// Restore search text
setSearch(m_searchTexts.value(m_iPlaylistId));
setDefaultSort(fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_POSITION),
Qt::AscendingOrder); setSort(defaultSortColumn(), defaultSortOrder());
*/
}

void FindAllTableModel::orderTracksByCurrPos() {
    QList<std::pair<TrackId, int>> idPosList;
    int numOfTracks = rowCount();
    idPosList.reserve(numOfTracks);
    const int positionColumn = fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_POSITION);
    const int idColumn = fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ID);
    // Set up list of all IDs
    for (int i = 0; i < numOfTracks; i++) {
        TrackId trackId(index(i, idColumn).data());
        int oldPosition = index(i, positionColumn).data().toInt();
        idPosList.append(std::make_pair(trackId, oldPosition));
    }
}

const QList<int> FindAllTableModel::getSelectedPositions(const QModelIndexList& indices) const {
    if (indices.isEmpty()) {
        return {};
    }
    QList<int> positions;
    const int positionColumn = fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_POSITION);
    for (auto idx : indices) {
        int pos = idx.siblingAtColumn(positionColumn).data().toInt();
        positions.append(pos);
    }
    return positions;
}

mixxx::Duration FindAllTableModel::getTotalDuration(const QModelIndexList& indices) {
    if (indices.isEmpty()) {
        return mixxx::Duration::empty();
    }

    double durationTotal = 0.0;
    const int durationColumnIndex = fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_DURATION);
    for (const auto& index : indices) {
        durationTotal += index.sibling(index.row(), durationColumnIndex)
                                 .data(Qt::EditRole)
                                 .toDouble();
    }

    return mixxx::Duration::fromSeconds(durationTotal);
}

bool FindAllTableModel::isColumnInternal(int column) {
    return column == fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_TRACKID) ||
            TrackSetTableModel::isColumnInternal(column);
}

bool FindAllTableModel::isColumnHiddenByDefault(int column) {
    if (column == fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_DATETIMEADDED)) {
        return true;
    } else if (column == fieldIndex(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_POSITION)) {
        return false;
    }
    return BaseSqlTableModel::isColumnHiddenByDefault(column);
}

TrackModel::Capabilities FindAllTableModel::getCapabilities() const {
    TrackModel::Capabilities caps =
            Capability::Reorder |
            Capability::EditMetadata |
            Capability::LoadToDeck |
            Capability::LoadToSampler |
            Capability::LoadToPreviewDeck |
            Capability::ResetPlayed |
            Capability::RemoveFromDisk |
            Capability::Hide |
            Capability::Analyze |
            Capability::Properties;
    return caps;
}

QString FindAllTableModel::modelKey(bool noSearch) const {
    if (noSearch) {
        return m_tableName;
    }
    return m_tableName +
            QStringLiteral("#") +
            currentSearch();
}

#include "library/scanner/libraryscanner.h"

#include "library/coverartutils.h"
#include "library/queryutil.h"
#include "library/scanner/libraryscannerdlg.h"
#include "library/scanner/recursivescandirectorytask.h"
#include "library/scanner/scannertask.h"
#include "library/scanner/scannerutil.h"
#include "moc_libraryscanner.cpp"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/db/dbconnectionpooled.h"
#include "util/db/dbconnectionpooler.h"
#include "util/db/fwdsqlquery.h"
#include "util/logger.h"
#include "util/performancetimer.h"
#include "util/timer.h"
#include "util/trace.h"

namespace {

// TODO(rryan) make configurable
constexpr int kScannerThreadPoolSize = 1;

mixxx::Logger kLogger("LibraryScanner");

QAtomicInt s_instanceCounter(0);

// Returns the number of affected rows or -1 on error
int execRowCountQuery(FwdSqlQuery& query) {
    VERIFY_OR_DEBUG_ASSERT(query.isPrepared()) {
        return -1;
    }
    if (!query.execPrepared()) {
        return -1;
    }
    return query.numRowsAffected();
}

/// Clean up the database and fix inconsistencies from previous runs.
/// See also: https://github.com/mixxxdj/mixxx/issues/9771
void cleanUpDatabase(const QSqlDatabase& database) {
    kLogger.info()
            << "Cleaning up database...";
    PerformanceTimer timer;
    timer.start();
    // FIXME: The DELETE statement deletes more directory entries than necessary.
    // The subselect only covers directories that contain track files. Hashes
    // of parent directories that do not contain any track files will be deleted
    // and then re-created during the next rescan. This should not really matter
    // since the re-calculation of the hash is always required.
    const auto sqlStmt = QStringLiteral(
            "DELETE FROM LibraryHashes WHERE hash<>:unequalHash "
            "AND directory_path NOT IN "
            "(SELECT directory FROM track_locations)");
    FwdSqlQuery query(database, sqlStmt);
    query.bindValue(
            QStringLiteral(":unequalHash"),
            QVariant(mixxx::invalidCacheKey()));
    const auto numRows = execRowCountQuery(query);
    VERIFY_OR_DEBUG_ASSERT(numRows >= 0) {
        kLogger.warning()
                << "Failed to delete orphaned directory hashes";
    }
    else if (numRows > 0) {
        kLogger.info()
                << "Deleted" << numRows << "orphaned directory hashes";
    }
    kLogger.info()
            << "Finished database cleanup:"
            << timer.elapsed().debugMillisWithUnit();
}

/// Update statistics for the query planner
/// See also: https://www.sqlite.org/lang_analyze.html
void updateQueryPlannerStatisticsForDatabase(const QSqlDatabase& database) {
    kLogger.info()
            << "Updating query planner statistics for database...";
    PerformanceTimer timer;
    timer.start();
    const auto sqlStmt = QStringLiteral("ANALYZE");
    FwdSqlQuery query(database, sqlStmt);
    const auto numRows = execRowCountQuery(query);
    VERIFY_OR_DEBUG_ASSERT(numRows >= 0) {
        kLogger.warning()
                << "Failed to update query planner statistics for database";
    }
    else {
        kLogger.info()
                << "Finished updating query planner statistics for database:"
                << timer.elapsed().debugMillisWithUnit();
    }
}

} // anonymous namespace

LibraryScanner::LibraryScanner(
        mixxx::DbConnectionPoolPtr pDbConnectionPool,
        const UserSettingsPointer& pConfig)
        : m_pDbConnectionPool(std::move(pDbConnectionPool)),
          m_analysisDao(pConfig),
          m_trackDao(m_cueDao, m_playlistDao,
                  m_analysisDao, m_libraryHashDao,
                  pConfig),
          m_stateSema(1), // only one transaction is possible at a time
          m_state(IDLE) {
    // Move LibraryScanner to its own thread so that our signals/slots will
    // queue to our event loop.
    moveToThread(this);
    m_pool.moveToThread(this);

    const int instanceId = s_instanceCounter.fetchAndAddAcquire(1) + 1;
    setObjectName(QString("LibraryScanner %1").arg(instanceId));

    m_pool.setMaxThreadCount(kScannerThreadPoolSize);

    // Listen to signals from our public methods (invoked by other threads) and
    // connect them to our slots to run the command on the scanner thread.
    connect(this, &LibraryScanner::startScan, this, &LibraryScanner::slotStartScan);

    m_pProgressDlg.reset(new LibraryScannerDlg());
    connect(this,
            &LibraryScanner::progressLoading,
            m_pProgressDlg.data(),
            &LibraryScannerDlg::slotUpdate);
    connect(this,
            &LibraryScanner::progressHashing,
            m_pProgressDlg.data(),
            &LibraryScannerDlg::slotUpdate);
    connect(this,
            &LibraryScanner::scanStarted,
            m_pProgressDlg.data(),
            &LibraryScannerDlg::slotScanStarted);
    connect(this,
            &LibraryScanner::scanFinished,
            m_pProgressDlg.data(),
            &LibraryScannerDlg::slotScanFinished);
    connect(m_pProgressDlg.data(),
            &LibraryScannerDlg::scanCancelled,
            this,
            &LibraryScanner::slotCancel);
    connect(&m_trackDao,
            &TrackDAO::progressVerifyTracksOutside,
            m_pProgressDlg.data(),
            &LibraryScannerDlg::slotUpdate);
    connect(&m_trackDao,
            &TrackDAO::progressCoverArt,
            m_pProgressDlg.data(),
            &LibraryScannerDlg::slotUpdateCover);
}

LibraryScanner::~LibraryScanner() {
    cancelAndQuit();
}

void LibraryScanner::run() {
    kLogger.debug() << "Entering thread";
    {
        Trace trace("LibraryScanner");

        const mixxx::DbConnectionPooler dbConnectionPooler(m_pDbConnectionPool);
        QSqlDatabase dbConnection = mixxx::DbConnectionPooled(m_pDbConnectionPool);
        if (!dbConnection.isOpen()) {
            kLogger.warning()
                    << "Failed to open database connection for library scanner";
            kLogger.debug() << "Exiting thread";
            return;
        }

        m_libraryHashDao.initialize(dbConnection);
        m_cueDao.initialize(dbConnection);
        m_trackDao.initialize(dbConnection);
        m_playlistDao.initialize(dbConnection);
        m_analysisDao.initialize(dbConnection);
        m_directoryDao.initialize(dbConnection);

        // Start the event loop.
        kLogger.debug() << "Event loop starting";
        exec();
        kLogger.debug() << "Event loop stopped";
    }
    kLogger.debug() << "Exiting thread";
}

void LibraryScanner::slotStartScan() {
    kLogger.debug() << "slotStartScan()";
    DEBUG_ASSERT(m_state == STARTING);

    cleanUpDatabase(m_libraryHashDao.database());

    // Recursively scan each directory in the directories table.
    m_libraryRootDirs = m_directoryDao.loadAllDirectories();
    // If there are no directories then we have nothing to do. Cleanup and
    // finish the scan immediately.
    if (m_libraryRootDirs.isEmpty()) {
        changeScannerState(IDLE);
        return;
    }
    changeScannerState(SCANNING);

    QSet<QString> trackLocations = m_trackDao.getAllTrackLocations();
    QHash<QString, mixxx::cache_key_t> directoryHashes = m_libraryHashDao.getDirectoryHashes();
    QRegularExpression extensionFilter(SoundSourceProxy::getSupportedFileNamesRegex());
    QRegularExpression coverExtensionFilter =
            QRegularExpression(CoverArtUtils::supportedCoverArtExtensionsRegex(),
                    QRegularExpression::CaseInsensitiveOption);
    QStringList directoryBlacklist = ScannerUtil::getDirectoryBlacklist();

    m_scannerGlobal = ScannerGlobalPointer(
            new ScannerGlobal(trackLocations, directoryHashes, extensionFilter,
                              coverExtensionFilter, directoryBlacklist));

    m_scannerGlobal->startTimer();

    emit scanStarted();

    // First, we're going to mark all the directories that we've previously
    // hashed as needing verification. As we search through the directory tree
    // when we rescan, we'll mark any directory that does still exist as
    // verified.
    m_libraryHashDao.invalidateAllDirectories();

    // Make sure that `directory` in in track_locations table is indeed a
    // directory path. This works around / removes residues of a bug where tracks
    // are falsely marked missing because `directory` == `location`.
    m_trackDao.cleanupTrackLocationsDirectory();

    // Mark all the tracks in the library as needing verification of their
    // existence. (ie. we want to check they're still on your hard drive where
    // we think they are)
    m_trackDao.invalidateTrackLocationsInLibrary();

    kLogger.debug() << "Recursively scanning library.";

    // Start scanning the library. This prepares insertion queries in TrackDAO
    // (must be called before calling addTracksAdd) and begins a transaction.
    m_trackDao.addTracksPrepare();

    // First Scan all known directories we have a hash for.
    // In a second stage, we scan all new directories. This guarantees,
    // that we discover always the same folder, in case of duplicated folders
    // by symlinks

    // Queue up recursive scan tasks for every hashed directory. When all tasks
    // are done, TaskWatcher will signal slotFinishHashedScan.
    TaskWatcher* pWatcher = &m_scannerGlobal->getTaskWatcher();
    pWatcher->watchTask();
    connect(pWatcher,
            &TaskWatcher::allTasksDone,
            this,
            &LibraryScanner::slotFinishHashedScan);

    for (const mixxx::FileInfo& rootDir : std::as_const(m_libraryRootDirs)) {
        // Acquire a security bookmark for this directory if we are in a
        // sandbox. For speed we avoid opening security bookmarks when recursive
        // scanning so that relies on having an open bookmark for the containing
        // directory.
        if (!rootDir.exists() || !rootDir.isDir()) {
            qWarning() << "Skipping to scan" << rootDir;
            continue;
        }
        auto dirAccess = mixxx::FileAccess(rootDir);
        if (!m_scannerGlobal->testAndMarkDirectoryScanned(rootDir.toQDir())) {
            queueTask(new RecursiveScanDirectoryTask(
                    this, m_scannerGlobal, std::move(dirAccess), false));
        }
    }
    pWatcher->taskDone();
}

// is called when all tasks of the first stage are done (threads are finished)
void LibraryScanner::slotFinishHashedScan() {
    kLogger.debug() << "slotFinishHashedScan";
    VERIFY_OR_DEBUG_ASSERT(!m_scannerGlobal.isNull()) {
        kLogger.critical() << "No scanner global state exists in slotFinishHashedScan";
        return;
    }

    TaskWatcher* pWatcher = &m_scannerGlobal->getTaskWatcher();
    disconnect(pWatcher,
            &TaskWatcher::allTasksDone,
            this,
            &LibraryScanner::slotFinishHashedScan);

    if (m_scannerGlobal->unhashedDirs().empty()) {
        // bypass the second stage
        slotFinishUnhashedScan();
        return;
    }

    // Queue up recursive scan tasks for every unhashed directory, discovered
    // in the first stage. When all tasks
    // are done, TaskWatcher will signal slotFinishUnhashedScan.
    pWatcher->watchTask();
    connect(pWatcher,
            &TaskWatcher::allTasksDone,
            this,
            &LibraryScanner::slotFinishUnhashedScan);

    for (mixxx::FileAccess dirAccess : m_scannerGlobal->unhashedDirs()) {
        // no testAndMarkDirectoryScanned() here, because all unhashedDirs()
        // are already tracked
        queueTask(new RecursiveScanDirectoryTask(
                this, m_scannerGlobal, std::move(dirAccess), true));
    }
    pWatcher->taskDone();
}

void LibraryScanner::cleanUpScan() {
    // At the end of a scan, mark all tracks and directories that weren't
    // "verified" as "deleted" (as long as the scan wasn't canceled half way
    // through). This condition is important because our rescanning algorithm
    // starts by marking all tracks and dirs as unverified, so a canceled scan
    // might leave half of your library as unverified. Don't want to mark those
    // tracks/dirs as deleted in that case) :)

    // Start a transaction for all the library hashing (moved file
    // detection) stuff.
    QSqlDatabase dbConnection = mixxx::DbConnectionPooled(m_pDbConnectionPool);
    ScopedTransaction transaction(dbConnection);

    kLogger.debug() << "Marking tracks in changed directories as verified";
    m_trackDao.markTrackLocationsAsVerified(m_scannerGlobal->verifiedTracks());

    kLogger.debug() << "Marking unchanged directories and tracks as verified";
    m_libraryHashDao.updateDirectoryStatuses(
            m_scannerGlobal->verifiedDirectories(),
            false,
            true);
    m_trackDao.markTracksInDirectoriesAsVerified(
            m_scannerGlobal->verifiedDirectories());

    // After verifying tracks and directories via recursive scanning of the
    // library directories the only unverified tracks will be files that are
    // outside of the library directories, files that have been
    // moved/deleted/renamed and are in duplicate directories by symlinks or
    // non normalized paths.
    kLogger.debug() << "Checking remaining unverified tracks";
    if (!m_trackDao.verifyRemainingTracks(
            m_libraryRootDirs,
            m_scannerGlobal->shouldCancelPointer())) {
        // canceled
        return;
    }

    kLogger.debug() << "Marking unverified tracks as deleted";
    m_trackDao.markUnverifiedTracksAsDeleted();

    kLogger.debug() << "Marking unverified directories as deleted";
    m_libraryHashDao.markUnverifiedDirectoriesAsDeleted();

    // Check to see if the "deleted" tracks showed up in another location,
    // and if so, do some magic to update all our tables.
    kLogger.debug() << "Detecting moved files";
    {
        QList<RelocatedTrack> relocatedTracks;
        if (!m_trackDao.detectMovedTracks(
                &relocatedTracks,
                m_scannerGlobal->addedTracks(),
                m_scannerGlobal->shouldCancelPointer())) {
            kLogger.info()
                    << "Detecting moved files has been canceled or aborted";
            return;
        }
        if (!relocatedTracks.isEmpty()) {
            kLogger.info()
                    << "Found"
                    << relocatedTracks.size()
                    << "moved track(s)";
            emit tracksRelocated(relocatedTracks);
        }
    }

    // Remove the hashes for any directories that have been marked as
    // deleted to clean up. We need to do this otherwise we can skip over
    // songs if you move a set of songs from directory A to B, then back to
    // A.
    m_libraryHashDao.removeDeletedDirectoryHashes();

    transaction.commit();

    kLogger.debug() << "Detecting cover art for unscanned files";
    QSet<TrackId> coverArtTracksChanged;
    m_trackDao.detectCoverArtForTracksWithoutCover(
            m_scannerGlobal->shouldCancelPointer(), &coverArtTracksChanged);

    // Update BaseTrackCache via signals connected to the main TrackDAO.
    if (!coverArtTracksChanged.isEmpty()) {
        emit tracksChanged(coverArtTracksChanged);
    }
}


// is called when all tasks of the second stage are done (threads are finished)
void LibraryScanner::slotFinishUnhashedScan() {
    kLogger.debug() << "slotFinishUnhashedScan";
    VERIFY_OR_DEBUG_ASSERT(!m_scannerGlobal.isNull()) {
        kLogger.critical() << "No scanner global state exists in slotFinishUnhashedScan";
        return;
    }

    bool bScanFinishedCleanly = m_scannerGlobal->scanFinishedCleanly();

    if (bScanFinishedCleanly) {
        kLogger.debug() << "Recursive scanning finished cleanly";
    } else {
        kLogger.debug() << "Recursive scanning interrupted by the user";
    }

    // Finish adding the tracks -- rollback the transaction if the scan did not
    // finish cleanly and the user did not cancel the transaction.
    m_trackDao.addTracksFinish(!m_scannerGlobal->shouldCancel() &&
                               !bScanFinishedCleanly);

    if (!m_scannerGlobal->shouldCancel() && bScanFinishedCleanly) {
        cleanUpScan();
    }

    if (!m_scannerGlobal->shouldCancel() && bScanFinishedCleanly) {
        const auto dbConnection = mixxx::DbConnectionPooled(m_pDbConnectionPool);
        updateQueryPlannerStatisticsForDatabase(dbConnection);
    }

    if (!m_scannerGlobal->shouldCancel() && bScanFinishedCleanly) {
        kLogger.debug() << "Scan finished cleanly";
    } else {
        kLogger.debug() << "Scan cancelled";
    }

    // TODO(XXX) doesn't take into account verifyRemainingTracks.
    qDebug("Scan took: %s. "
           "%d unchanged directories. "
           "%d changed/added directories. "
           "%d tracks verified from changed/added directories. "
           "%d new tracks.",
            m_scannerGlobal->timerElapsed().formatNanosWithUnit().toLocal8Bit().constData(),
            static_cast<int>(m_scannerGlobal->verifiedDirectories().size()),
            m_scannerGlobal->numScannedDirectories(),
            static_cast<int>(m_scannerGlobal->verifiedTracks().size()),
            static_cast<int>(m_scannerGlobal->addedTracks().size()));

    m_scannerGlobal.clear();
    changeScannerState(FINISHED);
    // now we may accept new scan commands

    emit scanFinished();
}

void LibraryScanner::scan() {
    if (changeScannerState(STARTING)) {
        emit startScan();
    }
}

// this is called after pressing the cancel button in the scanner
// progress dialog
void LibraryScanner::slotCancel() {
    // Wait until there is no scan starting.
    // All pending scan start request are canceled
    // as well until the scanner is idle again.
    changeScannerState(CANCELING);
    cancel();
    changeScannerState(IDLE);
}

void LibraryScanner::cancelAndQuit() {
    changeScannerState(CANCELING);
    cancel();
    // Quit the event loop gracefully and stay in CANCELING state until all
    // pending signals are processed
    quit();
    wait();
    changeScannerState(IDLE);
}

// be sure we hold the m_stateSema and we are in CANCELING state
void LibraryScanner::cancel() {
    DEBUG_ASSERT(m_state == CANCELING);


    // we need to make a local copy because cancel is called
    // from any thread but m_scannerGlobal may be cleared
    // in the LibraryScanner thread in the meanwhile
    ScannerGlobalPointer scanner = m_scannerGlobal;
    if (scanner) {
        scanner->cancel();
    }

    // Wait for the thread pool to empty. This is important because ScannerTasks
    // have pointers to the LibraryScanner and can cause a segfault if they run
    // after the LibraryScanner has been destroyed.
    m_pool.waitForDone();
}

void LibraryScanner::queueTask(ScannerTask* pTask) {
    //kLogger.debug() << "queueTask" << pTask;
    ScopedTimer timer(u"LibraryScanner::queueTask");
    if (m_scannerGlobal.isNull() || m_scannerGlobal->shouldCancel()) {
        return;
    }
    m_scannerGlobal->getTaskWatcher().watchTask();
    connect(pTask,
            &ScannerTask::queueTask,
            this,
            &LibraryScanner::queueTask);
    connect(pTask,
            &ScannerTask::directoryHashedAndScanned,
            this,
            &LibraryScanner::slotDirectoryHashedAndScanned);
    connect(pTask,
            &ScannerTask::directoryUnchanged,
            this,
            &LibraryScanner::slotDirectoryUnchanged);
    connect(pTask,
            &ScannerTask::trackExists,
            this,
            &LibraryScanner::slotTrackExists);
    connect(pTask,
            &ScannerTask::addNewTrack,
            this,
            &LibraryScanner::slotAddNewTrack);

    // Progress signals.
    // Pass directly to the main thread
    connect(pTask,
            &ScannerTask::progressLoading,
            this,
            &LibraryScanner::progressLoading);
    connect(pTask,
            &ScannerTask::progressHashing,
            this,
            &LibraryScanner::progressHashing);

    m_pool.start(pTask);
}

void LibraryScanner::slotDirectoryHashedAndScanned(const QString& directoryPath,
                                               bool newDirectory, mixxx::cache_key_t hash) {
    ScopedTimer timer(u"LibraryScanner::slotDirectoryHashedAndScanned");
    //kLogger.debug() << "sloDirectoryHashedAndScanned" << directoryPath
    //          << newDirectory << hash;

    // For statistics tracking -- if we hashed a directory then we scanned it
    // (it was changed or new).
    if (m_scannerGlobal) {
        m_scannerGlobal->directoryScanned();
    }

    if (newDirectory) {
        m_libraryHashDao.saveDirectoryHash(directoryPath, hash);
    } else {
        m_libraryHashDao.updateDirectoryHash(directoryPath, hash, 0);
    }
    emit progressHashing(directoryPath);
}

void LibraryScanner::slotDirectoryUnchanged(const QString& directoryPath) {
    ScopedTimer timer(u"LibraryScanner::slotDirectoryUnchanged");
    //kLogger.debug() << "slotDirectoryUnchanged" << directoryPath;
    if (m_scannerGlobal) {
        m_scannerGlobal->addVerifiedDirectory(directoryPath);
    }
    emit progressHashing(directoryPath);
}

void LibraryScanner::slotTrackExists(const QString& trackPath) {
    //kLogger.debug() << "slotTrackExists" << trackPath;
    ScopedTimer timer(u"LibraryScanner::slotTrackExists");
    if (m_scannerGlobal) {
        m_scannerGlobal->addVerifiedTrack(trackPath);
    }
}

void LibraryScanner::slotAddNewTrack(const QString& trackPath) {
    // kLogger.debug() << "slotAddNewTrack" << trackPath;
    ScopedTimer timer(u"LibraryScanner::addNewTrack");
    // For statistics tracking and to detect moved tracks
    TrackPointer pTrack = m_trackDao.addTracksAddFile(
            trackPath,
            false);
    if (!pTrack) {
        // This happens only when there is an issue with the database which
        // has been logged already. No need for yet another warning here.
        return;
    }

    DEBUG_ASSERT(!pTrack->isDirty());
    // The track's actual location might differ from the
    // given trackPath
    const QString trackLocation = pTrack->getLocation();
    // Acknowledge successful track addition
    if (m_scannerGlobal) {
        m_scannerGlobal->trackAdded(trackLocation);
    }
    // Signal the main instance of TrackDAO, that there is
    // a new track in the database.
    emit trackAdded(pTrack);
    emit progressLoading(trackLocation);
}

bool LibraryScanner::changeScannerState(ScannerState newState) {
    switch (newState) {
    case IDLE:
        // we are leaving STARTING or CANCELING state
        // m_state is already IDLE if a scan was canceled
        m_state = IDLE;
        m_stateSema.release();
        return true;
    case STARTING:
        // we need to hold the m_stateSema during the STARTING state
        // to prevent losing cancel commands or start the scanner
        // twice
        if (m_stateSema.tryAcquire()) {
            if (m_state != IDLE) {
                kLogger.debug() << "Scan already in progress";
                m_stateSema.release();
                return false;
            }
            m_state = STARTING;
            return true;
        } else {
            kLogger.debug() << "can't acquire semaphore, state =" << m_state;
            return false;
        }
    case SCANNING:
        DEBUG_ASSERT(m_state == STARTING);
        // Transition protected by the semaphore is over now
        // Allow canceling
        m_state = SCANNING;
        m_stateSema.release();
        return true;
    case CANCELING:
        DEBUG_ASSERT(m_state != CANCELING);
        // canceling is always possible, but wait
        // until there is no scan starting.
        // It must be unlocked by changeScannerState(IDLE);
        m_stateSema.acquire();
        m_state = CANCELING;
        return true;
    case FINISHED:
        // we must not acquire the semaphore here, because
        // it is already acquired in case we
        // are canceling.
        // There is no race condition, since the state
        // is set to IDLE after canceling as well
        m_state = IDLE;
        return true;
    default:
        DEBUG_ASSERT(false);
        return false;
    }
}

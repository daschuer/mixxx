#include "library/analysis/analysislibrarytablemodel.h"

#include "moc_analysislibrarytablemodel.cpp"

namespace {

const QString RECENT_FILTER = QStringLiteral("datetime_added > datetime('now', '-7 days')");

} // anonymous namespace

AnalysisLibraryTableModel::AnalysisLibraryTableModel(QObject* parent,
                                                   TrackCollectionManager* pTrackCollectionManager)
        : LibraryTableModel(parent, pTrackCollectionManager,
                            "mixxx.db.model.prepare") {
    // Default to showing recent tracks.
    setExtraFilter(RECENT_FILTER);
}

void AnalysisLibraryTableModel::showRecentSongs() {
    // Search with the recent filter.
    setExtraFilter(RECENT_FILTER);
    select();
}

void AnalysisLibraryTableModel::showAllSongs() {
    // Clear the recent filter.
    setExtraFilter({});
    select();
}

void AnalysisLibraryTableModel::searchCurrentTrackSet(const QString& text, bool useRecentFilter) {
    setExtraFilter(useRecentFilter ? RECENT_FILTER : QString{});
    search(text);
}

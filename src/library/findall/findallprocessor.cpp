#include "library/findall/findallprocessor.h"

#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "engine/channels/enginedeck.h"
#include "library/findall/findalltablemodel.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playermanager.h"
#include "moc_findallprocessor.cpp"
#include "track/track.h"
#include "util/math.h"

#define kConfigKey "[Auto DJ]"
namespace {

constexpr bool sDebug = false;

} // anonymous namespace

FindAllProcessor::FindAllProcessor(
        QObject* pParent,
        UserSettingsPointer pConfig,
        TrackCollectionManager* pTrackCollectionManager)
        : QObject(pParent),
          m_pConfig(pConfig),
          m_pFindAllTableModel(std::make_unique<FindAllTableModel>(
                  this, pTrackCollectionManager, "mixxx.db.model.findall")) {
    m_pFindAllTableModel->setTableModel();
    m_pFindAllTableModel->select();
}

FindAllProcessor::~FindAllProcessor() {
}

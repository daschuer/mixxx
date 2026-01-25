#pragma once

#include <QObject>
#include <QString>

#include "audio/frame.h"
#include "control/controlproxy.h"
#include "engine/channels/enginechannel.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"
#include "util/class.h"

class ControlPushButton;
class TrackCollectionManager;
class PlayerManagerInterface;
class BaseTrackPlayer;
class FindAllTableModel;
typedef QList<QModelIndex> QModelIndexList;

class FindAllProcessor : public QObject {
    Q_OBJECT
  public:
    FindAllProcessor(QObject* pParent,
            UserSettingsPointer pConfig,
            TrackCollectionManager* pTrackCollectionManager);
    virtual ~FindAllProcessor();


    FindAllTableModel* getTableModel() const {
        return m_pFindAllTableModel.get();
    }



  signals:

  private slots:

  protected:
  private:
    UserSettingsPointer m_pConfig;
    std::unique_ptr<FindAllTableModel> m_pFindAllTableModel;

    DISALLOW_COPY_AND_ASSIGN(FindAllProcessor);
};

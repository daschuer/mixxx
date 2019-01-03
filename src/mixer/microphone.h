#pragma once

#include <QObject>
#include <QString>

#include "control/controlproxylt.h"
#include "mixer/baseplayer.h"
#include "util/parented_ptr.h"

class ControlProxy;
class EffectsManager;
class EngineMaster;
class SoundManager;

class Microphone : public BasePlayer {
    Q_OBJECT
  public:
    Microphone(PlayerManager* pParent,
            const QString& group,
            int index,
            SoundManager* pSoundManager,
            EngineMaster* pMixingEngine,
            EffectsManager* pEffectsManager);
    ~Microphone() override;

  signals:
    void noMicrophoneInputConfigured();

  private slots:
    void slotTalkoverEnabled(double v);

  private:
    ControlProxyLt m_inputConfigured;
    parented_ptr<ControlProxy> m_pTalkoverEnabled;
};

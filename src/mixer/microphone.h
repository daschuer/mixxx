#ifndef MIXER_MICROPHONE_H
#define MIXER_MICROPHONE_H

#include <QObject>
#include <QScopedPointer>
#include <QString>

#include "mixer/baseplayer.h"
#include "util/parented_ptr.h"
#include "control/controlproxylt.h"

class ControlProxy;
class EffectsManager;
class EngineMaster;
class SoundManager;

class Microphone : public BasePlayer {
    Q_OBJECT
  public:
    Microphone(QObject* pParent,
               const QString& group,
               int index,
               SoundManager* pSoundManager,
               EngineMaster* pMixingEngine,
               EffectsManager* pEffectsManager);
    virtual ~Microphone();

  signals:
    void noMicrophoneInputConfigured();

  private slots:
    void slotTalkoverEnabled(double v);

  private:
    ControlProxyLt m_inputConfigured;
    parented_ptr<ControlProxy> m_pTalkoverEnabled;
};

#endif /* MIXER_MICROPHONE_H */

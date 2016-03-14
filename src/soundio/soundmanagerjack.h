
#ifndef SOUNDMANAGERJACK_H
#define SOUNDMANAGERJACK_H

#include <QList>

#include "preferences/usersettings.h"

class SoundDevice;
class SoundManager;

class SoundManagerJack {
  public:
    SoundManagerJack(UserSettingsPointer pConfig);
    virtual ~SoundManagerJack();

    void queryDevices(QList<SoundDevice*>* pDevices, SoundManager* pSM);

    bool isSampleRateDefinedByApi(QString api,
            QList<unsigned int>* pSamplerates) const;

    // Get a list of host APIs supported by PortAudio.
    void appendHostAPIList(QList<QString>* pApiList) const;

    void clearDeviceList();

  private:
    void setJACKName() const;

    UserSettingsPointer m_pConfig;
    unsigned int m_jackSampleRate;
};

#endif // SOUNDMANAGERJACK_H

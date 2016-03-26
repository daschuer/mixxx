
#ifndef SOUNDMANAGERPORTAUDIO_H
#define SOUNDMANAGERPORTAUDIO_H

#include <QList>

#include "preferences/usersettings.h"

class SoundDevice;
class SoundManager;

class SoundManagerPortAudio {
  public:
    SoundManagerPortAudio(UserSettingsPointer pConfig);
    virtual ~SoundManagerPortAudio();

    void queryDevices(QList<SoundDevice*>* pDevices, SoundManager* pSM);

    // Get a list of host APIs supported by PortAudio.
    void appendHostAPIList(QList<QString>* pApiList) const;

    void clearDeviceList();

  private:

    UserSettingsPointer m_pConfig;
    bool m_paInitialized;
};

#endif // SOUNDMANAGERPORTAUDIO_H

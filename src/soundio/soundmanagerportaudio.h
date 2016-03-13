
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

    bool isSampleRateDefinedByApi(QString api,
            QList<unsigned int>* pSamplerates) const;

    // Get a list of host APIs supported by PortAudio.
    QList<QString> getHostAPIList() const;

    void clearDeviceList();

  private:
    void setJACKName() const;

    UserSettingsPointer m_pConfig;
    bool m_paInitialized;
    unsigned int m_jackSampleRate;
};

#endif // SOUNDMANAGERPORTAUDIO_H

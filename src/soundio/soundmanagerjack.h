
#ifndef SOUNDMANAGERJACK_H
#define SOUNDMANAGERJACK_H

#include <QList>

#include <jack/types.h>
#include <jack/jack.h>

#include "util/fifo.h"
#include "preferences/usersettings.h"
#include "soundio/sounddevicejack.h"

class SoundDevice;
class SoundManager;
class AudioOutput;
class AudioInput;

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

    void registerOutput(const AudioOutput& output);
    void registerInput(const AudioInput& input);

  private:
    void setJACKName() const;
    void jackInitialize();
    void buildDeviceList();

    UserSettingsPointer m_pConfig;
    unsigned int m_jackSampleRate;

    jack_client_t* m_pJackClient;

    QHash<QString, JackDeviceInfo> m_devices;
};

#endif // SOUNDMANAGERJACK_H

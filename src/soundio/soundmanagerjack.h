
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
class AudioSource;
class AudioDestination;


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

    void registerOutput(const AudioOutput& output, AudioSource* src);
    void registerInput(const AudioInput& input, AudioDestination* dest);

    void connectOutputPorts(
            QString name,
            QList<QString> inputPorts,
            const AudioOutputBuffer& output,
            bool connect);
    void connectInputPorts(
            QString name,
            QList<QString> outputPorts,
            const AudioInputBuffer& input,
            bool connect);

    void onShutdown();
    int sampleRateCallback(jack_nframes_t nframes);
    int xRunCallback();
    int processCallback(jack_nframes_t nframes);


  private:
    void setJACKName() const;
    void jackInitialize();
    void buildDeviceList();

    UserSettingsPointer m_pConfig;
    unsigned int m_jackSampleRate;
    jack_nframes_t m_jackBufferSize;
    bool m_activated;

    jack_client_t* m_pJackClient;

    QHash<QString, JackDeviceInfo> m_devices;
    QHash<AudioOutput, jack_port_t*> m_registeredOutputPorts;
    QHash<AudioInput, jack_port_t*> m_registeredInputPorts;
};

#endif // SOUNDMANAGERJACK_H


#ifndef SOUNDMANAGERJACK_H
#define SOUNDMANAGERJACK_H

#include <QList>

#include <jack/types.h>
#include <jack/jack.h>

#include "util/fifo.h"
#include "preferences/usersettings.h"
#include "soundio/sounddevicejack.h"
#include "soundio/soundmanagerutil.h"

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

    void registerOutput(const AudioOutput& output, AudioSource* pSrc);
    void registerInput(const AudioInput& input, AudioDestination* pDest);

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
    void portConnectCallback(jack_port_id_t a,
                             jack_port_id_t b,
                             int connect);
    int sampleRateCallback(jack_nframes_t nframes);
    int xRunCallback();
    int processCallback(jack_nframes_t nframes);

    void portConnect(jack_port_id_t portId, int connect);


  private:
    struct InputPort {
        AudioInput audioInput;
        AudioDestination *pDest;
        jack_port_t* pJackPort;
    };

    struct OutputPort {
        AudioOutput audioOutput;
        AudioSource *pSrc;
        jack_port_t* pJackPort;
    };

    void setJACKName() const;
    void jackInitialize();
    void buildDeviceList();

    UserSettingsPointer m_pConfig;
    unsigned int m_jackSampleRate;
    jack_nframes_t m_jackBufferSize;
    bool m_activated;

    jack_client_t* m_pJackClient;

    QHash<QString, JackDeviceInfo> m_devices;
    QHash<QString, OutputPort> m_registeredOutputPorts;
    QHash<QString, InputPort> m_registeredInputPorts;

    QList<OutputPort> m_connectedOutputPorts;
    QList<InputPort> m_connectedInputPorts;

    QMutex m_processMutex;
};

#endif // SOUNDMANAGERJACK_H

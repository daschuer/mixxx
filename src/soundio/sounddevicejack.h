#ifndef SOUNDDEVICEJACK_H
#define SOUNDDEVICEJACK_H

#include <portaudio.h>
#include <jack/types.h>
#include <jack/jack.h>

#include <QString>
#include "util/performancetimer.h"
#include "util/fifo.h"

#include "soundio/sounddevice.h"
//#include "soundio/soundmanagerjack.h"
#include "util/duration.h"


#define CPU_USAGE_UPDATE_RATE 30 // in 1/s, fits to display frame rate
#define CPU_OVERLOAD_DURATION 500 // in ms

class SoundManager;
class SoundManagerJack;
class ControlObjectSlave;

/** Dynamically resolved function which allows us to enable a realtime-priority callback
    thread from ALSA/PortAudio. This must be dynamically resolved because PortAudio can't
    tell us if ALSA is compiled into it or not. */
typedef int (*EnableAlsaRT)(PaStream* s, int enable);


struct JackDeviceInfo {
    QString name;
    QList<QString> inputPorts;
    QList<QString> outputPorts;
    unsigned int sampleRate;
    jack_client_t* pJackClient;
};


class SoundDeviceJack : public SoundDevice {
  public:
    SoundDeviceJack(UserSettingsPointer config,
                    SoundManager *sm,
                    SoundManagerJack *smj,
                    const JackDeviceInfo& deviceInfo);
    ~SoundDeviceJack() override;

    Result open(bool isClkRefDevice, int syncBuffers) override;
    bool isOpen() const override;
    Result close() override;
    void readProcess() override;
    void writeProcess() override;
    SoundDeviceError addOutput(const AudioOutputBuffer& out) override;

    // This callback function gets called everytime the sound device runs out of
    // samples (ie. when it needs more sound to play)
    int callbackProcess(const unsigned int framesPerBuffer,
                        CSAMPLE *output, const CSAMPLE* in,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags);
    // Same as above but with drift correction
    int callbackProcessDrift(const unsigned int framesPerBuffer,
                        CSAMPLE *output, const CSAMPLE* in,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags);
    // The same as above but drives the MixxEngine
    int callbackProcessClkRef(const unsigned int framesPerBuffer,
                        CSAMPLE *output, const CSAMPLE* in,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags);

    unsigned int getDefaultSampleRate() const override {
        return m_deviceInfo.sampleRate;
    };

  private:
    void updateCallbackEntryToDacTime(const PaStreamCallbackTimeInfo* timeInfo);

    SoundManagerJack* m_pSoundManagerJack;
    JackDeviceInfo m_deviceInfo;
    bool m_isOpen;
};

#endif // SOUNDDEVICEJACK_H

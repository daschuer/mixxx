#ifndef SOUNDDEVICEJACK_H
#define SOUNDDEVICEJACK_H

#include <portaudio.h>
#include <jack/types.h>
#include <jack/jack.h>

#include <QString>
#include "util/performancetimer.h"
#include "util/fifo.h"

#include "soundio/sounddevice.h"
#include "util/duration.h"


#define CPU_USAGE_UPDATE_RATE 30 // in 1/s, fits to display frame rate
#define CPU_OVERLOAD_DURATION 500 // in ms

class SoundManager;
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
                    SoundManager *sm, const JackDeviceInfo& deviceInfo);
    virtual ~SoundDeviceJack();

    virtual Result open(bool isClkRefDevice, int syncBuffers);
    virtual bool isOpen() const;
    virtual Result close();
    virtual void readProcess();
    virtual void writeProcess();
    virtual QString getError() const;

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

    virtual unsigned int getDefaultSampleRate() const {
        return m_deviceInfo.sampleRate;
    }

  private:
    void updateCallbackEntryToDacTime(const PaStreamCallbackTimeInfo* timeInfo);

    // PortAudio stream for this device.
    PaStream* volatile m_pStream;
    // Struct containing information about this device. Don't free() it, it
    // belongs to PortAudio.
    JackDeviceInfo m_deviceInfo;
    // Description of the output stream going to the soundcard.
    PaStreamParameters m_outputParams;
    // Description of the input stream coming from the soundcard.
    PaStreamParameters m_inputParams;
    FIFO<CSAMPLE>* m_outputFifo;
    FIFO<CSAMPLE>* m_inputFifo;
    bool m_outputDrift;
    bool m_inputDrift;

    // A string describing the last PortAudio error to occur.
    QString m_lastError;
    // Whether we have set the thread priority to realtime or not.
    bool m_bSetThreadPriority;
    ControlObjectSlave* m_pMasterAudioLatencyOverloadCount;
    ControlObjectSlave* m_pMasterAudioLatencyUsage;
    ControlObjectSlave* m_pMasterAudioLatencyOverload;
    int m_underflowUpdateCount;
    static volatile int m_underflowHappened;
    mixxx::Duration m_timeInAudioCallback;
    int m_framesSinceAudioLatencyUsageUpdate;
    int m_syncBuffers;
    bool m_invalidTimeInfoWarned;
    PerformanceTimer m_clkRefTimer;
};

#endif // SOUNDDEVICEJACK_H

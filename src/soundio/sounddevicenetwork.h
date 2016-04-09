#ifndef SOUNDDEVICENETWORK_H
#define SOUNDDEVICENETWORK_H

#include <QString>

#include "soundio/sounddevice.h"
#include "util/fifo.h"


#define CPU_USAGE_UPDATE_RATE 30 // in 1/s, fits to display frame rate
#define CPU_OVERLOAD_DURATION 500 // in ms

class SoundManager;
class EngineNetworkStream;

class SoundDeviceNetwork : public SoundDevice {
  public:
    SoundDeviceNetwork(UserSettingsPointer config,
                       SoundManager *sm,
                       QSharedPointer<EngineNetworkStream> pNetworkStream);
    ~SoundDeviceNetwork() override;

    Result open(bool isClkRefDevice, int syncBuffers) override;
    bool isOpen() const override;
    Result close() override;
    void readProcess() override;
    void writeProcess() override;

    virtual unsigned int getDefaultSampleRate() const {
        return 44100;
    }

  private:
    QSharedPointer<EngineNetworkStream> m_pNetworkStream;
    FIFO<CSAMPLE>* m_outputFifo;
    FIFO<CSAMPLE>* m_inputFifo;
    bool m_outputDrift;
    bool m_inputDrift;
    static volatile int m_underflowHappened;
};

#endif // SOUNDDEVICENETWORK_H

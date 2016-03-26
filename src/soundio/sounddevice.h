
#ifndef SOUNDDEVICE_H
#define SOUNDDEVICE_H

#include <QString>
#include <QList>

#include "util/types.h"
#include "util/result.h"
#include "preferences/usersettings.h"

#define MIXXX_PORTAUDIO_JACK_STRING "JACK Audio Connection Kit"
#define MIXXX_PORTAUDIO_ALSA_STRING "ALSA"
#define MIXXX_PORTAUDIO_OSS_STRING "OSS"
#define MIXXX_PORTAUDIO_ASIO_STRING "ASIO"
#define MIXXX_PORTAUDIO_DIRECTSOUND_STRING "Windows DirectSound"
#define MIXXX_PORTAUDIO_COREAUDIO_STRING "Core Audio"
#define MIXXX_PORTAUDIO_SKELETON_STRING "skeleton implementation"

//Forward declarations
class SoundManager;
class AudioOutput;
class AudioInput;
class AudioOutputBuffer;
class AudioInputBuffer;

enum SoundDeviceError {
    SOUNDDEVICE_ERROR_OK = OK,
    SOUNDDEVICE_ERROR_DUPLICATE_OUTPUT_CHANNEL,
    SOUNDDEVICE_ERROR_EXCESSIVE_OUTPUT_CHANNEL,
    SOUNDDEVICE_ERROR_EXCESSIVE_INPUT_CHANNEL,
};

const QString kNetworkDeviceInternalName = "Network stream";

class SoundDevice {
  public:
    SoundDevice(UserSettingsPointer config, SoundManager* sm);
    virtual ~SoundDevice();

    inline const QString& getInternalName() const {
        return m_strInternalName;
    }
    inline const QString& getDisplayName() const {
        return m_strDisplayName;
    }
    inline const QString& getHostAPI() const {
        return m_hostAPI;
    }
    void setSampleRate(double sampleRate);
    void setFramesPerBuffer(unsigned int framesPerBuffer);
    virtual Result open(bool isClkRefDevice, int syncBuffers) = 0;
    virtual bool isOpen() const = 0;
    virtual Result close() = 0;
    virtual void readProcess() = 0;
    virtual void writeProcess() = 0;
    virtual QString getError() const = 0;
    virtual unsigned int getDefaultSampleRate() const = 0;
    int getNumOutputChannels() const;
    int getNumInputChannels() const;
    SoundDeviceError addOutput(const AudioOutputBuffer& out);
    SoundDeviceError addInput(const AudioInputBuffer& in);
    const QList<AudioInputBuffer>& inputs() const {
        return m_audioInputs;
    }
    const QList<AudioOutputBuffer>& outputs() const {
        return m_audioOutputs;
    }

    void clearOutputs();
    void clearInputs();
    bool operator==(const SoundDevice &other) const;
    bool operator==(const QString &other) const;

  protected:
    void composeOutputBuffer(CSAMPLE* outputBuffer,
                             const unsigned int iFramesPerBuffer,
                             const unsigned int readOffset,
                             const unsigned int iFrameSize);

    void composeInputBuffer(const CSAMPLE* inputBuffer,
                            const unsigned int framesToPush,
                            const unsigned int framesWriteOffset,
                            const unsigned int iFrameSize);

    void clearInputBuffer(const unsigned int framesToPush,
                          const unsigned int framesWriteOffset);

    UserSettingsPointer m_pConfig;
    // Pointer to the SoundManager object which we'll request audio from.
    SoundManager* m_pSoundManager;
    // The name of the soundcard, used internally (may include the device ID)
    QString m_strInternalName;
    // The name of the soundcard, as displayed to the user
    QString m_strDisplayName;
    // The number of output channels that the soundcard has
    int m_iNumOutputChannels;
    // The number of input channels that the soundcard has
    int m_iNumInputChannels;
    // The current samplerate for the sound device.
    double m_dSampleRate;
    // The name of the audio API used by this device.
    QString m_hostAPI;
    unsigned int m_framesPerBuffer;
    QList<AudioOutputBuffer> m_audioOutputs;
    QList<AudioInputBuffer> m_audioInputs;
};

#endif // SOUNDDEVICE_H

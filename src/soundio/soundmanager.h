
#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>

#include "preferences/usersettings.h"
#include "engine/sidechain/enginenetworkstream.h"
#include "soundio/soundmanagerconfig.h"
#include "util/result.h"
#include "util/types.h"
#include "soundio/soundmanagerportaudio.h"
#include "soundio/soundmanagerjack.h"

class SoundDevice;
class EngineMaster;
class AudioOutput;
class AudioInput;
class AudioSource;
class AudioDestination;
class ControlObject;

#define SOUNDMANAGER_DISCONNECTED 0
#define SOUNDMANAGER_CONNECTING 1
#define SOUNDMANAGER_CONNECTED 2

class SoundManager : public QObject {
    Q_OBJECT
  public:
    SoundManager(UserSettingsPointer pConfig, EngineMaster *_master);
    virtual ~SoundManager();

    // Returns a list of all devices we've enumerated that match the provided
    // filterApi, and have at least one output or input channel if the
    // bOutputDevices or bInputDevices are set, respectively.
    QList<SoundDevice*> getDeviceList(QString filterAPI, bool bOutputDevices, bool bInputDevices);

    // Creates a list of sound devices
    void clearAndQueryDevices();
    void queryDevices();
    void queryDevicesMixxx();

    // Opens all the devices chosen by the user in the preferences dialog, and
    // establishes the proper connections between them and the mixing engine.
    Result setupDevices();

    // Playermanager will notify us when the number of decks changes.
    void setConfiguredDeckCount(int count);
    int getConfiguredDeckCount() const;

    SoundDevice* getErrorDevice() const;

    // Returns a list of samplerates we will attempt to support for a given API.
    QList<unsigned int> getSampleRates(QString api) const;

    // Convenience overload for SoundManager::getSampleRates(QString)
    QList<unsigned int> getSampleRates() const;

    // Get a list of host APIs supported by PortAudio.
    QList<QString> getHostAPIList() const;
    SoundManagerConfig getConfig() const;
    Result setConfig(SoundManagerConfig config);
    void checkConfig();

    void onDeviceOutputCallback(const unsigned int iFramesPerBuffer);

    // Used by SoundDevices to "push" any audio from their inputs that they have
    // into the mixing engine.
    void pushInputBuffers(const QList<AudioInputBuffer>& inputs,
                          const unsigned int iFramesPerBuffer);


    void writeProcess();
    void readProcess();

    void registerOutput(AudioOutput output, AudioSource *src);
    void registerInput(AudioInput input, AudioDestination *dest);
    QList<AudioOutput> registeredOutputs() const;
    QList<AudioInput> registeredInputs() const;

    QSharedPointer<EngineNetworkStream> getNetworkStream() const {
        return m_pNetworkStream;
    }

  signals:
    void devicesUpdated(); // emitted when pointers to SoundDevices go stale
    void devicesSetup(); // emitted when the sound devices have been set up
    void outputRegistered(AudioOutput output, AudioSource *src);
    void inputRegistered(AudioInput input, AudioDestination *dest);

  private:
    // Closes all the devices and empties the list of devices we have.
    void clearDeviceList(bool sleepAfterClosing);

    // Closes all the open sound devices. Because multiple soundcards might be
    // open, this method simply runs through the list of all known soundcards
    // (from PortAudio) and attempts to close them all. Closing a soundcard that
    // isn't open is safe.
    void closeDevices(bool sleepAfterClosing);

    EngineMaster *m_pMaster;
    UserSettingsPointer m_pConfig;
#ifdef __PORTAUDIO__
    SoundManagerPortAudio m_smPortAudio;
#endif
    SoundManagerJack m_smJack;
    QList<SoundDevice*> m_devices;
    QList<unsigned int> m_samplerates;
    QList<CSAMPLE*> m_inputBuffers;

    SoundManagerConfig m_config;
    SoundDevice* m_pErrorDevice;
    QHash<AudioOutput, AudioSource*> m_registeredSources;
    QHash<AudioInput, AudioDestination*> m_registeredDestinations;
    ControlObject* m_pControlObjectSoundStatusCO;
    ControlObject* m_pControlObjectVinylControlGainCO;

    QSharedPointer<EngineNetworkStream> m_pNetworkStream;
};

#endif

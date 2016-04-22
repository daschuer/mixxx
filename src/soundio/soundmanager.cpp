#include "soundio/soundmanager.h"

#include <QtDebug>
#include <cstring> // for memcpy and strcmp

#include "controlobject.h"
#include "engine/enginebuffer.h"
#include "engine/enginemaster.h"
#include "engine/sidechain/enginenetworkstream.h"
#include "soundio/sounddevice.h"
#include "soundio/sounddevicenetwork.h"
#include "soundio/sounddeviceportaudio.h"
#include "soundio/sounddevicejack.h"
#include "soundio/soundmanagerutil.h"
#include "util/cmdlineargs.h"
#include "util/defs.h"
#include "util/sample.h"
#include "util/sleep.h"
#include "util/version.h"
#include "vinylcontrol/defs_vinylcontrol.h"

namespace {

struct DeviceMode {
    SoundDevice* device;
    bool isInput;
    bool isOutput;
};

#ifdef __LINUX__
const unsigned int kSleepSecondsAfterClosingDevice = 5;
#endif
} // anonymous namespace

SoundManager::SoundManager(UserSettingsPointer pConfig,
                           EngineMaster *pMaster)
        : m_pMaster(pMaster),
          m_pConfig(pConfig),
#ifdef __PORTAUDIO__
 //         m_smPortAudio(pConfig),
#endif
          m_smJack(pConfig),
          m_pErrorDevice(NULL) {
    // TODO(xxx) some of these ControlObject are not needed by soundmanager, or are unused here.
    // It is possible to take them out?
    m_pControlObjectSoundStatusCO = new ControlObject(ConfigKey("[SoundManager]", "status"));
    m_pControlObjectSoundStatusCO->set(SOUNDMANAGER_DISCONNECTED);
    m_pControlObjectVinylControlGainCO = new ControlObject(ConfigKey(VINYL_PREF_KEY, "gain"));

    //Hack because PortAudio samplerate enumeration is slow as hell on Linux (ALSA dmix sucks, so we can't blame PortAudio)
    m_samplerates.push_back(44100);
    m_samplerates.push_back(48000);
    m_samplerates.push_back(96000);

    m_pNetworkStream = QSharedPointer<EngineNetworkStream>(
            new EngineNetworkStream(2, 0));

    queryDevices();

    if (!m_config.readFromDisk()) {
        m_config.loadDefaults(this, SoundManagerConfig::ALL);
    }
    checkConfig();
    m_config.writeToDisk(); // in case anything changed by applying defaults
}

SoundManager::~SoundManager() {
    // Clean up devices.
    const bool sleepAfterClosing = false;
    clearDeviceList(sleepAfterClosing);

    // vinyl control proxies and input buffers are freed in closeDevices, called
    // by clearDeviceList -- bkgood

    delete m_pControlObjectSoundStatusCO;
    delete m_pControlObjectVinylControlGainCO;
}

QList<SoundDevice*> SoundManager::getDeviceList(
    QString filterAPI, bool bOutputDevices, bool bInputDevices) {
    //qDebug() << "SoundManager::getDeviceList";

    if (filterAPI == "None") {
        QList<SoundDevice*> emptyList;
        return emptyList;
    }

    // Create a list of sound devices filtered to match given API and
    // input/output.
    QList<SoundDevice*> filteredDeviceList;

    for (const auto& device: m_devices) {
        // Skip devices that don't match the API, don't have input channels when
        // we want input devices, or don't have output channels when we want
        // output devices.
        if (device->getHostAPI() != filterAPI ||
                (bOutputDevices && device->getNumOutputChannels() <= 0) ||
                (bInputDevices && device->getNumInputChannels() <= 0)) {
            continue;
        }
        filteredDeviceList.push_back(device);
    }
    return filteredDeviceList;
}

QList<QString> SoundManager::getHostAPIList() const {
    QList<QString> apiList;
#ifdef __PORTAUDIO__
//    m_smPortAudio.appendHostAPIList(&apiList);
#endif
    m_smJack.appendHostAPIList(&apiList);
    return apiList;
}

void SoundManager::closeDevices(bool sleepAfterClosing) {
    //qDebug() << "SoundManager::closeDevices()";

    bool closed = false;
    for (const auto& pDevice: m_devices) {
        if (pDevice->isOpen()) {
            // NOTE(rryan): As of 2009 (?) it has been safe to close() a SoundDevice
            // while callbacks are active.
            pDevice->close();
            closed = true;
        }
    }

    if (closed && sleepAfterClosing) {
#ifdef __LINUX__
        // Sleep for 5 sec to allow asynchronously sound APIs like "pulse" to free
        // its resources as well
        sleep(kSleepSecondsAfterClosingDevice);
#endif
    }

    // TODO(rryan): Should we do this before SoundDevice::close()? No! Because
    // then the callback may be running when we call
    // onInputDisconnected/onOutputDisconnected.
    for (const auto& pDevice: m_devices) {
        for (const auto& in: pDevice->inputs()) {
            // Need to tell all registered AudioDestinations for this AudioInput
            // that the input was disconnected.
            auto it = m_registeredDestinations.find(in);
            if (it != m_registeredDestinations.end()) {
                it.value()->onInputUnconfigured(in);
            }
        }
        for (const auto& out: pDevice->outputs()) {
            // Need to tell all registered AudioSources for this AudioOutput
            // that the output was disconnected.
            auto it = m_registeredSources.find(out);
            if (it != m_registeredSources.end()) {
                it.value()->onOutputDisconnected(out);
            }
        }
    }

    while (!m_inputBuffers.isEmpty()) {
        CSAMPLE* pBuffer = m_inputBuffers.takeLast();
        if (pBuffer != NULL) {
            SampleUtil::free(pBuffer);
        }
    }
    m_inputBuffers.clear();

    // Indicate to the rest of Mixxx that sound is disconnected.
    m_pControlObjectSoundStatusCO->set(SOUNDMANAGER_DISCONNECTED);
}

void SoundManager::clearDeviceList(bool sleepAfterClosing) {
    //qDebug() << "SoundManager::clearDeviceList()";

    // Close the devices first.
    closeDevices(sleepAfterClosing);

    // Empty out the list of devices we currently have.
    while (!m_devices.empty()) {
        SoundDevice* dev = m_devices.takeLast();
        delete dev;
    }

#ifdef __PORTAUDIO__
 //   m_smPortAudio.clearDeviceList();
#endif
    m_smJack.clearDeviceList();
}

QList<unsigned int> SoundManager::getSampleRates(QString api) const {
    QList<unsigned int> samplerates;
    if (m_smJack.isSampleRateDefinedByApi(api, &samplerates)) {
        return samplerates;
    }
    return m_samplerates;
}

QList<unsigned int> SoundManager::getSampleRates() const {
    return getSampleRates("");
}

void SoundManager::queryDevices() {
    qDebug() << "SoundManager::queryDevices()";
    m_smJack.queryDevices(&m_devices, this);
#ifdef __PORTAUDIO__
 //   m_smPortAudio.queryDevices(&m_devices, this);
#endif
    queryDevicesMixxx();

    // now tell the prefs that we updated the device list -- bkgood
    emit(devicesUpdated());
}

void SoundManager::clearAndQueryDevices() {
    const bool sleepAfterClosing = true;
    clearDeviceList(sleepAfterClosing);
    queryDevices();
}

void SoundManager::queryDevicesMixxx() {
    SoundDeviceNetwork* currentDevice = new SoundDeviceNetwork(
            m_pConfig, this, m_pNetworkStream);
    m_devices.push_back(currentDevice);
}

Result SoundManager::setupDevices() {
    // NOTE(rryan): Big warning: This function is concurrent with calls to
    // pushBuffer and onDeviceOutputCallback until closeDevices() below.

    qDebug() << "SoundManager::setupDevices()";
    m_pControlObjectSoundStatusCO->set(SOUNDMANAGER_CONNECTING);
    Result err = OK;
    // NOTE(rryan): Do not clear m_pClkRefDevice here. If we didn't touch the
    // SoundDevice that is the clock reference, then it is safe to leave it as
    // it was. Clearing it causes the engine to stop being processed which
    // results in a stuttering noise (sometimes a loud buzz noise at low
    // latencies) when changing devices.
    //m_pClkRefDevice = NULL;
    m_pErrorDevice = NULL;
    int devicesAttempted = 0;
    int devicesOpened = 0;
    int outputDevicesOpened = 0;
    int inputDevicesOpened = 0;

    // filter out any devices in the config we don't actually have
    m_config.filterOutputs(this);
    m_config.filterInputs(this);

    // NOTE(rryan): Documenting for future people touching this class. If you
    // would like to remove the fact that we close all the devices first and
    // then re-open them, I'm with you! The problem is that SoundDevicePortAudio
    // and SoundManager are not thread safe and the way that mutual exclusion
    // between the Qt main thread and the PortAudio callback thread is acheived
    // is that we shut off the PortAudio callbacks for all devices by closing
    // every device first. We then update all the SoundDevice settings
    // (configured AudioInputs/AudioOutputs) and then we re-open them.
    //
    // If you want to solve this issue, you should start by separating the PA
    // callback from the logic in SoundDevicePortAudio. They should communicate
    // via message passing over a request/response FIFO.

    // Instead of clearing m_pClkRefDevice and then assigning it directly,
    // compute the new one then atomically hand off below.
    SoundDevice* pNewMasterClockRef = NULL;

    // pair is isInput, isOutput
    QList<DeviceMode> toOpen;
    bool haveOutput = false;
    for (const auto& device: m_devices) {
        DeviceMode mode = {device, false, false};
        device->clearInputs();
        device->clearOutputs();
        m_pErrorDevice = device;

        for (const auto& in:
                 m_config.getInputs().values(device->getInternalName())) {
            mode.isInput = true;

            AudioDestination* pDest = m_registeredDestinations.value(in);
            // TODO(bkgood) look into allocating this with the frames per
            // buffer value from SMConfig
            AudioInputBuffer aib(in, SampleUtil::alloc(MAX_BUFFER_LEN), pDest);
            err = device->addInput(aib) != SOUNDDEVICE_ERROR_OK ? ERR : OK;
            if (err != OK) {
                qWarning() << "failed to addInput to" << device->getInternalName();
                delete [] aib.getBuffer();
                goto closeAndError;
            }

            m_inputBuffers.append(aib.getBuffer());

            // Check if any AudioDestination is registered for this AudioInput
            // and call the onInputConnected method.
            auto itd = m_registeredDestinations.find(in);
            if (itd != m_registeredDestinations.end()) {
                itd.value()->onInputConfigured(in);
            }
        }
        QList<AudioOutput> outputs =
                m_config.getOutputs().values(device->getInternalName());

        // Statically connect the Network Device to the Sidechain
        if (device->getInternalName() == kNetworkDeviceInternalName) {
            AudioOutput out(AudioPath::SIDECHAIN, 0, 2, 0);
            outputs.append(out);
        }

        for (const auto& out: outputs) {
            mode.isOutput = true;
            if (device->getInternalName() != kNetworkDeviceInternalName) {
                haveOutput = true;
            }
            // following keeps us from asking for a channel buffer EngineMaster
            // doesn't have -- bkgood
            const CSAMPLE* pBuffer = nullptr;
            auto its = m_registeredSources.find(out);
            if (its != m_registeredSources.end()) {
                pBuffer = its.value()->buffer(out);
            }
            if (pBuffer == nullptr) {
                qDebug() << "AudioSource returned null for" << out.getTrString();
                continue;
            }

            AudioOutputBuffer aob(out, pBuffer);
            err = device->addOutput(aob) != SOUNDDEVICE_ERROR_OK ? ERR : OK;
            if (err != OK) {
                qWarning() << "failed to addOutput to" << device->getInternalName();
                goto closeAndError;
            }
            if (out.getType() == AudioOutput::MASTER) {
                pNewMasterClockRef = device;
            } else if ((out.getType() == AudioOutput::DECK ||
                        out.getType() == AudioOutput::BUS)
                    && !pNewMasterClockRef) {
                pNewMasterClockRef = device;
            }

            // Check if any AudioSource is registered for this AudioOutput and
            // call the onOutputConnected method.
            auto it = m_registeredSources.find(out);
            if (it != m_registeredSources.end()) {
                it.value()->onOutputConnected(out);
            }
        }

        if (mode.isInput || mode.isOutput) {
            device->setSampleRate(m_config.getSampleRate());
            device->setFramesPerBuffer(m_config.getFramesPerBuffer());
            toOpen.append(mode);
        }
    }

    for (const auto& mode: toOpen) {
        ++devicesAttempted;
        SoundDevice* device = mode.device;
        m_pErrorDevice = device;
        // If we have not yet set a clock source then we use the first
        // output device
        if (device->getInternalName() != kNetworkDeviceInternalName &&
                pNewMasterClockRef == NULL &&
                (!haveOutput || mode.isOutput)) {
            pNewMasterClockRef = device;
            qWarning() << "Output sound device clock reference not set! Using"
                       << device->getDisplayName();
        }

        int syncBuffers = m_config.getSyncBuffers();
        // If we are in safe mode and using experimental polling support, use
        // the default of 2 sync buffers instead.
        if (CmdlineArgs::Instance().getSafeMode() && syncBuffers == 0) {
            syncBuffers = 2;
        }
        err = device->open(pNewMasterClockRef == device, syncBuffers);
        if (err != OK) {
            qWarning() << "failed to open" << device->getInternalName();
            goto closeAndError;
        } else {
            ++devicesOpened;
            if (mode.isOutput) {
                ++outputDevicesOpened;
            }
            if (mode.isInput) {
                ++inputDevicesOpened;
            }
        }
    }

    if (pNewMasterClockRef) {
        qDebug() << "Using" << pNewMasterClockRef->getDisplayName()
                 << "as output sound device clock reference";
    } else {
        qWarning() << "No output devices opened, no clock reference device set";
    }

    qDebug() << outputDevicesOpened << "output sound devices opened";
    qDebug() << inputDevicesOpened << "input  sound devices opened";

    m_pControlObjectSoundStatusCO->set(
            outputDevicesOpened > 0 ?
                    SOUNDMANAGER_CONNECTED : SOUNDMANAGER_DISCONNECTED);

    // returns OK if we were able to open all the devices the user wanted
    if (devicesAttempted == devicesOpened) {
        emit(devicesSetup());
        return OK;
    }
    m_pErrorDevice = NULL;
    return ERR;

closeAndError:
    const bool sleepAfterClosing = false;
    closeDevices(sleepAfterClosing);
    return err;
}

SoundDevice* SoundManager::getErrorDevice() const {
    return m_pErrorDevice;
}

SoundManagerConfig SoundManager::getConfig() const {
    return m_config;
}

Result SoundManager::setConfig(SoundManagerConfig config) {
    m_config = config;
    checkConfig();

    // Close open devices. After this call we will not get any more
    // onDeviceOutputCallback() or pushBuffer() calls because all the
    // SoundDevices are closed. closeDevices() blocks and can take a while.
    const bool sleepAfterClosing = true;
    closeDevices(sleepAfterClosing);

    // certain parts of mixxx rely on this being here, for the time being, just
    // letting those be -- bkgood
    // Do this first so vinyl control gets the right samplerate -- Owen W.
    m_pConfig->set(ConfigKey("[Soundcard]","Samplerate"), ConfigValue(m_config.getSampleRate()));

    Result err = setupDevices();
    if (err == OK) {
        m_config.writeToDisk();
    }
    return err;
}

void SoundManager::checkConfig() {
    if (!m_config.checkAPI(*this)) {
        m_config.setAPI(SoundManagerConfig::kDefaultAPI);
        m_config.loadDefaults(this, SoundManagerConfig::API | SoundManagerConfig::DEVICES);
    }
    if (!m_config.checkSampleRate(*this)) {
        m_config.setSampleRate(SoundManagerConfig::kFallbackSampleRate);
        m_config.loadDefaults(this, SoundManagerConfig::OTHER);
    }

    // Even if we have a two-deck skin, if someone has configured a deck > 2
    // then the configuration needs to know about that extra deck.
    m_config.setCorrectDeckCount(getConfiguredDeckCount());
    // latency checks itself for validity on SMConfig::setLatency()
}

void SoundManager::onDeviceOutputCallback(const unsigned int iFramesPerBuffer) {
    // Produce a block of samples for output. EngineMaster expects stereo
    // samples so multiply iFramesPerBuffer by 2.
    m_pMaster->process(iFramesPerBuffer*2);
}

void SoundManager::pushInputBuffers(const QList<AudioInputBuffer>& inputs,
                                    const unsigned int iFramesPerBuffer) {
   for (QList<AudioInputBuffer>::ConstIterator i = inputs.begin(),
                 e = inputs.end(); i != e; ++i) {
        const AudioInputBuffer& in = *i;
        CSAMPLE* pInputBuffer = in.getBuffer();
        AudioDestination* pDest = in.getDestination();
        if (pDest != nullptr) {
            pDest->receiveBuffer(in, pInputBuffer, iFramesPerBuffer);
        }
    }
}

void SoundManager::writeProcess() {
    QListIterator<SoundDevice*> dev_it(m_devices);
    while (dev_it.hasNext()) {
        SoundDevice* device = dev_it.next();
        if (device) {
            device->writeProcess();
        }
    }
}

void SoundManager::readProcess() {
    QListIterator<SoundDevice*> dev_it(m_devices);
    while (dev_it.hasNext()) {
        SoundDevice* device = dev_it.next();
        if (device) {
            device->readProcess();
        }
    }
}

void SoundManager::registerOutput(AudioOutput output, AudioSource *src) {
    DEBUG_ASSERT(!m_registeredSources.contains(output));
    m_registeredSources.insert(output, src);
    m_smJack.registerOutput(output, src);
    m_registeredSources.insert(output, src);
    emit(outputRegistered(output, src));
}

void SoundManager::registerInput(AudioInput input, AudioDestination *dest) {
    DEBUG_ASSERT(!m_registeredDestinations.contains(input));
    m_registeredDestinations.insert(input, dest);
    m_smJack.registerInput(input, dest);
    emit(inputRegistered(input, dest));
}

QList<AudioOutput> SoundManager::registeredOutputs() const {
    return m_registeredSources.keys();
}

QList<AudioInput> SoundManager::registeredInputs() const {
    return m_registeredDestinations.keys();
}

void SoundManager::setConfiguredDeckCount(int count) {
    m_config.setDeckCount(count);
    checkConfig();
    m_config.writeToDisk();
}

int SoundManager::getConfiguredDeckCount() const {
    return m_config.getDeckCount();
}

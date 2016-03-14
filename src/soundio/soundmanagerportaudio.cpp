#include <QList>
#include <QLibrary>
#include <portaudio.h>

#include "util/version.h"

#include "soundio/sounddeviceportaudio.h"
#include "soundio/soundmanagerportaudio.h"
#include "soundio/sounddevice.h"


SoundManagerPortAudio::SoundManagerPortAudio(UserSettingsPointer pConfig)
        : m_pConfig(pConfig),
          m_paInitialized(false),
          m_jackSampleRate(-1) {
}

SoundManagerPortAudio::~SoundManagerPortAudio() {
    if (m_paInitialized) {
        Pa_Terminate();
        m_paInitialized = false;
    }
}
void SoundManagerPortAudio::appendHostAPIList(QList<QString>* pApiList) const {
    for (PaHostApiIndex i = 0; i < Pa_GetHostApiCount(); i++) {
        const PaHostApiInfo* api = Pa_GetHostApiInfo(i);
        if (api && QString(api->name) != "skeleton implementation") {
            pApiList->push_back(api->name);
        }
    }
}

void SoundManagerPortAudio::clearDeviceList() {
    if (m_paInitialized) {
        Pa_Terminate();
        m_paInitialized = false;
    }
}

bool SoundManagerPortAudio::isSampleRateDefinedByApi(QString api,
        QList<unsigned int>* pSamplerates) const {
    if (api == MIXXX_PORTAUDIO_JACK_STRING) {
        pSamplerates->append(m_jackSampleRate);
        return true;
    }
    return false;
}

void SoundManagerPortAudio::queryDevices(QList<SoundDevice*>* pDevices,
        SoundManager* pSM) {
#ifdef __PORTAUDIO__
    PaError err = paNoError;
    if (!m_paInitialized) {
#ifdef Q_OS_LINUX
        setJACKName();
#endif
        err = Pa_Initialize();
        m_paInitialized = true;
    }
    if (err != paNoError) {
        qDebug() << "Error:" << Pa_GetErrorText(err);
        m_paInitialized = false;
        return;
    }

    int iNumDevices = Pa_GetDeviceCount();
    if (iNumDevices < 0) {
        qDebug() << "ERROR: Pa_CountDevices returned" << iNumDevices;
        return;
    }

    const PaDeviceInfo* deviceInfo;
    for (int i = 0; i < iNumDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        if (!deviceInfo) {
            continue;
        }
        /* deviceInfo fields for quick reference:
            int     structVersion
            const char *    name
            PaHostApiIndex  hostApi
            int     maxInputChannels
            int     maxOutputChannels
            PaTime  defaultLowInputLatency
            PaTime  defaultLowOutputLatency
            PaTime  defaultHighInputLatency
            PaTime  defaultHighOutputLatency
            double  defaultSampleRate
         */
        SoundDevicePortAudio* currentDevice = new SoundDevicePortAudio(
                m_pConfig, pSM, deviceInfo, i);
        pDevices->push_back(currentDevice);
        if (!strcmp(Pa_GetHostApiInfo(deviceInfo->hostApi)->name,
                    MIXXX_PORTAUDIO_JACK_STRING)) {
            m_jackSampleRate = deviceInfo->defaultSampleRate;
        }
    }
#endif
}

void SoundManagerPortAudio::setJACKName() const {
#ifdef __PORTAUDIO__
#ifdef Q_OS_LINUX
    typedef PaError (*SetJackClientName)(const char *name);
    QLibrary portaudio("libportaudio.so.2");
    if (portaudio.load()) {
        SetJackClientName func(
            reinterpret_cast<SetJackClientName>(
                portaudio.resolve("PaJack_SetClientName")));
        if (func) {
            if (!func(Version::applicationName().toLocal8Bit().constData())) qDebug() << "JACK client name set";
        } else {
            qWarning() << "failed to resolve JACK name method";
        }
    } else {
        qWarning() << "failed to load portaudio for JACK rename";
    }
#endif
#endif
}

#include <QList>
#include <QLibrary>
#include <portaudio.h>

#include "util/version.h"

#include "soundio/sounddeviceportaudio.h"
#include "soundio/soundmanagerportaudio.h"
#include "soundio/sounddevice.h"

SoundManagerPortAudio::SoundManagerPortAudio(UserSettingsPointer pConfig)
        : m_pConfig(pConfig),
          m_paInitialized(false) {
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
        if (api &&
                QString(api->name) != MIXXX_PORTAUDIO_SKELETON_STRING &&
                QString(api->name) != MIXXX_PORTAUDIO_JACK_STRING) {
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

void SoundManagerPortAudio::queryDevices(QList<SoundDevice*>* pDevices,
        SoundManager* pSM) {
    qDebug() << "SoundManagerPortAudio::queryDevices";
#ifdef __PORTAUDIO__
    PaError err = paNoError;
    if (!m_paInitialized) {
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

        if (!strcmp(Pa_GetHostApiInfo(deviceInfo->hostApi)->name,
                    MIXXX_PORTAUDIO_JACK_STRING)) {
            // Blacklist Jack devices here since they are
            // added by SoundManagerJack
            continue;
        }

        SoundDevicePortAudio* currentDevice = new SoundDevicePortAudio(
                m_pConfig, pSM, deviceInfo, i);
        pDevices->push_back(currentDevice);
    }
#endif
}


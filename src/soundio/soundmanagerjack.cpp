#include <QList>
#include <QLibrary>
#include <regex.h>
#include <portaudio.h>

#include "util/version.h"

#include "soundio/sounddeviceportaudio.h"
#include "soundio/soundmanagerjack.h"
#include "soundio/sounddevice.h"
#include "soundio/soundmanagerutil.h"

namespace {
    const char* kJackAudioPortFilter = "audio";
} // Anonymous namespace

SoundManagerJack::SoundManagerJack(UserSettingsPointer pConfig)
        : m_pConfig(pConfig),
          m_jackSampleRate(48000),
          m_pJackClient(nullptr) {
}

SoundManagerJack::~SoundManagerJack() {
}

void SoundManagerJack::appendHostAPIList(QList<QString>* pApiList) const {
    pApiList->push_back(QString(MIXXX_PORTAUDIO_JACK_STRING));
}

void SoundManagerJack::clearDeviceList() {
}

bool SoundManagerJack::isSampleRateDefinedByApi(QString api,
        QList<unsigned int>* pSamplerates) const {
    if (api == MIXXX_PORTAUDIO_JACK_STRING) {
        pSamplerates->append(m_jackSampleRate);
        return true;
    }
    return false;
}

void SoundManagerJack::queryDevices(QList<SoundDevice*>* pDevices,
        SoundManager* pSM) {
    if (m_pJackClient == nullptr) {
        jackInitialize();
        if (m_pJackClient == nullptr) {
            return;
        }
    }

    m_jackSampleRate = jack_get_sample_rate(m_pJackClient);

    buildDeviceList();

    for (const auto& client: m_devices) {
        SoundDeviceJack* currentDevice = new SoundDeviceJack(
                m_pConfig, pSM, client);
        pDevices->push_back(currentDevice);
    }
}

void SoundManagerJack::jackInitialize() {
    /* Try to become a client of the JACK server.  If we cannot do
     * this, then this API cannot be used.
     *
     * Without the JackNoStartServer option, the jackd server is started
     * automatically which we do not want.
     */
    jack_status_t jackStatus = static_cast<jack_status_t>(0);
    //m_pJackClient = jack_client_open(
    //        Version::applicationNameCStr(),
    //        JackNoStartServer, &jackStatus);
    m_pJackClient = jack_client_open(
            Version::applicationNameCStr(),
            JackNullOption, &jackStatus);
    if(!m_pJackClient)
    {
        qDebug() << "SoundManagerJack::jackInitialize()"
                 << "Couldn't connect to JACK, status:"
                 << jackStatus;
    }
}

void SoundManagerJack::buildDeviceList() {
    /* JACK has no concept of a device.  To JACK, there are clients
     * which have an arbitrary number of ports.  To make this
     * intelligible to PortAudio clients, we will group each JACK client
     * into a device, and make each port of that client a channel */

    /* We can only retrieve the list of clients indirectly, by first
     * asking for a list of all ports, then parsing the port names
     * according to the client_name:port_name convention (which is
     * enforced by jackd)
     * A: If jack_get_ports returns NULL, there's nothing for us to do */
    const char **jack_ports = jack_get_ports(m_pJackClient, "",
            kJackAudioPortFilter, 0);

    for (size_t numPorts = 0; jack_ports[numPorts]; ++numPorts) {
        jack_port_t *pPort = jack_port_by_name(m_pJackClient, jack_ports[numPorts]);
        QString jackPort(jack_ports[numPorts]);
        qDebug() << jackPort;
        QStringList jackPortsplit = jackPort.split(':');
        JackDeviceInfo& deviceInfo = m_devices[jackPortsplit.at(0)];

        if (deviceInfo.name.isEmpty()) {
            deviceInfo.name = jackPortsplit.at(0);
            deviceInfo.sampleRate =  m_jackSampleRate;
        }

        if (jackPortsplit.count() > 1) {
            int flags = jack_port_flags(pPort);
            if (flags | JackPortIsInput) {
                deviceInfo.inputPorts.append(jackPortsplit.at(1));
            }
            if (flags | JackPortIsOutput) {
                deviceInfo.outputPorts.append(jackPortsplit.at(1));
            }
        }
    }
}


void SoundManagerJack::registerOutput(const AudioOutput& output) {
    DEBUG_ASSERT_AND_HANDLE(m_pJackClient != nullptr) {
        return;
    }
/*
    output.get

    for( i = 0; i < inputChannelCount; i++ )
    {
        snprintf( port_string, jack_port_name_size(), "in_%lu", ofs + i );
        UNLESS( stream->local_input_ports[i] = jack_port_register(
              jackHostApi->jack_client, port_string,
              JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0 ), paInsufficientMemory );
    }
    */
}

void SoundManagerJack::registerInput(const AudioInput& input) {
    DEBUG_ASSERT_AND_HANDLE(m_pJackClient != nullptr) {
        return;
    }
}

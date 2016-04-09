#include <QList>
#include <QLibrary>
#include <regex.h>
#include <portaudio.h>

#include "util/version.h"

#include "soundio/sounddeviceportaudio.h"
#include "soundio/soundmanagerjack.h"
#include "soundio/sounddevicejack.h"
#include "soundio/sounddevice.h"
#include "soundio/soundmanagerutil.h"

namespace {
    const char* kJackAudioPortFilter = "audio";

    void jackOnShutdown(void* arg) {
        SoundManagerJack* pSmJack = static_cast<SoundManagerJack*>(arg);
        pSmJack->onShutdown();
    }

    void jackErrorCallback(const char* msg) {
        qDebug() << "Jack error:" << msg;
    }

    int jackSampleRateCallback(jack_nframes_t nframes, void* arg) {
        SoundManagerJack* pSmJack = static_cast<SoundManagerJack*>(arg);
        return pSmJack->sampleRateCallback(nframes);
    }

    int jackXRunCallback(void *arg) {
        SoundManagerJack* pSmJack = static_cast<SoundManagerJack*>(arg);
        return pSmJack->xRunCallback();
    }

    // This is called by the JACK server in a
    // special realtime thread once for each audio cycle.
    int jackProcessCallback(jack_nframes_t nframes, void* arg) {
        SoundManagerJack* pSmJack = static_cast<SoundManagerJack*>(arg);
        return pSmJack->processCallback(nframes);
    }
} // Anonymous namespace

SoundManagerJack::SoundManagerJack(UserSettingsPointer pConfig)
        : m_pConfig(pConfig),
          m_jackSampleRate(48000),
          m_jackBufferSize(0),
          m_activated(false),
          m_pJackClient(nullptr) {
}

SoundManagerJack::~SoundManagerJack() {
    /*
    PaJackHostApiRepresentation *jackHostApi = (PaJackHostApiRepresentation*)hostApi;

    * note: this automatically disconnects all ports, since a deactivated
     * client is not allowed to have any ports connected *
    ASSERT_CALL( jack_deactivate( jackHostApi->jack_client ), 0 );

    ASSERT_CALL( pthread_mutex_destroy( &jackHostApi->mtx ), 0 );
    ASSERT_CALL( pthread_cond_destroy( &jackHostApi->cond ), 0 );

    ASSERT_CALL( jack_client_close( jackHostApi->jack_client ), 0 );

    if( jackHostApi->deviceInfoMemory )
    {
        PaUtil_FreeAllAllocations( jackHostApi->deviceInfoMemory );
        PaUtil_DestroyAllocationGroup( jackHostApi->deviceInfoMemory );
    }

    PaUtil_FreeMemory( jackHostApi );

    free( jackErr_ );
    jackErr_ = NULL;
    */
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
                m_pConfig, pSM, this, client);
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

    jack_on_shutdown(m_pJackClient, jackOnShutdown, this);
    jack_set_error_function(jackErrorCallback);
    m_jackBufferSize = jack_get_buffer_size (m_pJackClient);
    jack_set_sample_rate_callback(m_pJackClient, jackSampleRateCallback, this);
    jack_set_xrun_callback(m_pJackClient, jackXRunCallback, this);
    jack_set_process_callback(m_pJackClient, jackProcessCallback, this);
    jack_activate(m_pJackClient);
    m_activated = true;
}

void SoundManagerJack::buildDeviceList() {
    m_devices.clear();

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
            if (flags & JackPortIsInput) {
                deviceInfo.inputPorts.append(jackPortsplit.at(1));
            }
            if (flags & JackPortIsOutput) {
                deviceInfo.outputPorts.append(jackPortsplit.at(1));
            }
        }
    }
}

void SoundManagerJack::registerOutput(const AudioOutput& output, AudioSource *src) {
    DEBUG_ASSERT_AND_HANDLE(m_pJackClient != nullptr) {
        return;
    }

    for (unsigned char i = 0; i < output.getChannelCount(); ++i) {
        QString portName = output.getTrString() +
                QLatin1String(" ") +
                QString::number(output.getChannelBase() + i + 1);
        qDebug() << "SoundManagerJack::registerOutput" << portName;
        jack_port_t* port = jack_port_register(m_pJackClient,
                portName.toLocal8Bit(),
                JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );
        AudioOutput outputChannel(output.getType(),
                output.getChannelBase() + i,
                1,
                output.getIndex());
        m_registeredOutputPorts.insert(outputChannel, port);
    }
}

void SoundManagerJack::registerInput(const AudioInput& input, AudioDestination *dest) {
    DEBUG_ASSERT_AND_HANDLE(m_pJackClient != nullptr) {
        return;
    }

    for (unsigned char i = 0; i < input.getChannelCount(); ++i) {
        QString portName = input.getTrString() +
                QLatin1String(" ") + QString::number(input.getChannelBase() + i + 1);
        qDebug() << "SoundManagerJack::registerInput" << portName;
        jack_port_t* port = jack_port_register(m_pJackClient,
                portName.toLocal8Bit(),
                JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        AudioInput inputChannel(input.getType(),
                input.getChannelBase() + i,
                1,
                input.getIndex());
        m_registeredInputPorts.insert(inputChannel, port);
    }
}


void SoundManagerJack::connectOutputPorts(
        QString name,
        QList<QString> inputPorts,
        const AudioOutputBuffer& output,
        bool connect) {

    for (unsigned char i = 0;
            i < output.getChannelCount() && i < inputPorts.count();
            ++i) {
        QString outputPortName = Version::applicationNameCStr() +
                QLatin1String(":") +
                output.getTrString() +
                QLatin1String(" ") +
                QString::number(output.getChannelBase() + i + 1);
        QString inputPortName = name + QLatin1String(":") + inputPorts[i];

        int r;
        if (connect) {
            r = jack_connect(m_pJackClient,
                             outputPortName.toLocal8Bit(),
                             inputPortName.toLocal8Bit());
        } else {
            r = jack_disconnect(m_pJackClient,
                                outputPortName.toLocal8Bit(),
                                inputPortName.toLocal8Bit());
        }
        if (!(0 == r) && !(EEXIST == r)) {
            qWarning() << "jack_connect" << outputPortName << inputPortName;
        }
    }
}


void SoundManagerJack::connectInputPorts(
        QString name,
        QList<QString> outputPorts,
        const AudioInputBuffer& input,
        bool connect) {

    for (unsigned char i = 0;
            i < input.getChannelCount() && i < outputPorts.count();
            ++i) {
        QString outputPortName = name + QLatin1String(":") + outputPorts[i];
        QString inputPortName = Version::applicationNameCStr() +
                QLatin1String(":") +
                input.getTrString() +
                QLatin1String(" ") +
                QString::number(input.getChannelBase() + i + 1);

        int r;
        if (connect) {
            r = jack_connect(m_pJackClient,
                             outputPortName.toLocal8Bit(),
                             inputPortName.toLocal8Bit());
        } else {
            r = jack_disconnect(m_pJackClient,
                                outputPortName.toLocal8Bit(),
                                inputPortName.toLocal8Bit());
        }
        if (!(0 == r) && !(EEXIST == r)) {
            qWarning() << "jack_connect" << outputPortName << inputPortName;
        }
    }
}

void SoundManagerJack::onShutdown() {

/*
PaJackHostApiRepresentation *jackApi = (PaJackHostApiRepresentation *)arg;
PaJackStream *stream = jackApi->processQueue;

PA_DEBUG(( "%s: JACK server is shutting down\n", __FUNCTION__ ));
for( ; stream; stream = stream->next )
{
    stream->is_active = 0;
}

* Make sure that the main thread doesn't get stuck waiting on the condition *
ASSERT_CALL( pthread_mutex_lock( &jackApi->mtx ), 0 );
jackApi->jackIsDown = 1;
ASSERT_CALL( pthread_cond_signal( &jackApi->cond ), 0 );
ASSERT_CALL( pthread_mutex_unlock( &jackApi->mtx ), 0 );
*/
}

int SoundManagerJack::sampleRateCallback(jack_nframes_t nframes) {
    /*
    PaJackHostApiRepresentation *jackApi = (PaJackHostApiRepresentation *)arg;
    double sampleRate = (double)nframes;
    PaJackStream *stream = jackApi->processQueue;

    * Update all streams in process queue *
    PA_DEBUG(( "%s: Acting on change in JACK samplerate: %f\n", __FUNCTION__, sampleRate ));
    for( ; stream; stream = stream->next )
    {
        if( stream->streamRepresentation.streamInfo.sampleRate != sampleRate )
        {
            PA_DEBUG(( "%s: Updating samplerate\n", __FUNCTION__ ));
            UpdateSampleRate( stream, sampleRate );
        }
    }
*/
    return 0;
}

int SoundManagerJack::xRunCallback() {
    //hostApi->xrun = TRUE;
    qDebug() << "JACK signaled xrun";
    return 0;
}

int SoundManagerJack::processCallback(jack_nframes_t nframes) {
    return 0;
 /*

    PaError result = paNoError;
    PaJackHostApiRepresentation *hostApi = (PaJackHostApiRepresentation *)userData;
    PaJackStream *stream = NULL;
    int xrun = hostApi->xrun;
    hostApi->xrun = 0;

    assert( hostApi );

    ENSURE_PA( UpdateQueue( hostApi ) );

    * Process each stream *
    stream = hostApi->processQueue;
    for( ; stream; stream = stream->next )
    {
        if( xrun )  * Don't override if already set *
            stream->xrun = 1;

        * See if this stream is to be started *
        if( stream->doStart )
        {
            * If we can't obtain a lock, we'll try next time *
            int err = pthread_mutex_trylock( &stream->hostApi->mtx );
            if( !err )
            {
                if( stream->doStart )   * Could potentially change before obtaining the lock *
                {
                    stream->is_active = 1;
                    stream->doStart = 0;
                    PA_DEBUG(( "%s: Starting stream\n", __FUNCTION__ ));
                    ASSERT_CALL( pthread_cond_signal( &stream->hostApi->cond ), 0 );
                    stream->callbackResult = paContinue;
                    stream->isSilenced = 0;
                }

                ASSERT_CALL( pthread_mutex_unlock( &stream->hostApi->mtx ), 0 );
            }
            else
                assert( err == EBUSY );
        }
        else if( stream->doStop || stream->doAbort )    * Should we stop/abort stream? *
        {
            if( stream->callbackResult == paContinue )     * Ok, make it stop *
            {
                PA_DEBUG(( "%s: Stopping stream\n", __FUNCTION__ ));
                stream->callbackResult = stream->doStop ? paComplete : paAbort;
            }
        }

        if( stream->is_active )
            ENSURE_PA( RealProcess( stream, frames ) );
        * If we have just entered inactive state, silence output *
        if( !stream->is_active && !stream->isSilenced )
        {
            int i;

            * Silence buffer after entering inactive state *
            PA_DEBUG(( "Silencing the output\n" ));
            for( i = 0; i < stream->num_outgoing_connections; ++i )
            {
                jack_default_audio_sample_t *buffer = jack_port_get_buffer( stream->local_output_ports[i], frames );
                memset( buffer, 0, sizeof (jack_default_audio_sample_t) * frames );
            }

            stream->isSilenced = 1;
        }

        if( stream->doStop || stream->doAbort )
        {
            * See if RealProcess has acted on the request *
            if( !stream->is_active )   * Ok, signal to the main thread that we've carried out the operation *
            {
                * If we can't obtain a lock, we'll try next time *
                int err = pthread_mutex_trylock( &stream->hostApi->mtx );
                if( !err )
                {
                    stream->doStop = stream->doAbort = 0;
                    ASSERT_CALL( pthread_cond_signal( &stream->hostApi->cond ), 0 );
                    ASSERT_CALL( pthread_mutex_unlock( &stream->hostApi->mtx ), 0 );
                }
                else
                    assert( err == EBUSY );
            }
        }
    }

    return 0;
error:
    return -1;
    */
}





#include <QList>
#include <QLibrary>
#include <regex.h>

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

    void jackPortConnectCallback(jack_port_id_t a,
                                 jack_port_id_t b,
                                 int connect,
                                 void* arg) {
        SoundManagerJack* pSmJack = static_cast<SoundManagerJack*>(arg);
        return pSmJack->portConnectCallback(a, b, connect);
    }

    int jackXRunCallback(void *arg) {
        //qDebug() << "jackXRunCallback";
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
    m_jackBufferSize = jack_get_buffer_size(m_pJackClient);
    jack_set_port_connect_callback(m_pJackClient, jackPortConnectCallback, this);
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

void SoundManagerJack::registerOutput(const AudioOutput& output, AudioSource *pSrc) {
    DEBUG_ASSERT_AND_HANDLE(m_pJackClient != nullptr) {
        return;
    }

    for (unsigned char i = 0; i < output.getChannelCount(); ++i) {
        QString portName = output.getTrString() +
                QLatin1String(" ") +
                QString::number(output.getChannelBase() + i + 1);
        qDebug() << "SoundManagerJack::registerOutput" << portName;
        jack_port_t* pPort = jack_port_register(m_pJackClient,
                portName.toLocal8Bit(),
                JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );
        AudioOutput outputChannel(output.getType(),
                output.getChannelBase() + i,
                1,
                output.getIndex());
        OutputPort op = {outputChannel, pSrc, pPort};
        m_registeredOutputPorts.insert(portName, op);
    }
}

void SoundManagerJack::registerInput(const AudioInput& input, AudioDestination *pDest) {
    DEBUG_ASSERT_AND_HANDLE(m_pJackClient != nullptr) {
        return;
    }

    for (unsigned char i = 0; i < input.getChannelCount(); ++i) {
        QString portName = input.getTrString() +
                QLatin1String(" ") + QString::number(input.getChannelBase() + i + 1);
        qDebug() << "SoundManagerJack::registerInput" << portName;
        jack_port_t* pPort = jack_port_register(m_pJackClient,
                portName.toLocal8Bit(),
                JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        AudioInput inputChannel(input.getType(),
                input.getChannelBase() + i,
                1,
                input.getIndex());
        InputPort ip = {inputChannel, pDest, pPort};
        m_registeredInputPorts.insert(portName, ip);
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
    qDebug() << "SoundManagerJack::onShutdown";

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

void SoundManagerJack::portConnectCallback(jack_port_id_t a,
                             jack_port_id_t b,
                             int connect) {
    qDebug() << "SoundManagerJack::PortConnectCallback";
    portConnect(a, connect);
    portConnect(b, connect);
}

void SoundManagerJack::portConnect(jack_port_id_t portId, int connect) {
    jack_port_t* pPort = jack_port_by_id(m_pJackClient, portId);
    if (jack_port_is_mine (m_pJackClient,  pPort)) {
        int flags = jack_port_flags (pPort);
        const char* shortName = jack_port_short_name(pPort);
        if (flags & JackPortIsInput) {
            auto ipIt = m_registeredInputPorts.find(QString(shortName));
            if (ipIt != m_registeredInputPorts.end()) {
                InputPort& ip = ipIt.value();
                if (connect) {
                    // Add port to process list and configure channel
                    m_connectedInputPorts.append(ip);
                    ip.pDest->onInputConfigured(ip.audioInput);
                } else {
                    // Remove port from process list and unconfigure channel
                    for(int i = 0; i < m_connectedInputPorts.count(); ++i) {
                        if (m_connectedInputPorts.at(i).audioInput == ip.audioInput) {
                            ip.pDest->onInputUnconfigured(ip.audioInput);
                            m_connectedInputPorts.removeAt(i);
                            break;
                        }
                    }
                }
            }
        }
        if (flags & JackPortIsOutput) {
            auto opIt = m_registeredOutputPorts.find(QString(shortName));
            if (opIt != m_registeredOutputPorts.end()) {
                OutputPort& op = opIt.value();
                if (connect) {
                    // Add port to process list and configure channel
                    m_connectedOutputPorts.append(op);
                    op.pSrc->onOutputConnected(op.audioOutput);
                } else {
                    // Remove port from process list and unconfigure channel
                    for(int i = 0; i < m_connectedOutputPorts.count(); ++i) {
                        if (m_connectedOutputPorts.at(i).audioOutput == op.audioOutput) {
                            op.pSrc->onOutputDisconnected(op.audioOutput);
                            m_connectedOutputPorts.removeAt(i);
                            break;
                        }
                    }
                }
            }
        }
    }
}

int SoundManagerJack::sampleRateCallback(jack_nframes_t nframes) {
    qDebug() << "SoundManagerJack::sampleRateCallback" << nframes;
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
    // TODO count and

    //qDebug() << "JACK signaled xrun" << jack_get_run_delayed_usecs(m_pJackClient);
    qDebug() << "JACK signaled xrun";
    return 0;
}

int SoundManagerJack::processCallback(jack_nframes_t nframes) {
    //qDebug() << "SoundManagerJack::processCallback" << nframes;

    if(m_processMutex.tryLock()) {

        /*
        const double sr = jack_get_sample_rate(m_pJackClient); // Shouldn't change during the process callback

        PaStreamCallbackTimeInfo timeInfo = {0,0,0};
        timeInfo.currentTime = (jack_frame_time(m_pJackClient) - stream->t0) / sr;

       // if ( find clock reference device
       //         m_registeredInputPorts

        timeInfo.inputBufferAdcTime = timeInfo.currentTime - jack_port_get_latency( stream->remote_output_ports[0] )
            / sr;
        timeInfo.outputBufferDacTime = timeInfo.currentTime + jack_port_get_latency( stream->remote_input_ports[0] )
            / sr;


        PaUtil_BeginCpuLoadMeasurement( &stream->cpuLoadMeasurer );


        PaUtil_BeginBufferProcessing( &stream->bufferProcessor, &timeInfo,
                cbFlags );

        if( stream->num_incoming_connections > 0 )
            PaUtil_SetInputFrameCount( &stream->bufferProcessor, frames );
        if( stream->num_outgoing_connections > 0 )
            PaUtil_SetOutputFrameCount( &stream->bufferProcessor, frames );

        for( chn = 0; chn < stream->num_incoming_connections; chn++ )
        {
            jack_default_audio_sample_t *channel_buf = (jack_default_audio_sample_t*)
                jack_port_get_buffer( stream->local_input_ports[chn],
                        frames );

            PaUtil_SetNonInterleavedInputChannel( &stream->bufferProcessor,
                    chn,
                    channel_buf );
        }

        for( chn = 0; chn < stream->num_outgoing_connections; chn++ )
        {
            jack_default_audio_sample_t *channel_buf = (jack_default_audio_sample_t*)
                jack_port_get_buffer( stream->local_output_ports[chn],
                        frames );

            PaUtil_SetNonInterleavedOutputChannel( &stream->bufferProcessor,
                    chn,
                    channel_buf );
        }

        framesProcessed = PaUtil_EndBufferProcessing( &stream->bufferProcessor,
                &stream->callbackResult );
        * We've specified a host buffer size mode where every frame should be consumed by the buffer processor *
        assert( framesProcessed == frames );

        PaUtil_EndCpuLoadMeasurement( &stream->cpuLoadMeasurer, framesProcessed );

        */

        m_processMutex.unlock();
    }
    return 0;
}





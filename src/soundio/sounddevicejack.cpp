
#include "soundio/sounddevicejack.h"

#include <portaudio.h>
#include <float.h>

#include <QtDebug>
#include <QThread>

#ifdef __LINUX__
#include <QLibrary>
#endif

#include "controlobject.h"
#include "controlobjectslave.h"
#include "soundio/sounddevice.h"
#include "soundio/soundmanager.h"
#include "soundio/soundmanagerutil.h"
#include "util/denormalsarezero.h"
#include "util/sample.h"
#include "util/timer.h"
#include "util/trace.h"
#include "vinylcontrol/defs_vinylcontrol.h"
#include "waveform/visualplayposition.h"

// static
volatile int SoundDeviceJack::m_underflowHappened = 0;

namespace {

// Buffer for drift correction 1 full, 1 for r/w, 1 empty
static const int kDriftReserve = 1;
// Buffer for drift correction 1 full, 1 for r/w, 1 empty
static const int kFifoSize = 2 * kDriftReserve + 1;

int paV19Callback(const void *inputBuffer, void *outputBuffer,
                  unsigned long framesPerBuffer,
                  const PaStreamCallbackTimeInfo *timeInfo,
                  PaStreamCallbackFlags statusFlags,
                  void *soundDevice) {
    return ((SoundDeviceJack*) soundDevice)->callbackProcess(
            (unsigned int) framesPerBuffer, (CSAMPLE*) outputBuffer,
            (const CSAMPLE*) inputBuffer, timeInfo, statusFlags);
}

int paV19CallbackDrift(const void *inputBuffer, void *outputBuffer,
                       unsigned long framesPerBuffer,
                       const PaStreamCallbackTimeInfo *timeInfo,
                       PaStreamCallbackFlags statusFlags,
                       void *soundDevice) {
    return ((SoundDeviceJack*) soundDevice)->callbackProcessDrift(
            (unsigned int) framesPerBuffer, (CSAMPLE*) outputBuffer,
            (const CSAMPLE*) inputBuffer, timeInfo, statusFlags);
}

int paV19CallbackClkRef(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *soundDevice) {
    return ((SoundDeviceJack*) soundDevice)->callbackProcessClkRef(
            (unsigned int) framesPerBuffer, (CSAMPLE*) outputBuffer,
            (const CSAMPLE*) inputBuffer, timeInfo, statusFlags);
}

/*
static PaError ValidateOpenStreamParameters(
    const PaStreamParameters *inputParameters,
    const PaStreamParameters *outputParameters,
    double sampleRate,
    unsigned long framesPerBuffer,
    PaStreamFlags streamFlags,
    PaStreamCallback *streamCallback,
    PaUtilHostApiRepresentation **hostApi,
    PaDeviceIndex *hostApiInputDevice,
    PaDeviceIndex *hostApiOutputDevice )
{
    int inputHostApiIndex  = -1,  Surpress uninitialised var warnings: compiler does
        outputHostApiIndex = -1;  not see that if inputParameters and outputParame-
                                  ters are both nonzero, these indices are set.

    if( (inputParameters == NULL) && (outputParameters == NULL) )
    {
        return paInvalidDevice;  @todo should be a new error code "invalid device parameters" or something
    }
    else
    {
        if( inputParameters == NULL )
        {
            *hostApiInputDevice = paNoDevice;
        }
        else if( inputParameters->device == paUseHostApiSpecificDeviceSpecification )
        {
            if( inputParameters->hostApiSpecificStreamInfo )
            {
                inputHostApiIndex = Pa_HostApiTypeIdToHostApiIndex(
                        ((PaUtilHostApiSpecificStreamInfoHeader*)inputParameters->hostApiSpecificStreamInfo)->hostApiType );

                if( inputHostApiIndex != -1 )
                {
                    *hostApiInputDevice = paUseHostApiSpecificDeviceSpecification;
                    *hostApi = hostApis_[inputHostApiIndex];
                }
                else
                {
                    return paInvalidDevice;
                }
            }
            else
            {
                return paInvalidDevice;
            }
        }
        else
        {
            if( inputParameters->device < 0 || inputParameters->device >= deviceCount_ )
                return paInvalidDevice;

            inputHostApiIndex = FindHostApi( inputParameters->device, hostApiInputDevice );
            if( inputHostApiIndex < 0 )
                return paInternalError;

            *hostApi = hostApis_[inputHostApiIndex];

            if( inputParameters->channelCount <= 0 )
                return paInvalidChannelCount;

            if( !SampleFormatIsValid( inputParameters->sampleFormat ) )
                return paSampleFormatNotSupported;

            if( inputParameters->hostApiSpecificStreamInfo != NULL )
            {
                if( ((PaUtilHostApiSpecificStreamInfoHeader*)inputParameters->hostApiSpecificStreamInfo)->hostApiType
                        != (*hostApi)->info.type )
                    return paIncompatibleHostApiSpecificStreamInfo;
            }
        }

        if( outputParameters == NULL )
        {
            *hostApiOutputDevice = paNoDevice;
        }
        else if( outputParameters->device == paUseHostApiSpecificDeviceSpecification  )
        {
            if( outputParameters->hostApiSpecificStreamInfo )
            {
                outputHostApiIndex = Pa_HostApiTypeIdToHostApiIndex(
                        ((PaUtilHostApiSpecificStreamInfoHeader*)outputParameters->hostApiSpecificStreamInfo)->hostApiType );

                if( outputHostApiIndex != -1 )
                {
                    *hostApiOutputDevice = paUseHostApiSpecificDeviceSpecification;
                    *hostApi = hostApis_[outputHostApiIndex];
                }
                else
                {
                    return paInvalidDevice;
                }
            }
            else
            {
                return paInvalidDevice;
            }
        }
        else
        {
            if( outputParameters->device < 0 || outputParameters->device >= deviceCount_ )
                return paInvalidDevice;

            outputHostApiIndex = FindHostApi( outputParameters->device, hostApiOutputDevice );
            if( outputHostApiIndex < 0 )
                return paInternalError;

            *hostApi = hostApis_[outputHostApiIndex];

            if( outputParameters->channelCount <= 0 )
                return paInvalidChannelCount;

            if( !SampleFormatIsValid( outputParameters->sampleFormat ) )
                return paSampleFormatNotSupported;

            if( outputParameters->hostApiSpecificStreamInfo != NULL )
            {
                if( ((PaUtilHostApiSpecificStreamInfoHeader*)outputParameters->hostApiSpecificStreamInfo)->hostApiType
                        != (*hostApi)->info.type )
                    return paIncompatibleHostApiSpecificStreamInfo;
            }
        }

        if( (inputParameters != NULL) && (outputParameters != NULL) )
        {
             ensure that both devices use the same API
            if( inputHostApiIndex != outputHostApiIndex )
                return paBadIODeviceCombination;
        }
    }


     Check for absurd sample rates.
    if( (sampleRate < 1000.0) || (sampleRate > 200000.0) )
        return paInvalidSampleRate;

    if( ((streamFlags & ~paPlatformSpecificFlags) & ~(paClipOff | paDitherOff | paNeverDropInput | paPrimeOutputBuffersUsingStreamCallback ) ) != 0 )
        return paInvalidFlag;

    if( streamFlags & paNeverDropInput )
    {
         must be a callback stream
        if( !streamCallback )
             return paInvalidFlag;

         must be a full duplex stream
        if( (inputParameters == NULL) || (outputParameters == NULL) )
            return paInvalidFlag;

         must use paFramesPerBufferUnspecified
        if( framesPerBuffer != paFramesPerBufferUnspecified )
            return paInvalidFlag;
    }

    return paNoError;
}

 Basic stream initialization
static PaError InitializeStream( PaJackStream *stream, PaJackHostApiRepresentation *hostApi, int numInputChannels,
        int numOutputChannels )
{
    PaError result = paNoError;
    assert( stream );

    memset( stream, 0, sizeof (PaJackStream) );
    UNLESS( stream->stream_memory = PaUtil_CreateAllocationGroup(), paInsufficientMemory );


    stream->jack_client = hostApi->jack_client;
    stream->hostApi = hostApi;

    if( numInputChannels > 0 )
    {
        UNLESS( stream->local_input_ports =
                (jack_port_t**) PaUtil_GroupAllocateMemory( stream->stream_memory, sizeof(jack_port_t*) * numInputChannels ),
                paInsufficientMemory );
        memset( stream->local_input_ports, 0, sizeof(jack_port_t*) * numInputChannels );
        UNLESS( stream->remote_output_ports =
                (jack_port_t**) PaUtil_GroupAllocateMemory( stream->stream_memory, sizeof(jack_port_t*) * numInputChannels ),
                paInsufficientMemory );
        memset( stream->remote_output_ports, 0, sizeof(jack_port_t*) * numInputChannels );
    }


    if( numOutputChannels > 0 )
    {
        UNLESS( stream->local_output_ports =
                (jack_port_t**) PaUtil_GroupAllocateMemory( stream->stream_memory, sizeof(jack_port_t*) * numOutputChannels ),
                paInsufficientMemory );
        memset( stream->local_output_ports, 0, sizeof(jack_port_t*) * numOutputChannels );
        UNLESS( stream->remote_input_ports =
                (jack_port_t**) PaUtil_GroupAllocateMemory( stream->stream_memory, sizeof(jack_port_t*) * numOutputChannels ),
                paInsufficientMemory );
        memset( stream->remote_input_ports, 0, sizeof(jack_port_t*) * numOutputChannels );
    }

    stream->num_incoming_connections = numInputChannels;
    stream->num_outgoing_connections = numOutputChannels;

error:
    return result;
}
*/

/*
PaError OpenStream( PaStream** stream,
                       const PaStreamParameters *inputParameters,
                       const PaStreamParameters *outputParameters,
                       double sampleRate,
                       unsigned long framesPerBuffer,
                       PaStreamFlags streamFlags,
                       PaStreamCallback *streamCallback,
                       void *userData )
{
    PaError result;
    PaUtilHostApiRepresentation *hostApi = 0;
    PaDeviceIndex hostApiInputDevice = paNoDevice, hostApiOutputDevice = paNoDevice;
    PaStreamParameters hostApiOutputParameters;
    PaStreamParameters *hostApiInputParametersPtr, *hostApiOutputParametersPtr;

    result = ValidateOpenStreamParameters( inputParameters,
                                           outputParameters,
                                           sampleRate, framesPerBuffer,
                                           streamFlags, streamCallback,
                                           &hostApi,
                                           &hostApiInputDevice,
                                           &hostApiOutputDevice );
    if( result != paNoError )
    {
        PA_LOGAPI(("Pa_OpenStream returned:\n" ));
        PA_LOGAPI(("\t*(PaStream** stream): undefined\n" ));
        PA_LOGAPI(("\tPaError: %d ( %s )\n", result, Pa_GetErrorText( result ) ));
        return result;
    }


    PaStreamParameters hostApiInputParameters;
    if( inputParameters )
    {
        hostApiInputParameters.device = hostApiInputDevice;
        hostApiInputParameters.channelCount = inputParameters->channelCount;
        hostApiInputParameters.sampleFormat = inputParameters->sampleFormat;
        hostApiInputParameters.suggestedLatency = inputParameters->suggestedLatency;
        hostApiInputParameters.hostApiSpecificStreamInfo = inputParameters->hostApiSpecificStreamInfo;
        hostApiInputParametersPtr = &hostApiInputParameters;
    }
    else
    {
        hostApiInputParametersPtr = NULL;
    }

    if( outputParameters )
    {
        hostApiOutputParameters.device = hostApiOutputDevice;
        hostApiOutputParameters.channelCount = outputParameters->channelCount;
        hostApiOutputParameters.sampleFormat = outputParameters->sampleFormat;
        hostApiOutputParameters.suggestedLatency = outputParameters->suggestedLatency;
        hostApiOutputParameters.hostApiSpecificStreamInfo = outputParameters->hostApiSpecificStreamInfo;
        hostApiOutputParametersPtr = &hostApiOutputParameters;
    }
    else
    {
        hostApiOutputParametersPtr = NULL;
    }

    result = OpenStream( hostApi, stream,
                                  hostApiInputParametersPtr, hostApiOutputParametersPtr,
                                  sampleRate, framesPerBuffer, streamFlags, streamCallback, userData );

    PaStream** s = stream;
    const PaStreamParameters *inputParameters;
    const PaStreamParameters *outputParameters;
    double sampleRate;
    unsigned long framesPerBuffer;
    PaStreamFlags streamFlags;
    void *userData;

        PaError result = paNoError;
        PaJackHostApiRepresentation *jackHostApi = (PaJackHostApiRepresentation*)hostApi;
        PaJackStream *stream = NULL;

        char *port_string = PaUtil_GroupAllocateMemory( jackHostApi->deviceInfoMemory, jack_port_name_size() );
        unsigned long regexSz = jack_client_name_size() + 3;
        char *regex_pattern = PaUtil_GroupAllocateMemory( jackHostApi->deviceInfoMemory, regexSz );
        const char **jack_ports = NULL;
         int jack_max_buffer_size = jack_get_buffer_size( jackHostApi->jack_client );
        int i;
        int inputChannelCount, outputChannelCount;
        const double jackSr = jack_get_sample_rate( jackHostApi->jack_client );
        PaSampleFormat inputSampleFormat = 0, outputSampleFormat = 0;
        int bpInitialized = 0, srInitialized = 0;    Initialized buffer processor and stream representation?
        unsigned long ofs;

         validate platform specific flags
        if( (streamFlags & paPlatformSpecificFlags) != 0 )
            return paInvalidFlag;  unexpected platform specific flag
        if( (streamFlags & paPrimeOutputBuffersUsingStreamCallback) != 0 )
        {
            streamFlags &= ~paPrimeOutputBuffersUsingStreamCallback;
            return paInvalidFlag;    This implementation does not support buffer priming
        }

         Preliminary checks

        if( inputParameters )
        {
            inputChannelCount = inputParameters->channelCount;
            inputSampleFormat = inputParameters->sampleFormat;
        }
        else
        {
            inputChannelCount = 0;
        }

        if( outputParameters )
        {
            outputChannelCount = outputParameters->channelCount;
            outputSampleFormat = outputParameters->sampleFormat;
        }
        else
        {
            outputChannelCount = 0;
        }


        UNLESS( stream = (PaJackStream*)PaUtil_AllocateMemory( sizeof(PaJackStream) ), paInsufficientMemory );
        ENSURE_PA( InitializeStream( stream, jackHostApi, inputChannelCount, outputChannelCount ) );

        PaUtil_InitializeStreamRepresentation( &stream->streamRepresentation,
                                                   &jackHostApi->callbackStreamInterface, streamCallback, userData );
        srInitialized = 1;
        PaUtil_InitializeCpuLoadMeasurer( &stream->cpuLoadMeasurer, jackSr );

        * create the JACK ports.  We cannot connect them until audio
         * processing begins *

       * Register a unique set of ports for this stream
         * TODO: Robust allocation of new port names *

        ofs = jackHostApi->inputBase;
        for( i = 0; i < inputChannelCount; i++ )
        {
            snprintf( port_string, jack_port_name_size(), "in_%lu", ofs + i );
            UNLESS( stream->local_input_ports[i] = jack_port_register(
                  jackHostApi->jack_client, port_string,
                  JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0 ), paInsufficientMemory );
        }
        jackHostApi->inputBase += inputChannelCount;

        ofs = jackHostApi->outputBase;
        for( i = 0; i < outputChannelCount; i++ )
        {
            snprintf( port_string, jack_port_name_size(), "out_%lu", ofs + i );
            UNLESS( stream->local_output_ports[i] = jack_port_register(
                 jackHostApi->jack_client, port_string,
                 JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 ), paInsufficientMemory );
        }
        jackHostApi->outputBase += outputChannelCount;

        * look up the jack_port_t's for the remote ports.  We could do
         * this at stream start time, but doing it here ensures the
         * name lookup only happens once. *

        if( inputChannelCount > 0 )
        {
            int err = 0;

            * Get output ports of our capture device *
            snprintf( regex_pattern, regexSz, "%s:.*", hostApi->deviceInfos[ inputParameters->device ]->name );
            UNLESS( jack_ports = jack_get_ports( jackHostApi->jack_client, regex_pattern,
                                         NULL, JackPortIsOutput ), paUnanticipatedHostError );
            for( i = 0; i < inputChannelCount && jack_ports[i]; i++ )
            {
                if( (stream->remote_output_ports[i] = jack_port_by_name(
                     jackHostApi->jack_client, jack_ports[i] )) == NULL )
                {
                    err = 1;
                    break;
                }
            }
            free( jack_ports );
            UNLESS( !err, paInsufficientMemory );

            * Fewer ports than expected? *
            UNLESS( i == inputChannelCount, paInternalError );
        }

        if( outputChannelCount > 0 )
        {
            int err = 0;

            * Get input ports of our playback device *
            snprintf( regex_pattern, regexSz, "%s:.*", hostApi->deviceInfos[ outputParameters->device ]->name );
            UNLESS( jack_ports = jack_get_ports( jackHostApi->jack_client, regex_pattern,
                                         NULL, JackPortIsInput ), paUnanticipatedHostError );
            for( i = 0; i < outputChannelCount && jack_ports[i]; i++ )
            {
                if( (stream->remote_input_ports[i] = jack_port_by_name(
                     jackHostApi->jack_client, jack_ports[i] )) == 0 )
                {
                    err = 1;
                    break;
                }
            }
            free( jack_ports );
            UNLESS( !err , paInsufficientMemory );

             Fewer ports than expected? *
            UNLESS( i == outputChannelCount, paInternalError );
        }

        ENSURE_PA( PaUtil_InitializeBufferProcessor(
                      &stream->bufferProcessor,
                      inputChannelCount,
                      inputSampleFormat,
                      paFloat32 | paNonInterleaved,  hostInputSampleFormat
                      outputChannelCount,
                      outputSampleFormat,
                      paFloat32 | paNonInterleaved,  hostOutputSampleFormat
                      jackSr,
                      streamFlags,
                      framesPerBuffer,
                      0,                             Ignored
                      paUtilUnknownHostBufferSize,   Buffer size may vary on JACK's discretion *
                      streamCallback,
                      userData ) );
        bpInitialized = 1;

        if( stream->num_incoming_connections > 0 )
            stream->streamRepresentation.streamInfo.inputLatency = (jack_port_get_latency( stream->remote_output_ports[0] )
                    - jack_get_buffer_size( jackHostApi->jack_client )  / One buffer is not counted as latency *
                + PaUtil_GetBufferProcessorInputLatencyFrames( &stream->bufferProcessor )) / sampleRate;
        if( stream->num_outgoing_connections > 0 )
            stream->streamRepresentation.streamInfo.outputLatency = (jack_port_get_latency( stream->remote_input_ports[0] )
                    - jack_get_buffer_size( jackHostApi->jack_client )  / One buffer is not counted as latency/
                + PaUtil_GetBufferProcessorOutputLatencyFrames( &stream->bufferProcessor )) / sampleRate;

        stream->streamRepresentation.streamInfo.sampleRate = jackSr;
        stream->t0 = jack_frame_time( jackHostApi->jack_client );    A: Time should run from Pa_OpenStream

         Add to queue of opened streams
        ENSURE_PA( AddStream( stream ) );

        *s = (PaStream*)stream;

        return result;

    error:
        if( stream )
            CleanUpStream( stream, srInitialized, bpInitialized );

        return result;
    }


    if( result == paNoError ) {
        AddOpenStream( *stream );
        PA_STREAM_REP(*stream)->hostApiType = hostApi->info.type;
    }


    PA_LOGAPI(("Pa_OpenStream returned:\n" ));
    PA_LOGAPI(("\t*(PaStream** stream): 0x%p\n", *stream ));
    PA_LOGAPI(("\tPaError: %d ( %s )\n", result, Pa_GetErrorText( result ) ));

    return result;
}*/

} // anonymous namespace



SoundDeviceJack::SoundDeviceJack(UserSettingsPointer config,
                                           SoundManager *sm,
                                           SoundManagerJack *smj,
                                           const JackDeviceInfo& deviceInfo)
        : SoundDevice(config, sm),
          m_pSoundManagerJack(smj),
          m_pStream(NULL),
          m_deviceInfo(deviceInfo),
          m_outputFifo(NULL),
          m_inputFifo(NULL),
          m_outputDrift(false),
          m_inputDrift(false),
          m_bSetThreadPriority(false),
          m_underflowUpdateCount(0),
          m_framesSinceAudioLatencyUsageUpdate(0),
          m_syncBuffers(2),
          m_invalidTimeInfoWarned(false) {
    // Setting parent class members:
    m_hostAPI = MIXXX_PORTAUDIO_JACK_STRING;
    m_dSampleRate = static_cast<double>(deviceInfo.sampleRate);
    m_strInternalName = deviceInfo.name;
    m_strDisplayName = deviceInfo.name;
    m_iNumInputChannels = deviceInfo.inputPorts.count();
    m_iNumOutputChannels = deviceInfo.outputPorts.count();

    m_pMasterAudioLatencyOverloadCount = new ControlObjectSlave("[Master]",
            "audio_latency_overload_count");
    m_pMasterAudioLatencyUsage = new ControlObjectSlave("[Master]",
            "audio_latency_usage");
    m_pMasterAudioLatencyOverload = new ControlObjectSlave("[Master]",
            "audio_latency_overload");
}

SoundDeviceJack::~SoundDeviceJack() {
    delete m_pMasterAudioLatencyOverloadCount;
    delete m_pMasterAudioLatencyUsage;
    delete m_pMasterAudioLatencyOverload;
}

Result SoundDeviceJack::open(bool isClkRefDevice, int syncBuffers) {
    qDebug() << "SoundDeviceJack::open()" << getInternalName();

    Q_UNUSED (syncBuffers)
    // Jack does not support multiple soundcards by default
    // See http://jackaudio.org/faq/multiple_devices.html
    Q_UNUSED (isClkRefDevice)
    // The jackd is always the clock reference
    // the callback arrives in SoundManagerJack

    DEBUG_ASSERT_AND_HANDLE(!m_audioOutputs.empty() || !m_audioInputs.empty()) {
        m_lastError =
                "No inputs or outputs in SoundDeviceJack::open() "
                "(THIS IS A BUG, this should be filtered by SM::setupDevices)";
        return ERR;
    }

    for (const auto& out: m_audioOutputs) {
        m_pSoundManagerJack->connectOutputPorts(m_deviceInfo.name,
                                                m_deviceInfo.inputPorts,
                                                out,
                                                true);
    }
    for (const auto& in: m_audioInputs) {
        m_pSoundManagerJack->connectInputPorts(m_deviceInfo.name,
                                               m_deviceInfo.outputPorts,
                                               in,
                                               true);
    }

    /*
    // Get latency in milliseconds
    qDebug() << "framesPerBuffer:" << m_framesPerBuffer;
    double bufferMSec = m_framesPerBuffer / m_dSampleRate * 1000;
    qDebug() << "Requested sample rate: " << m_dSampleRate << "Hz, latency:"
             << bufferMSec << "ms";

    qDebug() << "Output channels:" << m_outputParams.channelCount
             << "| Input channels:"
             << m_inputParams.channelCount;

    // PortAudio's JACK backend also only properly supports
    // paFramesPerBufferUnspecified in non-blocking mode because the latency
    // comes from the JACK daemon. (PA should give an error or something though,
    // but it doesn't.)
    m_framesPerBuffer = paFramesPerBufferUnspecified;

    //Fill out the rest of the info.
    m_outputParams.sampleFormat = paFloat32;
    m_outputParams.suggestedLatency = bufferMSec / 1000.0;
    m_outputParams.hostApiSpecificStreamInfo = NULL;

    m_inputParams.sampleFormat  = paFloat32;
    m_inputParams.suggestedLatency = bufferMSec / 1000.0;
    m_inputParams.hostApiSpecificStreamInfo = NULL;

    m_syncBuffers = syncBuffers;

    // Create the callback function pointer.
    PaStreamCallback* callback = NULL;
    if (isClkRefDevice) {
        callback = paV19CallbackClkRef;
    } else if (m_syncBuffers == 2) { // "Default (long delay)"
        callback = paV19CallbackDrift;
        // to avoid overflows when one callback overtakes the other or
        // when there is a clock drift compared to the clock reference device
        // we need an additional artificial delay
        if (m_outputParams.channelCount) {
            // On chunk for reading one for writing and on for drift correction
            m_outputFifo = new FIFO<CSAMPLE>(
                    m_outputParams.channelCount * m_framesPerBuffer
                            * kFifoSize);
            // Clear first 1.5 chunks on for the required artificial delaly to
            // a allow jitter and a half, because we can't predict which
            // callback fires first.
            m_outputFifo->releaseWriteRegions(
                    m_outputParams.channelCount * m_framesPerBuffer * kFifoSize
                            / 2);
        }
        if (m_inputParams.channelCount) {
            m_inputFifo = new FIFO<CSAMPLE>(
                    m_inputParams.channelCount * m_framesPerBuffer * kFifoSize);
            // Clear first two 1.5 chunks (see above)
            m_inputFifo->releaseWriteRegions(
                    m_inputParams.channelCount * m_framesPerBuffer * kFifoSize
                            / 2);
        }
    } else if (m_syncBuffers == 1) { // "Disabled (short delay)"
        // this can be used on a second device when it is driven by the Clock
        // reference device clock
        callback = paV19Callback;
        if (m_outputParams.channelCount) {
            m_outputFifo = new FIFO<CSAMPLE>(
                    m_outputParams.channelCount * m_framesPerBuffer);
        }
        if (m_inputParams.channelCount) {
            m_inputFifo = new FIFO<CSAMPLE>(
                    m_inputParams.channelCount * m_framesPerBuffer);
        }
    } else if (m_syncBuffers == 0) { // "Experimental (no delay)"
        if (m_outputParams.channelCount) {
            m_outputFifo = new FIFO<CSAMPLE>(
                    m_outputParams.channelCount * m_framesPerBuffer * 2);
        }
        if (m_inputParams.channelCount) {
            m_inputFifo = new FIFO<CSAMPLE>(
                    m_inputParams.channelCount * m_framesPerBuffer * 2);
        }
    }

    PaStream *pStream;
    // Try open device using iChannelMax

    err = OpenStream(&pStream,
                        pInputParams,
                        pOutputParams,
                        m_dSampleRate,
                        m_framesPerBuffer,
                        paClipOff, // Stream flags
                        callback,
                        (void*) this); // pointer passed to the callback function

    if (err != paNoError) {
        qWarning() << "Error opening stream:" << Pa_GetErrorText(err);
        m_lastError = QString::fromUtf8(Pa_GetErrorText(err));
        return ERR;
    } else {
        qDebug() << "Opened PortAudio stream successfully... starting";
    }


    // Start stream
    err = Pa_StartStream(pStream);
    if (err != paNoError) {
        qWarning() << "PortAudio: Start stream error:" << Pa_GetErrorText(err);
        m_lastError = QString::fromUtf8(Pa_GetErrorText(err));
        err = Pa_CloseStream(pStream);
        if (err != paNoError) {
            qWarning() << "PortAudio: Close stream error:"
                       << Pa_GetErrorText(err) << getInternalName();
        }
        return ERR;
    } else {
        qDebug() << "PortAudio: Started stream successfully";
    }

    // Get the actual details of the stream & update Mixxx's data
    const PaStreamInfo* streamDetails = Pa_GetStreamInfo(pStream);
    m_dSampleRate = streamDetails->sampleRate;
    double currentLatencyMSec = streamDetails->outputLatency * 1000;
    qDebug() << "   Actual sample rate: " << m_dSampleRate << "Hz, latency:"
             << currentLatencyMSec << "ms";

    if (isClkRefDevice) {
        // Update the samplerate and latency ControlObjects, which allow the
        // waveform view to properly correct for the latency.
        ControlObject::set(ConfigKey("[Master]", "latency"), currentLatencyMSec);
        ControlObject::set(ConfigKey("[Master]", "samplerate"), m_dSampleRate);
        ControlObject::set(ConfigKey("[Master]", "audio_buffer_size"), bufferMSec);

        if (m_pMasterAudioLatencyOverloadCount) {
            m_pMasterAudioLatencyOverloadCount->set(0);
        }
    }
    m_pStream = pStream;
    */
    return OK;
}

bool SoundDeviceJack::isOpen() const {
    return m_pStream != NULL;
}

Result SoundDeviceJack::close() {
    qDebug() << "SoundDeviceJack::close()" << getInternalName();

    for (const auto& out: m_audioOutputs) {
        m_pSoundManagerJack->connectOutputPorts(m_deviceInfo.name,
                                                m_deviceInfo.inputPorts,
                                                out,
                                                false);
    }
    for (const auto& in: m_audioInputs) {
        m_pSoundManagerJack->connectInputPorts(m_deviceInfo.name,
                                               m_deviceInfo.outputPorts,
                                               in,
                                               false);
    }

    /*

    PaStream* pStream = m_pStream;
    m_pStream = NULL;
    if (pStream) {
        // Make sure the stream is not stopped before we try stopping it.
        PaError err = Pa_IsStreamStopped(pStream);
        // 1 means the stream is stopped. 0 means active.
        if (err == 1) {
            //qDebug() << "PortAudio: Stream already stopped, but no error.";
            return OK;
        }
        // Real PaErrors are always negative.
        if (err < 0) {
            qWarning() << "PortAudio: Stream already stopped:"
                       << Pa_GetErrorText(err) << getInternalName();
            return ERR;
        }

        //Stop the stream.
        err = Pa_StopStream(pStream);
        //PaError err = Pa_AbortStream(m_pStream); //Trying Pa_AbortStream instead, because StopStream seems to wait
                                                   //until all the buffers have been flushed, which can take a
                                                   //few (annoying) seconds when you're doing soundcard input.
                                                   //(it flushes the input buffer, and then some, or something)
                                                   //BIG FAT WARNING: Pa_AbortStream() will kill threads while they're
                                                   //waiting on a mutex, which will leave the mutex in an screwy
                                                   //state. Don't use it!

        if (err != paNoError) {
            qWarning() << "PortAudio: Stop stream error:"
                       << Pa_GetErrorText(err) << getInternalName();
            return ERR;
        }

        // Close stream
        err = Pa_CloseStream(pStream);
        if (err != paNoError) {
            qWarning() << "PortAudio: Close stream error:"
                       << Pa_GetErrorText(err) << getInternalName();
            return ERR;
        }

        if (m_outputFifo) {
            delete m_outputFifo;
        }
        if (m_inputFifo) {
            delete m_inputFifo;
        }
    }

    m_outputFifo = NULL;
    m_inputFifo = NULL;
    m_bSetThreadPriority = false;

*/
    return OK;
}

void SoundDeviceJack::readProcess() {
    /*
    PaStream* pStream = m_pStream;
    if (pStream && m_inputParams.channelCount && m_inputFifo) {
        int inChunkSize = m_framesPerBuffer * m_inputParams.channelCount;
        if (m_syncBuffers == 0) { // "Experimental (no delay)"
            // Polling mode
            signed int readAvailable = Pa_GetStreamReadAvailable(pStream) * m_inputParams.channelCount;
            int writeAvailable = m_inputFifo->writeAvailable();
            int copyCount = qMin(writeAvailable, readAvailable);
            if (copyCount > 0) {
                CSAMPLE* dataPtr1;
                ring_buffer_size_t size1;
                CSAMPLE* dataPtr2;
                ring_buffer_size_t size2;
                (void)m_inputFifo->aquireWriteRegions(copyCount,
                        &dataPtr1, &size1, &dataPtr2, &size2);
                // Fetch fresh samples and write to the the input buffer
                PaError err = Pa_ReadStream(pStream, dataPtr1,
                        size1 / m_inputParams.channelCount);
                CSAMPLE* lastFrame = &dataPtr1[size1 - m_inputParams.channelCount];
                if (err == paInputOverflowed) {
                    //qDebug() << "SoundDevicePortAudio::readProcess() Pa_ReadStream paInputOverflowed" << getInternalName();
                    m_underflowHappened = 1;
                }
                if (size2 > 0) {
                    PaError err = Pa_ReadStream(pStream, dataPtr2,
                            size2 / m_inputParams.channelCount);
                    lastFrame = &dataPtr2[size2 - m_inputParams.channelCount];
                    if (err == paInputOverflowed) {
                        //qDebug() << "SoundDevicePortAudio::readProcess() Pa_ReadStream paInputOverflowed" << getInternalName();
                        m_underflowHappened = 1;
                    }
                }
                m_inputFifo->releaseWriteRegions(copyCount);

                if (readAvailable > writeAvailable + inChunkSize / 2) {
                    // we are not able to consume all frames
                    if (m_inputDrift) {
                        // Skip one frame
                        //qDebug() << "SoundDevicePortAudio::readProcess() skip one frame"
                        //        << (float)writeAvailable / inChunkSize << (float)readAvailable / inChunkSize;
                        PaError err = Pa_ReadStream(pStream, dataPtr1, 1);
                        if (err == paInputOverflowed) {
                            //qDebug()
                            //        << "SoundDevicePortAudio::readProcess() Pa_ReadStream paInputOverflowed"
                            //        << getInternalName();
                            m_underflowHappened = 1;
                        }
                    } else {
                        m_inputDrift = true;
                    }
                } else if (readAvailable < inChunkSize / 2) {
                    // We should read at least inChunkSize
                    if (m_inputDrift) {
                        // duplicate one frame
                        //qDebug() << "SoundDevicePortAudio::readProcess() duplicate one frame"
                        //        << (float)writeAvailable / inChunkSize << (float)readAvailable / inChunkSize;
                        (void) m_inputFifo->aquireWriteRegions(
                                m_inputParams.channelCount, &dataPtr1, &size1,
                                &dataPtr2, &size2);
                        if (size1) {
                            SampleUtil::copy(dataPtr1, lastFrame, size1);
                            m_inputFifo->releaseWriteRegions(size1);
                        }
                    } else {
                        m_inputDrift = true;
                    }
                } else {
                    m_inputDrift = false;
                }
            }
        }

        int readAvailable = m_inputFifo->readAvailable();
        int readCount = inChunkSize;
        if (inChunkSize > readAvailable) {
            readCount = readAvailable;
            m_underflowHappened = 1;
            //qDebug() << "readProcess()" << (float)readAvailable / inChunkSize << "underflow";
        }
        if (readCount) {
            CSAMPLE* dataPtr1;
            ring_buffer_size_t size1;
            CSAMPLE* dataPtr2;
            ring_buffer_size_t size2;
            // We use size1 and size2, so we can ignore the return value
            (void) m_inputFifo->aquireReadRegions(readCount, &dataPtr1, &size1,
                    &dataPtr2, &size2);
            // Fetch fresh samples and write to the the output buffer
            composeInputBuffer(dataPtr1,
                    size1 / m_inputParams.channelCount, 0,
                    m_inputParams.channelCount);
            if (size2 > 0) {
                composeInputBuffer(dataPtr2,
                        size2 / m_inputParams.channelCount,
                        size1 / m_inputParams.channelCount,
                        m_inputParams.channelCount);
            }
            m_inputFifo->releaseReadRegions(readCount);
        }
        if (readCount < inChunkSize) {
            // Fill remaining buffers with zeros
            clearInputBuffer(inChunkSize - readCount, readCount);
        }

        m_pSoundManager->pushInputBuffers(m_audioInputs, m_framesPerBuffer);
    }
    */
}

void SoundDeviceJack::writeProcess() {
    /*
    PaStream* pStream = m_pStream;

    if (pStream && m_outputParams.channelCount && m_outputFifo) {
        int outChunkSize = m_framesPerBuffer * m_outputParams.channelCount;
        int writeAvailable = m_outputFifo->writeAvailable();
        int writeCount = outChunkSize;
        if (outChunkSize > writeAvailable) {
            writeCount = writeAvailable;
            m_underflowHappened = 1;
            //qDebug() << "writeProcess():" << (float) writeAvailable / outChunkSize << "Overflow";
        }
        if (writeCount) {
            CSAMPLE* dataPtr1;
            ring_buffer_size_t size1;
            CSAMPLE* dataPtr2;
            ring_buffer_size_t size2;
            // We use size1 and size2, so we can ignore the return value
            (void) m_outputFifo->aquireWriteRegions(writeCount, &dataPtr1,
                    &size1, &dataPtr2, &size2);
            // Fetch fresh samples and write to the the output buffer
            composeOutputBuffer(dataPtr1, size1 / m_outputParams.channelCount, 0,
                    static_cast<unsigned int>(m_outputParams.channelCount));
            if (size2 > 0) {
                composeOutputBuffer(dataPtr2,
                        size2 / m_outputParams.channelCount,
                        size1 / m_outputParams.channelCount,
                        static_cast<unsigned int>(m_outputParams.channelCount));
            }
            m_outputFifo->releaseWriteRegions(writeCount);
        }

        if (m_syncBuffers == 0) { // "Experimental (no delay)"
            // Polling
            signed int writeAvailable = Pa_GetStreamWriteAvailable(pStream)
                    * m_outputParams.channelCount;
            int readAvailable = m_outputFifo->readAvailable();
            int copyCount = qMin(readAvailable, writeAvailable);
            //qDebug() << "SoundDevicePortAudio::writeProcess()" << toRead << writeAvailable;
            if (copyCount > 0) {
                CSAMPLE* dataPtr1;
                ring_buffer_size_t size1;
                CSAMPLE* dataPtr2;
                ring_buffer_size_t size2;
                m_outputFifo->aquireReadRegions(copyCount,
                        &dataPtr1, &size1, &dataPtr2, &size2);
                if (writeAvailable == outChunkSize * 2) {
                    // Underflow
                    //qDebug() << "SoundDevicePortAudio::writeProcess() Buffer empty";
                    // fill buffer duplicate one sample
                    for (int i = 0; i < writeAvailable - copyCount; i +=
                            m_outputParams.channelCount) {
                        Pa_WriteStream(pStream, dataPtr1, 1);
                    }
                    m_underflowHappened = 1;
                } else if (writeAvailable > readAvailable + outChunkSize / 2) {
                    // try to keep PAs buffer filled up to 0.5 chunks
                    if (m_outputDrift) {
                        // duplicate one frame
                        //qDebug() << "SoundDevicePortAudio::writeProcess() duplicate one frame"
                        //        << (float)writeAvailable / outChunkSize << (float)readAvailable / outChunkSize;
                        PaError err = Pa_WriteStream(pStream, dataPtr1, 1);
                        if (err == paOutputUnderflowed) {
                            //qDebug() << "SoundDevicePortAudio::writeProcess() Pa_ReadStream paOutputUnderflowed";
                            m_underflowHappened = 1;
                        }
                    } else {
                        //qDebug() << "SoundDevicePortAudio::writeProcess() OK" << (float)writeAvailable / outChunkSize << (float)readAvailable / outChunkSize;
                        m_outputDrift = true;
                    }
                } else if (writeAvailable < outChunkSize / 2) {
                    // We are not able to store all new frames
                    if (m_outputDrift) {
                        //qDebug() << "SoundDevicePortAudio::writeProcess() skip one frame"
                        //        << (float)writeAvailable / outChunkSize << (float)readAvailable / outChunkSize;
                        ++copyCount;
                    } else {
                        m_outputDrift = true;
                    }
                } else {
                    m_outputDrift = false;
                }
                PaError err = Pa_WriteStream(pStream, dataPtr1,
                        size1 / m_outputParams.channelCount);
                if (err == paOutputUnderflowed) {
                    //qDebug() << "SoundDevicePortAudio::writeProcess() Pa_ReadStream paOutputUnderflowed" << getInternalName();
                    m_underflowHappened = 1;
                }
                if (size2 > 0) {
                    PaError err = Pa_WriteStream(pStream, dataPtr2,
                            size2 / m_outputParams.channelCount);
                    if (err == paOutputUnderflowed) {
                        //qDebug() << "SoundDevicePortAudio::writeProcess() Pa_WriteStream paOutputUnderflowed" << getInternalName();
                        m_underflowHappened = 1;
                    }
                }
                m_outputFifo->releaseReadRegions(copyCount);
            }
        }
    }
    */
}

int SoundDeviceJack::callbackProcessDrift(
        const unsigned int framesPerBuffer, CSAMPLE *out, const CSAMPLE *in,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags) {

    /*
    Q_UNUSED(timeInfo);
    Trace trace("SoundDevicePortAudio::callbackProcessDrift %1",
            getInternalName());

    if (statusFlags & (paOutputUnderflow | paInputOverflow)) {
        m_underflowHappened = 1;
    }

    // Since we are on the non Clock reference device and may have an independent
    // Crystal clock, a drift correction is required
    //
    // There is a delay of up to one latency between composing a chunk in the Clock
    // Reference callback and write it to the device. So we need at lest one buffer.
    // Unfortunately this delay is somehow random, an WILL produce a delay slow
    // shift without we can avoid it. (Thats the price for using a cheap USB soundcard).
    //
    // Additional we need an filled chunk and an empty chunk. These are used when on
    // sound card overtakes the other. This always happens, if they are driven form
    // two crystals. In a test case every 30 s @ 23 ms. After they are consumed,
    // the drift correction takes place and fills or clears the reserve buffers.
    // If this is finished before an other overtake happens, we do not face any
    // dropouts or clicks.
    // So thats why we need a Fifo of 3 chunks.
    //
    // In addition there is a jitter effect. It happens that one callback is delayed,
    // in this case the second one fires two times and then the first one fires two
    // time as well to catch up. This is also fixed by the additional buffers. If this
    // happens just after an regular overtake, we will have clicks again.
    //
    // I the tests it turns out that it only happens in the opposite direction, so
    // 3 chunks are just fine.

    if (m_inputParams.channelCount) {
        int inChunkSize = framesPerBuffer * m_inputParams.channelCount;
        int readAvailable = m_inputFifo->readAvailable();
        int writeAvailable = m_inputFifo->writeAvailable();
        if (readAvailable < inChunkSize * kDriftReserve) {
            // risk of an underflow, duplicate one frame
            m_inputFifo->write(in, inChunkSize);
            if (m_inputDrift) {
                // Do not compensate the first delay, because it is likely a jitter
                // corrected in the next cycle
                // Duplicate one frame
                m_inputFifo->write(
                        &in[inChunkSize - m_inputParams.channelCount],
                        m_inputParams.channelCount);
                //qDebug() << "callbackProcessDrift write:" << (float)readAvailable / inChunkSize << "Skip";
            } else {
                m_inputDrift = true;
                //qDebug() << "callbackProcessDrift write:" << (float)readAvailable / inChunkSize << "Jitter Skip";
            }
        } else if (readAvailable == inChunkSize * kDriftReserve) {
            // Everything Ok
            m_inputFifo->write(in, inChunkSize);
            m_inputDrift = false;
            //qDebug() << "callbackProcess write:" << (float) readAvailable / inChunkSize << "Normal";
        } else if (writeAvailable >= inChunkSize) {
            // Risk of overflow, skip one frame
            if (m_inputDrift) {
                m_inputFifo->write(in, inChunkSize - m_inputParams.channelCount);
                //qDebug() << "callbackProcessDrift write:" << (float)readAvailable / inChunkSize << "Skip";
            } else {
                m_inputFifo->write(in, inChunkSize);
                m_inputDrift = true;
                //qDebug() << "callbackProcessDrift write:" << (float)readAvailable / inChunkSize << "Jitter Skip";
            }
        } else if (writeAvailable) {
            // Fifo Overflow
            m_inputFifo->write(in, writeAvailable);
            m_underflowHappened = 1;
            //qDebug() << "callbackProcessDrift write:" << (float) readAvailable / inChunkSize << "Overflow";
        } else {
            // Buffer full
            m_underflowHappened = 1;
            //qDebug() << "callbackProcessDrift write:" << (float) readAvailable / inChunkSize << "Buffer full";
        }
    }

    if (m_outputParams.channelCount) {
        int outChunkSize = framesPerBuffer * m_outputParams.channelCount;
        int readAvailable = m_outputFifo->readAvailable();

        if (readAvailable > outChunkSize * (kDriftReserve + 1)) {
            m_outputFifo->read(out, outChunkSize);
            if (m_outputDrift) {
                // Risk of overflow, skip one frame
                m_outputFifo->releaseReadRegions(m_outputParams.channelCount);
                //qDebug() << "callbackProcessDrift read:" << (float)readAvailable / outChunkSize << "Skip";
            } else {
                m_outputDrift = true;
                //qDebug() << "callbackProcessDrift read:" << (float)readAvailable / outChunkSize << "Jitter Skip";
            }
        } else if (readAvailable == outChunkSize * (kDriftReserve + 1)) {
            m_outputFifo->read(out,outChunkSize);
            m_outputDrift = false;
            //qDebug() << "callbackProcessDrift read:" << (float)readAvailable / outChunkSize << "Normal";
        } else if (readAvailable >= outChunkSize) {
            if (m_outputDrift) {
                // Risk of underflow, duplicate one frame
                m_outputFifo->read(out,
                        outChunkSize - m_outputParams.channelCount);
                SampleUtil::copy(
                        &out[outChunkSize - m_outputParams.channelCount],
                        &out[outChunkSize - (2 * m_outputParams.channelCount)],
                        m_outputParams.channelCount);
                //qDebug() << "callbackProcessDrift read:" << (float)readAvailable / outChunkSize << "Save";
            } else {
                m_outputFifo->read(out, outChunkSize);
                m_outputDrift = true;
                //qDebug() << "callbackProcessDrift read:" << (float)readAvailable / outChunkSize << "Jitter Save";
            }
        } else if (readAvailable) {
            m_outputFifo->read(out,
                    readAvailable);
            // underflow
            SampleUtil::clear(&out[readAvailable],
                    outChunkSize - readAvailable);
            m_underflowHappened = 1;
            //qDebug() << "callbackProcessDrift read:" << (float)readAvailable / outChunkSize << "Underflow";
        } else {
            // underflow
            SampleUtil::clear(out, outChunkSize);
            m_underflowHappened = 1;
            //qDebug() << "callbackProcess read:" << (float)readAvailable / outChunkSize << "Buffer empty";
        }
     }
     */
    return paContinue;
}

int SoundDeviceJack::callbackProcess(const unsigned int framesPerBuffer,
        CSAMPLE *out, const CSAMPLE *in,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags) {
    /*
    Q_UNUSED(timeInfo);
    Trace trace("SoundDevicePortAudio::callbackProcess %1", getInternalName());

    if (statusFlags & (paOutputUnderflow | paInputOverflow)) {
        m_underflowHappened = 1;
        //qDebug() << "callbackProcess read:" << "Underflow";

    }

    if (m_inputParams.channelCount) {
        int inChunkSize = framesPerBuffer * m_inputParams.channelCount;
        int writeAvailable = m_inputFifo->writeAvailable();
        if (writeAvailable >= inChunkSize) {
            m_inputFifo->write(in, inChunkSize - m_inputParams.channelCount);
        } else if (writeAvailable) {
            // Fifo Overflow
            m_inputFifo->write(in, writeAvailable);
            m_underflowHappened = 1;
            //qDebug() << "callbackProcess write:" << "Overflow";
        } else {
            // Buffer full
            m_underflowHappened = 1;
            //qDebug() << "callbackProcess write:" << "Buffer full";
        }
    }

    if (m_outputParams.channelCount) {
        int outChunkSize = framesPerBuffer * m_outputParams.channelCount;
        int readAvailable = m_outputFifo->readAvailable();
        if (readAvailable >= outChunkSize) {
            m_outputFifo->read(out, outChunkSize);
        } else if (readAvailable) {
            m_outputFifo->read(out,
                    readAvailable);
            // underflow
            SampleUtil::clear(&out[readAvailable],
                    outChunkSize - readAvailable);
            m_underflowHappened = 1;
            //qDebug() << "callbackProcess read:" << "Underflow";
        } else {
            // underflow
            SampleUtil::clear(out, outChunkSize);
            m_underflowHappened = 1;
            //qDebug() << "callbackProcess read:" << "Buffer empty";
        }
     }
     */
    return paContinue;
}

int SoundDeviceJack::callbackProcessClkRef(
        const unsigned int framesPerBuffer, CSAMPLE *out, const CSAMPLE *in,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags) {

    /*
    // This must be the very first call, else timeInfo becomes invalid
    m_clkRefTimer.start();
    updateCallbackEntryToDacTime(timeInfo);

    Trace trace("SoundDevicePortAudio::callbackProcessClkRef %1",
                getInternalName());

    //qDebug() << "SoundDevicePortAudio::callbackProcess:" << getInternalName();
    // Turn on TimeCritical priority for the callback thread. If we are running
    // in Linux userland, for example, this will have no effect.
    if (!m_bSetThreadPriority) {
        QThread::currentThread()->setPriority(QThread::TimeCriticalPriority);
        m_bSetThreadPriority = true;


#ifdef __SSE__
        // This disables the denormals calculations, to avoid a
        // performance penalty of ~20
        // https://bugs.launchpad.net/mixxx/+bug/1404401
        if (!_MM_GET_DENORMALS_ZERO_MODE()) {
            qDebug() << "SSE: Enabling denormals to zero mode";
            _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
        } else {
             qDebug() << "SSE: Denormals to zero mode already enabled";
        }

        if (!_MM_GET_FLUSH_ZERO_MODE()) {
            qDebug() << "SSE: Enabling flush to zero mode";
            _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        } else {
             qDebug() << "SSE: Flush to zero mode already enabled";
        }
#else
#if defined( __i386__ ) || defined( __i486__ ) || defined( __i586__ ) || \
         defined( __i686__ ) || defined( __x86_64__ ) || defined (_M_I86)
        qWarning() << "No SSE: No denormals to zero mode available. EQs and effects may suffer high CPU load";
#endif
#endif
        // verify if flush to zero or denormals to zero works
        // test passes if one of the two flag is set.
        volatile double doubleMin = DBL_MIN; // the smallest normalized double
        DEBUG_ASSERT_AND_HANDLE(doubleMin / 2 == 0.0) {
            qWarning() << "Denormals to zero mode is not working. EQs and effects may suffer high CPU load";
        } else {
            qDebug() << "Denormals to zero mode is working";
        }
    }

#ifdef __SSE__
#ifdef __WINDOWS__
    // We need to refresh the denormals flags every callback since some
    // driver + API combinations will reset them (known: DirectSound + Realtec)
    // Fixes Bug #1495047
    // (Both calls are very fast)
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
#endif

    if (statusFlags & (paOutputUnderflow | paInputOverflow)) {
        m_underflowHappened = true;
    }

    if (m_underflowUpdateCount == 0) {
        if (m_underflowHappened) {
            m_pMasterAudioLatencyOverload->set(1.0);
            m_pMasterAudioLatencyOverloadCount->set(
                    m_pMasterAudioLatencyOverloadCount->get() + 1);
            m_underflowUpdateCount = CPU_OVERLOAD_DURATION * m_dSampleRate
                    / framesPerBuffer / 1000;
            m_underflowHappened = 0; // resetting here is not thread safe,
                                     // but that is OK, because we count only
                                     // 1 underflow each 500 ms
        } else {
            m_pMasterAudioLatencyOverload->set(0.0);
        }
    } else {
        --m_underflowUpdateCount;
    }

    m_framesSinceAudioLatencyUsageUpdate += framesPerBuffer;
    if (m_framesSinceAudioLatencyUsageUpdate
            > (m_dSampleRate / CPU_USAGE_UPDATE_RATE)) {
        double secInAudioCb = m_timeInAudioCallback.toDoubleSeconds();
        m_pMasterAudioLatencyUsage->set(secInAudioCb /
                (m_framesSinceAudioLatencyUsageUpdate / m_dSampleRate));
        m_timeInAudioCallback = mixxx::Duration::fromSeconds(0);
        m_framesSinceAudioLatencyUsageUpdate = 0;
        //qDebug() << m_pMasterAudioLatencyUsage
        //         << m_pMasterAudioLatencyUsage->get();
    }

    //Note: Input is processed first so that any ControlObject changes made in
    //      response to input are processed as soon as possible (that is, when
    //      m_pSoundManager->requestBuffer() is called below.)

    // Send audio from the soundcard's input off to the SoundManager...
    if (in) {
        ScopedTimer t("SoundDevicePortAudio::callbackProcess input %1",
                getInternalName());
        composeInputBuffer(in, framesPerBuffer, 0,
                           m_inputParams.channelCount);
        m_pSoundManager->pushInputBuffers(m_audioInputs, m_framesPerBuffer);
    }

    m_pSoundManager->readProcess();

    {
        ScopedTimer t("SoundDevicePortAudio::callbackProcess prepare %1",
                getInternalName());
        m_pSoundManager->onDeviceOutputCallback(framesPerBuffer);
    }

    if (out) {
        ScopedTimer t("SoundDevicePortAudio::callbackProcess output %1",
                getInternalName());

        if (m_outputParams.channelCount <= 0) {
            qWarning()
                    << "SoundDevicePortAudio::callbackProcess m_outputParams channel count is zero or less:"
                    << m_outputParams.channelCount;
            // Bail out.
            return paContinue;
        }

        composeOutputBuffer(out, framesPerBuffer, 0, static_cast<unsigned int>(
                m_outputParams.channelCount));
    }

    m_pSoundManager->writeProcess();

    m_timeInAudioCallback += m_clkRefTimer.elapsed();
    */
    return paContinue;
}

void SoundDeviceJack::updateCallbackEntryToDacTime(
        const PaStreamCallbackTimeInfo* timeInfo) {
    PaTime callbackEntrytoDacSecs = -1;
    if (timeInfo->outputBufferDacTime > 0) {
        callbackEntrytoDacSecs = timeInfo->outputBufferDacTime
                - timeInfo->currentTime;
    }
    double bufferSizeSec = m_framesPerBuffer / m_dSampleRate;

    if (callbackEntrytoDacSecs < 0 ||
            callbackEntrytoDacSecs  > bufferSizeSec * 2) {
        // m_timeInfo Invalid, Audio API broken
        if (!m_invalidTimeInfoWarned) {
            qWarning() << "SoundDevicePortAudio: Audio API provides invalid time stamps,"
                       << "waveform syncing disabled."
                       << "DacTime:" << timeInfo->outputBufferDacTime
                       << "EntrytoDac:" << callbackEntrytoDacSecs;
            m_invalidTimeInfoWarned = true;
        }
        // Assume we are in Time
        VisualPlayPosition::setCallbackEntryToDacSecs(bufferSizeSec, m_clkRefTimer);
    } else {
        VisualPlayPosition::setCallbackEntryToDacSecs(callbackEntrytoDacSecs, m_clkRefTimer);
    }

    //qDebug() << "TimeInfo"
    //         << (timeInfo->currentTime - floor(timeInfo->currentTime))
    //         << (timeInfo->outputBufferDacTime - floor(timeInfo->outputBufferDacTime));
    //qDebug() << "TimeInfo" << bufferSizeSec
    //        << timeInfo->outputBufferDacTime - timeInfo->currentTime;
}

SoundDeviceError SoundDeviceJack::addOutput(const AudioOutputBuffer& out) {
    // Check if the output channels are already used
    foreach (AudioOutputBuffer myOut, m_audioOutputs) {
        if (out.channelsClash(myOut)) {
            m_lastError = QObject::tr("Output channel is used twice");
            return SOUNDDEVICE_ERROR_DUPLICATE_OUTPUT_CHANNEL;
        }
    }
    if (out.getHighChannel() > getNumOutputChannels()) {
        m_lastError = QObject::tr("To many output channels added");
        return SOUNDDEVICE_ERROR_EXCESSIVE_OUTPUT_CHANNEL;
    }
    m_audioOutputs.append(out);
    return SOUNDDEVICE_ERROR_OK;
}

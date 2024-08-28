#include <analyzer/analyzertuning.h>

#include <QString>
#include <iostream>
#include <string>

#define _VAMP_PLUGIN_IN_HOST_NAMESPACE 1

#include <track/keyutils.h>
#include <track/track.h>
#include <util/sample.h>
#include <vamp-hostsdk/PluginBufferingAdapter.h>
#include <vamp-hostsdk/PluginInputDomainAdapter.h>

// #include "../lib/nnls-chroma/Chordino.h"
#include "../lib/nnls-chroma/Tuning.h"

namespace {
constexpr uint_t kWinSize = 1024; // Window size
constexpr uint_t kHopSize = 512;  // Hop size
constexpr char_t kMethod[] = "yin";
} // namespace


bool AnalyzerTuning::initialize(const AnalyzerTrack& track,
        mixxx::audio::SampleRate sampleRate,
        SINT frameLength) {
    if (frameLength <= 0) {
        return false;
    }

    if (!m_keySettings.getKeyDetectionEnabled()) {
        qDebug() << "Key detection is deactivated";
        return false;
    }

    m_sampleRate = sampleRate;
    m_totalFrames = frameLength;

    // Create YIN object
    m_pAubioPitch = new_aubio_pitch(kMethod,
            kWinSize,
            kHopSize,
            static_cast<uint_t>(sampleRate.toDouble()));
    m_pInput = new_fvec(kHopSize); // Input buffer
    m_pOutput = new_fvec(1);       // Output buffer (pitch value)

    m_pTuning = new Tuning(m_sampleRate);
    m_pIa = new Vamp::HostExt::PluginInputDomainAdapter(m_pTuning);
    m_pIa->setProcessTimestampMethod(Vamp::HostExt::PluginInputDomainAdapter::ShiftData);
    m_pAdapter = new Vamp::HostExt::PluginBufferingAdapter(m_pIa);

    int blocksize = m_pAdapter->getPreferredBlockSize();

    // Plugin requires 1 channel (we will mix down)
    if (!m_pAdapter->initialise(1, blocksize, blocksize)) {
        std::cerr << ": Failed to initialise Chordino adapter!" << std::endl;
        return 1;
    }

    int chordFeatureNo = -1;
    Vamp::Plugin::OutputList outputs = m_pAdapter->getOutputDescriptors();
    for (int i = 0; i < int(outputs.size()); ++i) {
        if (outputs[i].identifier == "simplechord") {
            chordFeatureNo = i;
        }
    }
    if (chordFeatureNo < 0) {
        std::cerr << ": Failed to identify chords output!" << endl;
        return 1;
    }

    // if we can't load a stored track reanalyze it
    return shouldAnalyze(track.getTrack());
}

bool AnalyzerTuning::shouldAnalyze(TrackPointer pTrack) const {
    Q_UNUSED(pTrack);
    return true;
}

AnalyzerTuning::AnalyzerTuning(const KeyDetectionSettings& keySettings)
        : m_keySettings(keySettings),
          m_sampleRate(0),
          m_totalFrames(0),
          m_maxFramesToProcess(0),
          m_currentFrame(0),
          m_pAubioPitch(nullptr),
          m_pInput(nullptr),
          m_pOutput(nullptr),
          m_counts{},
          m_pTuning(nullptr),
          m_pIa(nullptr),
          m_pAdapter(nullptr),
          m_chordFeatureNo(0) {
}

bool AnalyzerTuning::processSamples(const CSAMPLE* pIn, SINT count) {
    /*
for (uint_t i = 0; i < count / 2; i += kHopSize) {
    // Fill input buffer with your audio data
    SampleUtil::mixMultichannelToMono(m_pInput->data, pIn + i * 2, kHopSize * 2);

    // Perform pitch detection
    aubio_pitch_do(m_pAubioPitch, m_pInput, m_pOutput);

    // Get detected pitch
    float detected_pitch = fvec_get_sample(m_pOutput, 0);
    double key = KeyUtils::powerOf2ToSemitoneChange(detected_pitch / 440.0) + 49;
    double tuning = std::fmod(key, 1.0);
    m_counts[static_cast<std::size_t>(tuning * 10 + 0.5)]++;
    if (tuning >= 0.5) {
        tuning -= 1.0;
    }
    qDebug() << "Detected pitch:" << detected_pitch << "Hz"
             << key << tuning;
    if (detected_pitch) {
        m_tunings.push_back(tuning);
    }
}
*/

    int blocksize = m_pAdapter->getPreferredBlockSize();
    float* mixbuf = new float[blocksize];

    Vamp::Plugin::FeatureList chordFeatures;
    Vamp::Plugin::FeatureSet fs;
    int frame = 0;
    while (frame < count / 2) {
        // mix down
        for (int i = 0; i < blocksize; ++i) {
            mixbuf[i] = 0.f;
            if (i < count / 2) {
                for (int c = 0; c < 2; ++c) {
                    mixbuf[i] += pIn[i * 2 + c] / 2;
                }
            }
        }

        Vamp::RealTime timestamp = Vamp::RealTime::frame2RealTime(frame, m_sampleRate);

        // feed to plugin: can just take address of buffer, as only one channel
        fs = m_pAdapter->process(&mixbuf, timestamp);

        chordFeatures.insert(
                chordFeatures.end(),
                fs[m_chordFeatureNo].begin(),
                fs[m_chordFeatureNo].end());

        frame += blocksize;
    }

    // features at end of processing (actually Chordino does all its work here)
    fs = m_pAdapter->getRemainingFeatures();

    // chord output is output index 0
    chordFeatures.insert(
            chordFeatures.end(),
            fs[m_chordFeatureNo].begin(),
            fs[m_chordFeatureNo].end());

    for (int i = 0; i < (int)chordFeatures.size(); ++i) {
        std::cout << chordFeatures[i].timestamp.toString() << ": "
                  << chordFeatures[i].label << endl;
    }

    delete[] mixbuf;

    return true;
}

void AnalyzerTuning::cleanup() {
    qDebug() << "cleamup";
    del_aubio_pitch(m_pAubioPitch);
    del_fvec(m_pInput);
    del_fvec(m_pOutput);
    m_tunings.clear();

    delete m_pAdapter;
}

void AnalyzerTuning::storeResults(TrackPointer pTrack) {
    /*
    qDebug() << "AnalyzerTuning::storeResults";
double average = 0;
for (const auto& tuning : m_tunings) {
    // qDebug() << tuning;
    average += tuning;
}

for (int i = 0; i < 10; ++i) {
    qDebug() << i << m_counts[i];
}

qDebug() << "average:" << average / m_tunings.size() <<
KeyUtils::semitoneChangeToPowerOf2(average / m_tunings.size()) * 440;
Q_UNUSED(pTrack);
*/

    // features at end of processing (actually Chordino does all its work here)
    Vamp::Plugin::FeatureSet fs = m_pAdapter->getRemainingFeatures();

    // chord output is output index 0
    Vamp::Plugin::FeatureList chordFeatures;
    chordFeatures.insert(
            chordFeatures.end(),
            fs[m_chordFeatureNo].begin(),
            fs[m_chordFeatureNo].end());

    for (int i = 0; i < (int)chordFeatures.size(); ++i) {
        std::cout << chordFeatures[i].timestamp.toString() << ": "
                  << chordFeatures[i].label << endl;
    }

    pTrack->setComment(QString::fromStdString(chordFeatures[0].label));
}

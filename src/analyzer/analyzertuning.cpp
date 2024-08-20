#include <analyzer/analyzertuning.h>
#include <track/keyutils.h>
#include <util/sample.h>

namespace {
constexpr uint_t kWinSize = 1024; // Window size
constexpr uint_t kHopSize = 512;  // Hop size
constexpr char_t kMethod[] = "yin";
} // namespace

AnalyzerTuning::AnalyzerTuning(const KeyDetectionSettings& keySettings)
        : m_keySettings(keySettings),
          m_sampleRate(0),
          m_totalFrames(0),
          m_maxFramesToProcess(0),
          m_currentFrame(0),
          m_counts{} {
}

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

    // if we can't load a stored track reanalyze it
    return shouldAnalyze(track.getTrack());
}

bool AnalyzerTuning::shouldAnalyze(TrackPointer pTrack) const {
    Q_UNUSED(pTrack);
    return true;
}

bool AnalyzerTuning::processSamples(const CSAMPLE* pIn, SINT count) {
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
    return true;
}

void AnalyzerTuning::cleanup() {
    qDebug() << "cleamup";
    del_aubio_pitch(m_pAubioPitch);
    del_fvec(m_pInput);
    del_fvec(m_pOutput);
    m_tunings.clear();
}

void AnalyzerTuning::storeResults(TrackPointer pTrack) {
    qDebug() << "AnalyzerTuning::storeResults";
    double average = 0;
    for (const auto& tuning : m_tunings) {
        qDebug() << tuning;
        average += tuning;
    }

    for (int i = 0; i < 10; ++i) {
        qDebug() << i << m_counts[i];
    }

    qDebug() << "average:" << average / m_tunings.size();
    Q_UNUSED(pTrack);
}

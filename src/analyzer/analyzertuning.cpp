#include <analyzer/analyzertuning.h>

AnalyzerTuning::AnalyzerTuning(const KeyDetectionSettings& keySettings)
        : m_keySettings(keySettings),
          m_sampleRate(0),
          m_totalFrames(0),
          m_maxFramesToProcess(0),
          m_currentFrame(0) {
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

    // if we can't load a stored track reanalyze it
    return shouldAnalyze(track.getTrack());
}

bool AnalyzerTuning::shouldAnalyze(TrackPointer pTrack) const {
    Q_UNUSED(pTrack);
    return true;
}

bool AnalyzerTuning::processSamples(const CSAMPLE* pIn, SINT count) {
    Q_UNUSED(pIn);
    Q_UNUSED(count);
    qDebug() << "AnalyzerTuning::processSamples";
    return true;
}

void AnalyzerTuning::cleanup() {
}

void AnalyzerTuning::storeResults(TrackPointer pTrack) {
    qDebug() << "AnalyzerTuning::storeResults";
    Q_UNUSED(pTrack);
}

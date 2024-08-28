#pragma once

#include <aubio/aubio.h>

#include "analyzer/analyzer.h"
#include "analyzer/plugins/analyzerplugin.h"
#include "preferences/keydetectionsettings.h"
#include "track/track_decl.h"

#define _VAMP_PLUGIN_IN_HOST_NAMESPACE 1

#include <vamp-hostsdk/PluginBufferingAdapter.h>
#include <vamp-hostsdk/PluginInputDomainAdapter.h>

#include "../lib/nnls-chroma/Tuning.h"

class AnalyzerTuning : public Analyzer {
  public:
    explicit AnalyzerTuning(const KeyDetectionSettings& keySettings);
    ~AnalyzerTuning() override = default;

    bool initialize(const AnalyzerTrack& track,
            mixxx::audio::SampleRate sampleRate,
            SINT frameLength) override;
    bool processSamples(const CSAMPLE* pIn, SINT count) override;
    void storeResults(TrackPointer tio) override;
    void cleanup() override;

  private:
    bool shouldAnalyze(TrackPointer tio) const;

    KeyDetectionSettings m_keySettings;
    mixxx::audio::SampleRate m_sampleRate;

    SINT m_totalFrames;
    SINT m_maxFramesToProcess;
    SINT m_currentFrame;

    aubio_pitch_t* m_pAubioPitch;
    fvec_t* m_pInput;
    fvec_t* m_pOutput;
    std::vector<double> m_tunings;
    int m_counts[10];

    Tuning* m_pTuning;
    Vamp::HostExt::PluginInputDomainAdapter* m_pIa;
    Vamp::HostExt::PluginBufferingAdapter* m_pAdapter;
    int m_chordFeatureNo;
};

#include "analyzer/analyzerrhythm.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <QtDebug>
#include <unordered_set>
#include <vector>

#include "analyzer/analyzerrhythmstats.h"
#include "analyzer/constants.h"
#include "engine/engine.h" // Included to get mixxx::kEngineChannelCount
#include "track/beatfactory.h"
#include "track/beatmap.h"
#include "track/track.h"

namespace {

// This determines the resolution of the resulting BeatMap.
// ~12 ms (86 Hz) is a fair compromise between accuracy and analysis speed,
// also matching the preferred window/step sizes from BeatTrack VAMP.
constexpr float kStepSecs = 0.0113378684807;
// results in 43 Hz @ 44.1 kHz / 47 Hz @ 48 kHz / 47 Hz @ 96 kHz
constexpr int kMaximumBinSizeHz = 50; // Hz
// This is a quick hack to make a beatmap with only downbeats - will affect the bpm
constexpr bool useDownbeatOnly = false;
// The number of types of detection functions
constexpr int kDfTypes = 5;
// tempogram resolution constants
constexpr float kNoveltyCurveMinDB = -74.0;
constexpr float kNoveltyCurveCompressionConstant = 1000.0;
constexpr int kTempogramLog2WindowLength = 12;
constexpr int kTempogramLog2HopSize = 8;
constexpr int kTempogramLog2FftLength = 12;
constexpr float kNoveltyCurveHop = 512.0;
constexpr float kNoveltyCurveWindow = 1024.0;

constexpr double kThressholDecay = 0.04;
constexpr double kThressholRecover = 0.5;
constexpr double kFloorFactor = 0.5;

constexpr double kCloseToIntTolerance = 0.05;

DFConfig makeDetectionFunctionConfig(int stepSize, int windowSize) {
    // These are the defaults for the VAMP beat tracker plugin
    DFConfig config;
    config.DFType = dfAll;
    config.stepSize = stepSize;
    config.frameLength = windowSize;
    config.dbRise = 3;
    config.adaptiveWhitening = 0;
    config.whiteningRelaxCoeff = -1;
    config.whiteningFloor = -1;
    return config;
}
} // namespace

double AnalyzerRhythm::frameToMinutes(int frame) {
    double minute = (frame/static_cast<double>(m_iSampleRate))/60.0;
    double intMinutes;
    double fractionalMinutes = std::modf(minute, &intMinutes);
    double seconds = (fractionalMinutes*60.0)/100;
    return intMinutes + seconds;
}

AnalyzerRhythm::AnalyzerRhythm(UserSettingsPointer pConfig)
        : m_iSampleRate(0),
          m_iTotalSamples(0),
          m_iMaxSamplesToProcess(0),
          m_iCurrentSample(0),
          m_iMinBpm(0),
          m_iMaxBpm(9999),
          m_noveltyCurveMinV(pow(10,kNoveltyCurveMinDB/20.0)) {
}

inline int AnalyzerRhythm::stepSize() {
    return m_iSampleRate * kStepSecs;
}

inline int AnalyzerRhythm::windowSize() {
    return MathUtilities::nextPowerOfTwo(m_iSampleRate / kMaximumBinSizeHz);
}

bool AnalyzerRhythm::initialize(TrackPointer pTrack, int sampleRate, int totalSamples) {
    if (totalSamples == 0 or !shouldAnalyze(pTrack)) {
        return false;
    }

    m_iSampleRate = sampleRate;
    m_iTotalSamples = totalSamples;
    m_iMaxSamplesToProcess = m_iTotalSamples;
    m_iCurrentSample = 0;

    // decimation factor aims at resampling to c. 3KHz; must be power of 2
    int factor = MathUtilities::nextPowerOfTwo(m_iSampleRate / 3000);
    m_downbeat = std::make_unique<DownBeat>(
            m_iSampleRate, factor, stepSize());
    
    m_fft = std::make_unique<FFTReal>(windowSize());
    m_fftRealOut.reserve(windowSize());
    m_fftImagOut.reserve(windowSize());

    m_window = std::make_unique<Window<double>>(HammingWindow, windowSize());
    m_pDetectionFunction = std::make_unique<DetectionFunction>(
            makeDetectionFunctionConfig(stepSize(), windowSize()));
    
    qDebug() << "input sample rate is " << m_iSampleRate << ", step size is " << stepSize();

    m_onsetsProcessor.initialize(
            windowSize(), stepSize(), [this](double* pWindow, size_t) {
                DFresults onsets;
                m_window->cut(pWindow);
                m_fft->forward(pWindow, &m_fftRealOut[0], &m_fftImagOut[0]);
                onsets = m_pDetectionFunction->processFrequencyDomain(&m_fftRealOut[0], &m_fftImagOut[0]);
                m_detectionResults.push_back(onsets);
                return true;
            });

    m_downbeatsProcessor.initialize(
            windowSize(), stepSize(), [this](double* pWindow, size_t) {
                std::vector<float> window;
                window.reserve(windowSize());
                for (int i = 0; i < windowSize(); i+=1) {
                    window.push_back(static_cast<float>(pWindow[i]));
                }
                m_downbeat->pushAudioBlock(&window[0]);
                return true;
            });

    m_noveltyCurveProcessor.initialize(
        kNoveltyCurveWindow, kNoveltyCurveHop, [this](double* pWindow, size_t) {
            int n = kNoveltyCurveWindow;
            double *in = pWindow;
            //calculate magnitude of FrequencyDomain input
            std::vector<float> fftCoefficients;
            for (int i = 0; i < n; i++){
                float magnitude = in[i];
                magnitude = magnitude > m_noveltyCurveMinV ? magnitude : m_noveltyCurveMinV;
                fftCoefficients.push_back(magnitude);
            }
            m_spectrogram.push_back(fftCoefficients);
            return true;
        });
    
    return true;
}

void AnalyzerRhythm::setTempogramParameters() {
    m_tempogramWindowLength = pow(2,kTempogramLog2WindowLength);
    m_tempogramHopSize = pow(2,kTempogramLog2HopSize);
    m_tempogramFftLength = pow(2,kTempogramLog2FftLength);
    m_tempogramInputSampleRate = m_iSampleRate / kNoveltyCurveHop;
    // BPM here is not the "beat' tempo but the metrical pulses tempo
    m_tempogramMinBPM = 4; // to noisy to go lower..
    m_tempogramMaxBPM = 70; // here are already reaching the beat pulse..
}


bool AnalyzerRhythm::shouldAnalyze(TrackPointer pTrack) const {
    bool bpmLock = pTrack->isBpmLocked();
    if (bpmLock) {
        qDebug() << "Track is BpmLocked: Beat calculation will not start";
        return true;
    }
    mixxx::BeatsPointer pBeats = pTrack->getBeats();
    if (!pBeats) {
        return true;
    } else if (!mixxx::Bpm::isValidValue(pBeats->getBpm())) {
        qDebug() << "Re-analyzing track with invalid BPM despite preference settings.";
        return true;
    } else {
        qDebug() << "Track already has beats, and won't re-analyze";
        return false;
    }
    return true;
}

bool AnalyzerRhythm::processSamples(const CSAMPLE* pIn, const int iLen) {
    m_iCurrentSample += iLen;
    if (m_iCurrentSample > m_iMaxSamplesToProcess) {
        return true; // silently ignore all remaining samples
    }
    bool onsetReturn = m_onsetsProcessor.processStereoSamples(pIn, iLen);
    bool downbeatsReturn = m_downbeatsProcessor.processStereoSamples(pIn, iLen);
    bool noveltyCurvReturn = m_noveltyCurveProcessor.processStereoSamples(pIn, iLen);
    return onsetReturn & downbeatsReturn & noveltyCurvReturn;
}

void AnalyzerRhythm::cleanup() {
    m_resultBeats.clear();
    m_detectionResults.clear();
    m_noveltyCurve.clear();
    m_tempogramDFT.clear();
    m_tempogramACF.clear();
    m_metergram.clear();
    m_spectrogram.clear();
    m_pDetectionFunction.reset();
    m_fft.reset();
    m_downbeat.reset();
    m_window.reset();
    m_fftImagOut.clear();
    m_fftRealOut.clear();
    m_broadbandEnergyAtBeat.clear();
    m_beats.clear();
    m_stableTemposByPositions.clear();
    m_rawTempos.clear();
    m_rawTemposFrenquency.clear();
}

void AnalyzerRhythm::computeBeats() {
    int nonZeroCount = m_detectionResults.size();
    while (nonZeroCount > 0 && m_detectionResults[nonZeroCount - 1].t.complexSpecDiff <= 0.0) {
        --nonZeroCount;
    }
    std::vector<double> noteOnsets;
    std::vector<double> beatPeriod;
    std::vector<double> tempi;
    const auto required_size = std::max(0, nonZeroCount - 2);
    noteOnsets.reserve(required_size);
    beatPeriod.reserve(required_size);
    // skip first 2 results as it might have detect noise as onset
    // that's how vamp does and seems works best this way
    for (int i = 2; i < nonZeroCount; ++i) {
        noteOnsets.push_back(m_detectionResults[i].t.complexSpecDiff);
        beatPeriod.push_back(0.0);
        
    }
    TempoTrackV2 tt(m_iSampleRate, stepSize());
    tt.calculateBeatPeriod(noteOnsets, beatPeriod, tempi);
    tt.calculateBeats(noteOnsets, beatPeriod, m_beats);
    // Let's compare all beats positions and use the "best" one
    /*
    double maxAgreement = 0.0;
    int maxAgreementIndex = 0;
    for (int thisOne = 0; thisOne < kDfTypes; thisOne += 1) {
        double agreementPercentage;
        int agreement = 0;
        int maxPossibleAgreement = 1;
        std::unordered_set<double> thisOneAsSet(allBeats[thisOne].begin(), allBeats[thisOne].end());
        for (int theOther = 0; theOther < kDfTypes; theOther += 1) {
            if (thisOne == theOther) {
                continue;
            }
            for (size_t beat = 0; beat < allBeats[theOther].size(); beat += 1) {
                if (thisOneAsSet.find(allBeats[theOther][beat]) != thisOneAsSet.end()) {
                    agreement += 1;
                }
                maxPossibleAgreement += 1;
            }
        }
        agreementPercentage = agreement / static_cast<double>(maxPossibleAgreement);
        qDebug() << thisOne << "agreementPercentage is" << agreementPercentage;
        if (agreementPercentage > maxAgreement) {
            maxAgreement = agreementPercentage;
            maxAgreementIndex = thisOne;
        }
    }
    */
   // convert beats position from stepsize increments to frame pos and save in member var
   for (size_t i = 0; i < m_beats.size(); ++i) {
        double result = (m_beats.at(i) * stepSize()) - (stepSize() / mixxx::kEngineChannelCount);
        m_resultBeats.push_back(result);
    }
    std::tie(m_rawTempos, m_rawTemposFrenquency) = computeRawTemposAndFrequency(m_resultBeats);
}

std::vector<double> AnalyzerRhythm::computeSnapGrid() {
    int size = m_detectionResults.size();

    int dfType = 3; // ComplexSD

    std::vector<double> complexSdMinDiff;
    std::vector<double> minWindow;
    std::vector<double> maxWindow;
    complexSdMinDiff.reserve(size);

    // Calculate the change from the minimum of three previous SDs
    // This ensures we find the beat start and not the noisiest place within the beat.
    complexSdMinDiff.push_back(m_detectionResults[0].results[3]);
    for (int i = 1; i < size; ++i) {
        double result = m_detectionResults[i].results[3];
        double min = result;
        for (int j = i - 3; j < i + 2; ++j) {
            if (j >= 0 && j < size) {
                double value = m_detectionResults[j].results[3];
                if (value < min) {
                    min = value;
                }
            }
        }
        complexSdMinDiff.push_back(result - min * kFloorFactor);
    }

    std::vector<double> allBeats;
    allBeats.reserve(size / 10);

    // This is a dynamic threshold that defines which SD is considered as a beat.
    double threshold = 0;

    // Find peak beats within a window of 9 SDs (100 ms)
    // This limits the detection result to 600 BPM
    for (int i = 0; i < size; ++i) {
        double beat = 0;
        double result = complexSdMinDiff[i];
        if (result > threshold) {
            double max = result;
            for (int j = i - 4; j < i + 4; ++j) {
                if (j >= 0 && j < size) {
                    double value = complexSdMinDiff[j];
                    if (value > max) {
                        max = value;
                    }
                }
            }
            if (max == result) {
                // Beat found
                beat = threshold;
                threshold = threshold * (1 - kThressholRecover) + result * kThressholRecover;
                allBeats.push_back(i);
            }
        }
        threshold = threshold * (1 - kThressholDecay) + result * kThressholDecay;
        /*
        qDebug() << i
                 << m_detectionResults[i].results[3]
                 << complexSdMinDiff[i]
                 << threshold << beat;
        */
    }

    return allBeats;
}

void AnalyzerRhythm::computeBeatsSpectralDifference(std::vector<double> &beats) {
    size_t downLength = 0;
    const float *downsampled = m_downbeat->getBufferedAudio(downLength);
    m_downbeat->findDownBeats(downsampled, downLength, beats, m_downbeats);
    m_downbeat->getBeatSD(m_beatsSpecDiff);
}

void AnalyzerRhythm::computeMeter() {
    int blockCounter = 0;
    double tempoSum = 0;
    double tempoCounter = 0.0;
    for (int i = 0; i < m_resultBeats.size(); i+=1) {
        double beatFramePos = m_resultBeats[i];
        double blockEnd = m_tempogramHopSize * (blockCounter + 1) * kNoveltyCurveHop;
        tempoSum += m_rawTempos[i];
        tempoCounter += 1.0;
        if (blockEnd < beatFramePos) {            
            // we only keep the pulses that are interger divisors of the bpm
            double localTempo = tempoSum / tempoCounter;
            qDebug() << "at" << frameToMinutes(beatFramePos) << "local tempo is" << localTempo;
            QMap<int, double> metricalPulsesLenghtsAndWeights;
            auto allPulses = m_metergram[blockCounter].keys();
            for (auto pulse : allPulses) {
                if (pulse > localTempo) {
                    break;
                }
                double ratio = localTempo/pulse;
                double beatLenghtOfPulse;
                double ratioDecimals = std::modf(ratio, &beatLenghtOfPulse);
                if (ratioDecimals < kCloseToIntTolerance) {
                    metricalPulsesLenghtsAndWeights[static_cast<int>(beatLenghtOfPulse)]
                            += m_metergram[blockCounter][pulse];
                }
            }
            // now we combine the integer pulses into metrical hieranchies
            std::vector<int> metricalHierchy;
            double highestWeight = 0.0;
            auto metricalPulses = metricalPulsesLenghtsAndWeights.keys();
            for (int thisMetricUnit = 0; thisMetricUnit < metricalPulses.size(); thisMetricUnit+=1) {
                std::vector<int> metricalHierchyCanditade;
                double metricalHierchyCanditadeWeight = 0.0;
                metricalHierchyCanditade.push_back(metricalPulses[thisMetricUnit]);
                metricalHierchyCanditadeWeight +=
                        metricalPulsesLenghtsAndWeights[metricalPulses[thisMetricUnit]];
                for (int metricUnitToTest = 0; 
                        metricUnitToTest < metricalPulses.size();
                        metricUnitToTest+=1) {

                    if (thisMetricUnit == metricUnitToTest) {continue;}
                    bool intergerDivisorOfAllUnits = false;
                    for (int thisHirearchyUnits = 0; 
                            thisHirearchyUnits < metricalHierchyCanditade.size();
                            thisHirearchyUnits+=1) {

                        if (metricalPulses[metricUnitToTest] % metricalHierchyCanditade[thisHirearchyUnits] != 0) {
                            intergerDivisorOfAllUnits = false;
                            break;
                        }
                        intergerDivisorOfAllUnits = true;
                    }
                    if (intergerDivisorOfAllUnits) {
                        metricalHierchyCanditade.push_back(metricalPulses[metricUnitToTest]);
                        metricalHierchyCanditadeWeight += 
                                metricalPulsesLenghtsAndWeights[metricalPulses[metricUnitToTest]];
                    }
                }
                // for now keep only the best hierarchy that explain the meter
                if (metricalHierchyCanditadeWeight > highestWeight) {
                    metricalHierchy = metricalHierchyCanditade;
                    highestWeight = metricalHierchyCanditadeWeight;
                }
            }
            qDebug() << metricalHierchy << highestWeight;
            // set for next iteration
            tempoSum = 0;
            tempoCounter = 0.0;
            blockCounter += 1;
        }
    }
}

int AnalyzerRhythm::computeNoveltyCurve() {
    NoveltyCurveProcessor nc(static_cast<float>(m_iSampleRate), kNoveltyCurveWindow, kNoveltyCurveCompressionConstant);
    m_noveltyCurve = nc.spectrogramToNoveltyCurve(m_spectrogram);
    return m_spectrogram.size();
}

void AnalyzerRhythm::computeTempogramByDFT() {
    auto hannWindow = std::vector<float>(m_tempogramWindowLength, 0.0);
    WindowFunction::hanning(&hannWindow[0], m_tempogramWindowLength);
    SpectrogramProcessor spectrogramProcessor(m_tempogramWindowLength,
            m_tempogramFftLength, m_tempogramHopSize);
    Spectrogram tempogramDFT = spectrogramProcessor.process(
            &m_noveltyCurve[0], m_noveltyCurve.size(), &hannWindow[0]);
    // convert y axis to bpm
    int tempogramMinBin = (std::max(static_cast<int>(floor(((m_tempogramMinBPM/60.0)
            /m_tempogramInputSampleRate)*m_tempogramFftLength)), 0));
    int tempogramMaxBin = (std::min(static_cast<int>(ceil(((m_tempogramMaxBPM/60.0)
            /m_tempogramInputSampleRate)*m_tempogramFftLength)), m_tempogramFftLength/2));
    int binCount = tempogramMaxBin - tempogramMinBin + 1;
    double highest;
    double bestBpm;
    int bin;
    for (int block = 0; block < tempogramDFT.size(); block++) {
        // dft
        //qDebug() << "block" << block;
        //qDebug() << "DFT tempogram";
        highest = .0;
        bestBpm = .0;
        bin = 0;
        QMap<double, double> dft;
        for (int k = tempogramMinBin; k <= tempogramMaxBin; k++){
            double w = (k/static_cast<double>(m_tempogramFftLength))*(m_tempogramInputSampleRate);
            double bpm = w*60;
            dft[bpm] = tempogramDFT[block][k];
            //qDebug() << "bin, bpm and value"<< bin++ << bpm << tempogramDFT[block][k];
            if (tempogramDFT[block][k] > highest) {
                highest = tempogramDFT[block][k];
                bestBpm = bpm;
            }
        }
        m_tempogramDFT.push_back(dft);
        //qDebug() << "best bpm at block" << bestBpm << block;
    }
}

void AnalyzerRhythm::computeTempogramByACF() {
    // Compute acf tempogram
    AutocorrelationProcessor autocorrelationProcessor(m_tempogramWindowLength, m_tempogramHopSize);
    Spectrogram tempogramACF = autocorrelationProcessor.process(&m_noveltyCurve[0], m_noveltyCurve.size());
    // Convert y axis to bpm
    int tempogramMinLag = std::max(static_cast<int>(ceil((60/ static_cast<double>((kNoveltyCurveHop) * m_tempogramMaxBPM))
                *m_iSampleRate)), 0);
    int tempogramMaxLag = std::min(static_cast<int>(floor((60/ static_cast<double>((kNoveltyCurveHop) * m_tempogramMinBPM))
                *m_iSampleRate)), m_tempogramWindowLength-1);
    qDebug() << tempogramMinLag << tempogramMaxLag;
    double highest;
    double bestBpm;
    int bin;
    for (int block = 0; block < tempogramACF.size(); block++) {
        //qDebug() << "block" << block;
        //qDebug() << "ACF tempogram";
        highest = .0;
        bestBpm = .0;
        bin = 0;
        QMap<double, double> acf;
        for (int lag = tempogramMaxLag; lag >= tempogramMinLag; lag--) {
            double bpm = 60/static_cast<double>(kNoveltyCurveHop * (lag/static_cast<double>(m_iSampleRate)));
            //qDebug() << "bin, bpm and value"<< bin++ << bpm << tempogramACF[block][lag];
            acf[bpm] = tempogramACF[block][lag];
            if (tempogramACF[block][lag] > highest) {
                highest = tempogramACF[block][lag];
                bestBpm = bpm;
            }
        }
        m_tempogramACF.push_back(acf);
        //qDebug() << "best bpm at block" << bestBpm << block;
    }
}

void AnalyzerRhythm::computeMetergram() {
    // the metergram is the element-wise product of the dft and act tempograms but
    // since they are mapped to different bpms first we need to interpolate
    // one of them...The idea is that the dft tempogram contains the meter pulses
    // with it's harmonics and some sporious peaks, while the acf has
    // the meter pulses, the subharmonics and other spororious peak, by multiplying
    // them we should enchance the meter pulses only..
    for (int i = 0; i < m_tempogramDFT.size(); i+=1) {
        QMap<double, double> metergramBlock;
        for (auto act = m_tempogramACF[i].begin(); act != m_tempogramACF[i].end(); act +=1) {
            auto nextDFT = m_tempogramDFT[i].lowerBound(act.key());
            auto previousDFT = nextDFT - 1;
            double x0 = previousDFT.key();
            double y0 = previousDFT.value();
            double x1 = nextDFT.key();
            double y1 = nextDFT.value();
            double xp = act.key();
            double yp = y0 + ((y1-y0)/(x1-x0)) * (xp - x0);
            metergramBlock[act.key()] = act.value() * yp;
        }
        m_metergram.push_back(metergramBlock);
    }
}

void AnalyzerRhythm::computeBroadbandEnergyAtBeats() {
    // The broadband energy detection function is very useful
    // for finding strongs percusive onsets...
    int stepPos = 0;
    int beatPos;
    int beatIndex = 0;
    m_broadbandEnergyAtBeat.resize(m_resultBeats.size(), 0.0);
    for (auto dfResult : m_detectionResults) {
        beatPos = static_cast<int>(m_resultBeats[beatIndex]);
        if (stepPos <= beatPos) {
            m_broadbandEnergyAtBeat[beatIndex] += dfResult.t.broadband;
        } else {
            if (beatIndex == m_resultBeats.size() -1) {
                break;
            }
            beatIndex += 1;
        }
        stepPos += stepSize();
    }
    double sum = 0;
    for (auto energy : m_broadbandEnergyAtBeat) {
        sum += energy;
    }
    m_broadbandMeanEnergy = sum / m_broadbandEnergyAtBeat.size();
    m_broadbandStdDev = BeatStatistics::stddev(m_broadbandEnergyAtBeat);
}

void AnalyzerRhythm::RemoveArrhythmicWeakBeats() {
    // A common problem the analyzer has is to detect arrhythmic regions
    // of tracks with a constant tempo as in a different unsteady tempo. 
    // This happens frequently on builds and breaks with heavy effects on edm music.
    // Since these occurs most on beatless regions we do not want them to be
    // on a different tempo, because they are still syncable in the true tempo
    // We use the broadband energy to check if these regions lack strong
    // percussive onsets and remove them if so..
    auto positionsWithTempoChange = m_stableTemposByPositions.keys();
    auto tempoValues = m_stableTemposByPositions.values();
    qDebug() << positionsWithTempoChange;
    QVector<double> anchoredBeats;
    anchoredBeats.reserve(m_resultBeats.size());
    anchoredBeats << QVector<double>::fromStdVector(std::vector<double>(
                m_resultBeats.begin(), m_resultBeats.begin() + positionsWithTempoChange[1]));

    for (int i = 2; i < positionsWithTempoChange.size(); i+=1) {
        int limitAtLeft = positionsWithTempoChange[i-1];
        int limitAtRight = positionsWithTempoChange[i];
        int lenghtOfChange = limitAtRight - limitAtLeft;
        double previousTempo = (m_stableTemposByPositions.lowerBound(limitAtLeft)-1).value();
        double localBroadbandMeanEnergy = 0;
        for (int j = limitAtLeft; j < limitAtRight; j += 1) {
            localBroadbandMeanEnergy += m_broadbandEnergyAtBeat[j];
        }
        localBroadbandMeanEnergy /= lenghtOfChange;
        double thirdOfStdDev = m_broadbandStdDev / 3.0;
        double energyThreshold = m_broadbandMeanEnergy - thirdOfStdDev;
        qDebug() << localBroadbandMeanEnergy << energyThreshold << frameToMinutes(m_resultBeats[limitAtLeft]);
        if (localBroadbandMeanEnergy < energyThreshold) {
            m_stableTemposByPositions.remove(limitAtLeft);
            double beatOffset = m_resultBeats[limitAtLeft];
            // we do not want beats at fractional frame position
            double beatLength = floor(((60.0 * m_iSampleRate) / previousTempo) + 0.5);
            while (beatOffset < m_resultBeats[limitAtRight]) {
                anchoredBeats << beatOffset;
                beatOffset += beatLength;
            }

        } else {
            anchoredBeats << QVector<double>::fromStdVector(std::vector<double>(
                    m_resultBeats.begin() + positionsWithTempoChange[i-1],
                    m_resultBeats.begin() + positionsWithTempoChange[i]));
        }
    }
    // We may have shinkred or enlargerged our beat vector so we 
    // make sure last sentinal is in the right place.
    m_stableTemposByPositions.remove(m_stableTemposByPositions.last());
    m_stableTemposByPositions[anchoredBeats.size() -1] = m_stableTemposByPositions[0];
    m_resultBeats = anchoredBeats;
    // And also update our raw tempo vector to match the new beats..
    std::tie(m_rawTempos, m_rawTemposFrenquency) = computeRawTemposAndFrequency(m_resultBeats);
}
 
void AnalyzerRhythm::storeResults(TrackPointer pTrack) {
    m_onsetsProcessor.finalize();
    m_downbeatsProcessor.finalize();
    m_noveltyCurveProcessor.finalize();

    auto notes = computeSnapGrid();
    setTempogramParameters();
    computeNoveltyCurve();    
    computeTempogramByACF();
    computeTempogramByDFT();
    computeMetergram();
    computeBeats();
    findTempoChanges();
    computeBroadbandEnergyAtBeats();
    RemoveArrhythmicWeakBeats();
    computeMeter();

    // TODO(Cristiano&Harshit) THIS IS WHERE A BEAT VECTOR IS CREATED
    auto pBeatMap = new mixxx::BeatMap(*pTrack, m_iSampleRate, m_resultBeats);
    auto pBeats = mixxx::BeatsPointer(pBeatMap, &BeatFactory::deleteBeats);
    pTrack->setBeats(pBeats);
}

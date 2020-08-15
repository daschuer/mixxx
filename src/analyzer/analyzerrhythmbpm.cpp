#include "analyzer/analyzerrhythm.h"

#include <algorithm>
#include <array>
#include <QtDebug>
#include <QString>
#include <QList>
#include <QMap>

#include "util/math.h"
#include "analyzer/analyzerrhythmstats.h"

namespace {
constexpr bool sDebug = false;
constexpr int kHistogramDecimalPlaces = 2;
const double kHistogramDecimalScale = pow(10.0, kHistogramDecimalPlaces);
// We look for the first beat whitin this to be the first beat
constexpr double kCorrectBeatLocalBpmEpsilon = 0.05; //0.2;
// We use a filter to compute the tempo values +- 1 from the median
// This way we considers and wights the results that have jitter
constexpr double kBpmFilterTolerance = 1.0;
// Just to demonstrate how you would count the beats manually
//    Beat numbers:   1  2  3  4   5  6  7  8    9
//    Beat positions: ?  ?  ?  ?  |?  ?  ?  ?  | ?
// Usually one measures the time of N beats. One stops the timer just before
// the (N+1)th beat begins.  The BPM is then computed by 60*N/<time needed
// to count N beats (in seconds)> 
// This value of 12 was taken from old beatutils class and was manually derived
constexpr int kBeatsToCountTempo = 12;
// This is the max error we allow for rouding the bpm to a integer
constexpr double kMaxBpmError = 0.06;
// This is the max error we allow for comparing if two tempos are equal
constexpr double kMaxBpmCompareError = 0.24;
// a constant tempo will have most of the values equal the mode
// while on a unsteady it should be more uniformally distribuited
// this value was empirical derived to determine if tempo is unsteady
constexpr double kMaxModePercentage = 0.4;
} // namespace

double AnalyzerRhythm::tempoMedian(const QVector<double>& beats, int beatWindow) {
    if (beats.size() < 2) {
        return 0;
    }
    auto [tempoList, tempoFrequencyTable] = computeRawTemposAndFrequency(beats, beatWindow);
    // Get the median BPM.
    std::sort(tempoList.begin(), tempoList.end());
    return BeatStatistics::median(tempoList);
}

double AnalyzerRhythm::tempoMode(const QVector<double> &beats, int beatWindow) {
    if (beats.size() < 2) {
        return 0;
    }
    auto [tempoList, tempoFrequencyTable] = computeRawTemposAndFrequency(beats, beatWindow);
    QMapIterator<double, int> tempo(tempoFrequencyTable);
    return BeatStatistics::mode(tempoFrequencyTable);
}

std::tuple<QList<double>, QMap<double, int>> AnalyzerRhythm::computeRawTemposAndFrequency(
        const QVector<double> &beats, const int beatWindow) {
    
    QList<double> tempoList;
    QMap<double, int> tempoFrequencyTable;
    for (int i = 0; i < beats.size(); i += 1) {
        double start_sample;
        double end_sample;
        // boundaries check..
        if (i < beatWindow) {
            start_sample = beats.at(i);
            end_sample = beats.at(i + beatWindow);
        } else if (i + beatWindow > beats.size()-1) {
            start_sample = beats.at(i - beatWindow);
            end_sample = beats.at(i);
        } else {
            if (beatWindow % 2 == 0) {
                start_sample = beats.at(i - (beatWindow/2));
                end_sample = beats.at(i + (beatWindow/2));
            } else {
                start_sample = beats.at(i - (beatWindow/2));
                end_sample = beats.at(i + (beatWindow/2) + 1);
            }
        }
        // Time needed to count beatWindow beats
        double time = (end_sample - start_sample) / m_iSampleRate;
        if (time == 0) continue;
        double localBpm = 60.0 * beatWindow / time;
        // round BPM to have two decimal places
        double roundedBpm = floor(localBpm * kHistogramDecimalScale + 0.5) /
                kHistogramDecimalScale;
        tempoList << roundedBpm;
        tempoFrequencyTable[roundedBpm] += 1;
    }
    return std::make_tuple(tempoList, tempoFrequencyTable);
}

void AnalyzerRhythm::findTempoChanges() {

    auto sortedTempoList = m_rawTempos;
    std::sort(sortedTempoList.begin(), sortedTempoList.end());
    // We have to make sure we have odd numbers
    if (!(sortedTempoList.size() % 2) and sortedTempoList.size() > 1) {
        sortedTempoList.pop_back();
    }
    // Since we use the median as a guess first and last tempo
    // And it can not have values outside from tempoFrequency
    const double median = BeatStatistics::median(sortedTempoList);
    // Forming a meter perception takes a few seconds, so we assume 
    // sections of consistent metrical structure to be at least around 5s
    int beatsToFilterMeterChanges =  (5 / (60 / median)) * 2; 
    if (!(beatsToFilterMeterChanges % 2)) {beatsToFilterMeterChanges += 1;}
    MovingMedian filterTempo(beatsToFilterMeterChanges); 
    MovingMode stabilzeTempo(beatsToFilterMeterChanges);
    int currentBeat = -1;
    int lastBeatChange = 0;
    m_stableTemposByPositions[lastBeatChange] = median;
    for (double tempo : m_rawTempos) {
        currentBeat += 1;
        double newStableTempo = stabilzeTempo(filterTempo(tempo));
        // The analyzer has some jitter that causes a steady beat to fluctuate around the correct
        // value so we don't consider changes to a neighboor value in the ordered tempoFrequency table
        if (newStableTempo == m_stableTemposByPositions.last()) {
            continue;
        // Here we check if the new tempo is the right neighboor of the previous tempo
        } else if (m_stableTemposByPositions.last() != m_rawTemposFrenquency.lastKey() and
                newStableTempo == (m_rawTemposFrenquency.find(m_stableTemposByPositions.last()) + 1).key()) {
            continue;
        // Here we check if the new tempo is the left neighboor of the previous tempo
        } else if (m_stableTemposByPositions.last() != m_rawTemposFrenquency.firstKey() and
                newStableTempo == (m_rawTemposFrenquency.find(m_stableTemposByPositions.last()) - 1).key()) {
            continue;
        } else {
            // This may not be case when our median window is even, we can't use it
            // Because find will return an iterator pointing to end that we will *
            if (m_rawTemposFrenquency.contains(newStableTempo)) {
                lastBeatChange = currentBeat - filterTempo.lag() - stabilzeTempo.lag();
                m_stableTemposByPositions[lastBeatChange] = newStableTempo;
            }
        }
    }
    m_stableTemposByPositions[m_rawTempos.count() -1] = median;
}

std::tuple <QVector<double>, QMap<int, double>> AnalyzerRhythm::FixBeatsPositions() {
    QVector<double> fixedBeats;
    QMap<int, double> beatsWithNewTempo;
    // here we have only the borders of where the tempo
    // has significant chages, but inside each block we 
    // don't know if the tempo is constant, or is slowing
    // in|de-creasing, or if the drummer is not keeping up
    // with the beat, so let's find out
    auto tempoChanges = m_stableTemposByPositions.keys();
    for (int lastTempoChage = 0;
            lastTempoChage < tempoChanges.count() - 1;
            lastTempoChage++) {
        int beatStart = tempoChanges[lastTempoChage];
        int beatEnd = tempoChanges[lastTempoChage + 1];
        int partLenght = beatEnd - beatStart;
        double leftRighDiff = 0.0;
        double modePercentage = 1.0;
        // here we detect if the segment has a constant tempo or not
        if (partLenght > m_beatsPerBar * 2) {
            int middle = partLenght / 2;            
            auto beatsAtLeft = QVector<double>::fromStdVector(std::vector<double>(
                    m_resultBeats.begin() + beatStart, m_resultBeats.begin() + beatStart + middle));
            auto beatsAtRight = QVector<double>::fromStdVector(std::vector<double>(
                    m_resultBeats.begin() + beatStart + middle, m_resultBeats.begin() + beatEnd));

            auto [temposLeft, temposFrequenciesLeft] =
                    computeRawTemposAndFrequency(beatsAtLeft, kBeatsToCountTempo);
            auto [temposRight, temposFrequenciesRight] =
                    computeRawTemposAndFrequency(beatsAtRight, kBeatsToCountTempo);
            double modeAtLeft = BeatStatistics::mode(temposFrequenciesLeft);
            double modeAtRight = BeatStatistics::mode(temposFrequenciesRight);
            // here we detect if the tempo is in|de-creassing 
            leftRighDiff = fabs(modeAtLeft - modeAtRight);
            
            // here we detect if the tempo is unsteady
            int totalFrequenciesAtLeft = 0;
            for (auto freq : temposFrequenciesLeft) {
                totalFrequenciesAtLeft += freq;
            }
            int totalFrequenciesAtRight = 0;
            for (auto freq : temposFrequenciesRight) {
                totalFrequenciesAtRight += freq;
            }
            int modeFrequencyAtLeft = temposFrequenciesLeft[modeAtLeft];
            int modeFrequencyAtRight = temposFrequenciesLeft[modeAtRight];
            modePercentage = (modeFrequencyAtLeft + modeFrequencyAtRight)
                    / static_cast<double>(totalFrequenciesAtLeft + totalFrequenciesAtRight);
        }
        if (leftRighDiff > kMaxBpmCompareError || modePercentage < kMaxModePercentage) { 
            // it's not constant, we make a beat for each bar
            while (beatStart <= beatEnd - m_beatsPerBar) {
                auto splittedAtMeasure = QVector<double>::fromStdVector(std::vector<double>(
                        m_resultBeats.begin() + beatStart, 
                        m_resultBeats.begin() + beatStart + m_beatsPerBar));
                double measureBpm = calculateBpm(splittedAtMeasure);
                beatsWithNewTempo[beatStart] = measureBpm;
                fixedBeats << calculateFixedTempoGrid(
                        splittedAtMeasure, measureBpm, false);
                beatStart += m_beatsPerBar;
            }
        } else {
            // here the bpm is constant for whole segment
            auto splittedAtTempoChange = QVector<double>::fromStdVector(std::vector<double>(
            m_resultBeats.begin() + beatStart, m_resultBeats.begin() + beatEnd));
            double bpm = calculateBpm(splittedAtTempoChange);
            if (splittedAtTempoChange.size() > kBeatsToCountTempo) {
                fixedBeats << calculateFixedTempoGrid(splittedAtTempoChange, bpm, true);
            } else {
                fixedBeats << calculateFixedTempoGrid(splittedAtTempoChange, bpm, false);
            }
            beatsWithNewTempo[beatStart] = bpm;
        }
    }
    return std::make_tuple(fixedBeats, beatsWithNewTempo);
}

QVector<double> AnalyzerRhythm::calculateFixedTempoGrid(
        const QVector<double> &rawbeats, const double localBpm, bool correctFirst) {
    
    if (rawbeats.size() == 0) {
        return rawbeats;
    }
    // Length of a beat at localBpm in mono samples.
    const double beat_length = floor(((60.0 * m_iSampleRate) / localBpm) + 0.5);
    double firstCorrectBeat = rawbeats.first();
    if (correctFirst) {
        firstCorrectBeat = findFirstCorrectBeat(rawbeats, localBpm);
    }
    // We start building a fixed beat grid at localBpm and the first beat from
    // rawbeats that matches localBpm.
    double beatOffset = firstCorrectBeat;
    QVector<double> fixedBeats;

    for (int i = 0; i < rawbeats.count(); i++) {
        fixedBeats << beatOffset;
        beatOffset += beat_length;
    }
    // Here we add the beats that were before our first "correct" beat to the grid
    beatOffset = firstCorrectBeat - beat_length;
    while (beatOffset > rawbeats.first()) {
        // this runs in O(n) for each call
        // TODO(Cristiano) Use QList instead of QVector?
        fixedBeats.prepend(beatOffset);
        beatOffset -= beat_length;
    }
    //qDebug() << fixedBeats;
    return fixedBeats;
}

double AnalyzerRhythm::findFirstCorrectBeat(
    const QVector<double> rawbeats, const double global_bpm) {
    for (int i = kBeatsToCountTempo; i < rawbeats.size(); i++) {
        // get start and end sample of the beats
        double start_sample = rawbeats.at(i-kBeatsToCountTempo);
        double end_sample = rawbeats.at(i);
        // The time in seconds represented by this sample range.
        double time = (end_sample - start_sample)/m_iSampleRate;
        // Average BPM within this sample range.
        double avg_bpm = 60.0 * kBeatsToCountTempo / time;
        // If the local BPM is within kCorrectBeatLocalBpmEpsilon of the global
        // BPM then use this window as the first beat.
        if (fabs(global_bpm - avg_bpm) <= kCorrectBeatLocalBpmEpsilon) {
            //qDebug() << "Using beat " << (i-N) << " as first beat";
            return start_sample;
        }
    }
    // If we didn't find any beat that matched the window, return the first
    // beat.
    return !rawbeats.empty() ? rawbeats.first() : 0.0;
}

double AnalyzerRhythm::calculateBpm(const QVector<double>& beats, bool tryToRound) {
    
    if (beats.size() < 2) {
        return 0;
    }
    // If we don't have enough beats for our regular approach, just divide the #
    // of beats by the duration in minutes.
    if (beats.size() <= kBeatsToCountTempo) {
        return 60.0 * (beats.size()-1) * m_iSampleRate / (beats.last() - beats.first());
    }
    auto [tempoList, tempoFrequency] = computeRawTemposAndFrequency(beats, kBeatsToCountTempo);
    auto sortedTempo = tempoList;
    std::sort(sortedTempo.begin(), sortedTempo.end());

    double median = BeatStatistics::median(sortedTempo);
    // Okay, let's consider the median an estimation of the BPM. 
    // We know that the jitter of the analyzer will regulary detect
    // a beat to be a bit shifted to left or the right so we use them
    auto [filterWeightedAverageBpm, filtered_bpm_frequency_table] =
            computeFilteredWeightedAverage(tempoFrequency, median);
    if (sDebug) {
        qDebug() << "Statistical median BPM: " << median;
        qDebug() << "Weighted Avg of BPM values +- 1BPM from the media"
                 << filterWeightedAverageBpm;
    }
    auto perfectAverageBpm = filterWeightedAverageBpm;
    // Round values that are within BPM_ERROR of a whole number.
    const double rounded_bpm = floor(perfectAverageBpm + 0.5);
    const double bpm_diff = fabs(rounded_bpm - perfectAverageBpm);
    bool perform_rounding = (bpm_diff <= kMaxBpmError);
    // Finally, restrict the BPM to be within min_bpm and max_bpm.
    const double maybeRoundedBpm = (perform_rounding && tryToRound) 
            ? rounded_bpm : perfectAverageBpm;
    if (sDebug) {
        qDebug() << "SampleMedianBpm=" << median;
        qDebug() << "FilterWeightedAverageBpm=" << filterWeightedAverageBpm;
        qDebug() << "Perfect BPM=" << perfectAverageBpm;
        qDebug() << "Rounded Perfect BPM=" << rounded_bpm;
        qDebug() << "Rounded difference=" << bpm_diff;
        qDebug() << "Perform rounding=" << (perform_rounding && tryToRound);
     }
     return maybeRoundedBpm;
}

std::tuple<double, QMap<double, int>> AnalyzerRhythm::computeFilteredWeightedAverage(
    const QMap<double, int> &tempoFrequency, const double filterCenter) {
    double avarageAccumulator = 0.0;
    int totalWeight = 0;
    QMap<double, int> filteredTempoFrequency;
    QMapIterator<double, int> tempoIterator(tempoFrequency);

    while (tempoIterator.hasNext()) {
        tempoIterator.next();
        double tempo = tempoIterator.key();
        int frequency = tempoIterator.value();

        if (fabs(tempo - filterCenter) <= kBpmFilterTolerance) {
            // TODO(raffitea): Why > 1 ?
            if (frequency > 1) {
                totalWeight += frequency;
                avarageAccumulator += tempo * frequency;
                filteredTempoFrequency[tempo] = frequency;
                if (sDebug) {
                    qDebug() << "Filtered Table:" << tempo
                             << "Frequency:" << frequency;
                }
            }
        }
    }
    if (sDebug) {
        qDebug() << "Sum of filtered frequencies: " << totalWeight;
    }
    double filteredWeightedAverage;
    if (totalWeight == 0) {
        filteredWeightedAverage = filterCenter;
    } else {
        filteredWeightedAverage = avarageAccumulator / static_cast<double>(totalWeight);
    }
    return std::make_tuple(filteredWeightedAverage, filteredTempoFrequency);
}
#include "metronomeeffect.h"

#include <cmath>
#include <optional>
#include <span>

#include "audio/types.h"
#include "effects/backends/effectmanifest.h"
#include "engine/effects/engineeffectparameter.h"
#include "engine/engine.h"
#include "metronomeclick.h"
#include "util/sample.h"
#include "util/types.h"

namespace {

std::size_t playMonoSamples(std::span<const CSAMPLE> monoSource, std::span<CSAMPLE> output) {
    const std::size_t outputBufferFrames = output.size() / mixxx::kEngineChannelCount;
    std::size_t framesPlayed = std::min(monoSource.size(), outputBufferFrames);
    SampleUtil::addMonoToStereo(output.data(), monoSource.data(), framesPlayed);
    return framesPlayed;
}

template<class T>
std::span<T> subspan_clamped(std::span<T> in, typename std::span<T>::size_type offset) {
    // TODO (Swiftb0y): should we instead create a wrapper type that implements
    // UB-free "clamped" operations?
    return in.subspan(std::min(offset, in.size()));
}

std::size_t samplesPerBeat(mixxx::audio::SampleRate sampleRate, double bpm) {
    double samplesPerMinute = sampleRate * 60;
    return static_cast<std::size_t>(samplesPerMinute / bpm);
}

std::span<CSAMPLE> syncedClickOutput(double beatFractionBufferEnd,
        std::optional<GroupFeatureBeatLength> beatLengthAndScratch,
        std::span<CSAMPLE> output) {
    if (!beatLengthAndScratch.has_value() || beatLengthAndScratch->scratch_rate == 0.0) {
        return {};
    }
    double beatLength = beatLengthAndScratch->frames / beatLengthAndScratch->scratch_rate;

    const bool needsPreviousBeat = beatLength < 0;
    double beatToBufferEndFrames = std::abs(beatLength) *
            (needsPreviousBeat ? (1 - beatFractionBufferEnd)
                               : beatFractionBufferEnd);
    std::size_t beatToBufferEndSamples =
            static_cast<std::size_t>(beatToBufferEndFrames) *
            mixxx::kEngineChannelCount;

    if (beatToBufferEndSamples <= output.size()) {
        return output.last(beatToBufferEndSamples);
    }
    return {};
}

} // namespace

// static
QString MetronomeEffect::getId() {
    return "org.mixxx.effects.metronome";
}

// static
EffectManifestPointer MetronomeEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Metronome"));
    pManifest->setAuthor("The Mixxx Team");
    pManifest->setVersion("1.0");
    pManifest->setDescription(QObject::tr("Adds a metronome click sound to the stream"));
    pManifest->setEffectRampsFromDry(true);

    // Period
    // The maximum is at 128 + 1 allowing 128 as max value and
    // enabling us to pause time when the parameter is above
    EffectManifestParameterPointer period = pManifest->addParameter();
    period->setId("bpm");
    period->setName(QObject::tr("BPM"));
    period->setDescription(QObject::tr("Set the beats per minute value of the click sound"));
    period->setValueScaler(EffectManifestParameter::ValueScaler::Logarithmic);
    period->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    period->setRange(40, 120, 208);

    // Period unit
    EffectManifestParameterPointer periodUnit = pManifest->addParameter();
    periodUnit->setId("sync");
    periodUnit->setName(QObject::tr("Sync"));
    periodUnit->setDescription(QObject::tr(
            "Synchronizes the BPM with the track if it can be retrieved"));
    periodUnit->setValueScaler(EffectManifestParameter::ValueScaler::Toggle);
    periodUnit->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    periodUnit->setRange(0, 1, 1);

    return pManifest;
}

void MetronomeEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBpmParameter = parameters.value("bpm");
    m_pSyncParameter = parameters.value("sync");
}

void MetronomeEffect::processChannel(
        MetronomeGroupState* pGroupState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    if (enableState == EffectEnableState::Disabled) {
        // assume click is fully played
        return;
    }

    auto output = std::span<CSAMPLE>(pOutput, engineParameters.samplesPerBuffer());

    MetronomeGroupState* gs = pGroupState;

    const std::span<const CSAMPLE> click = clickForSampleRate(engineParameters.sampleRate());

    if (pOutput != pInput) {
        SampleUtil::copy(pOutput, pInput, engineParameters.samplesPerBuffer());
    }

    const bool shouldSync = m_pSyncParameter->toBool();
    const bool hasBeatInfo = groupFeatures.beat_length.has_value() &&
            groupFeatures.beat_fraction_buffer_end.has_value();

    if (enableState == EffectEnableState::Enabling) {
        if (shouldSync && hasBeatInfo) {
            // Skip first click and sync phase
            gs->framesSinceLastClick = click.size();
        } else {
            gs->framesSinceLastClick = 0;
        }
    }

    playMonoSamples(subspan_clamped(click, gs->framesSinceLastClick), output);
    gs->framesSinceLastClick += engineParameters.framesPerBuffer();

    std::span<CSAMPLE> outputBufferOffset = [&] {
        if (shouldSync && hasBeatInfo) {
            return syncedClickOutput(*groupFeatures.beat_fraction_buffer_end,
                    groupFeatures.beat_length,
                    output);
        } else {
            std::size_t samplesSinceLastClick =
                    gs->framesSinceLastClick * mixxx::kEngineChannelCount;
            std::size_t offset = samplesSinceLastClick %
                    samplesPerBeat(engineParameters.sampleRate(),
                            m_pBpmParameter->value());
            return subspan_clamped(output, offset);
        }
    }();

    if (!outputBufferOffset.empty()) {
        gs->framesSinceLastClick = playMonoSamples(click, outputBufferOffset);
    }
}

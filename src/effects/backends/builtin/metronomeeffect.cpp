#include "metronomeeffect.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>

#include "audio/types.h"
#include "effects/backends/effectmanifest.h"
#include "effects/backends/effectmanifestparameter.h"
#include "engine/effects/engineeffectparameter.h"
#include "engine/engine.h"
#include "metronomeclick.h"
#include "util/math.h"
#include "util/sample.h"
#include "util/types.h"

namespace {

std::size_t playMonoSamplesWithGain(std::span<const CSAMPLE> monoSource,
        std::span<CSAMPLE> output,
        CSAMPLE_GAIN gain) {
    const std::size_t outputBufferFrames = output.size() / mixxx::kEngineChannelOutputCount;
    std::size_t framesPlayed = std::min(monoSource.size(), outputBufferFrames);
    SampleUtil::addMonoToStereoWithGain(gain, output.data(), monoSource.data(), framesPlayed);
    return framesPlayed;
}

constexpr std::size_t framesPerBeat(
        mixxx::audio::SampleRate framesPerSecond, double beatsPerMinute) {
    double framesPerMinute = framesPerSecond * 60;
    return static_cast<std::size_t>(framesPerMinute / beatsPerMinute);
}

std::size_t getbeatToBufferEndSamplesSynced(
        double beatFractionBufferEnd,
        const GroupFeatureBeatLength& beatLengthAndScratch) {
    if (beatLengthAndScratch.scratch_rate == 0.0) {
        // track paused, nothing to do.
        return 0;
    }
    double beatLength = beatLengthAndScratch.frames / beatLengthAndScratch.scratch_rate;
    const bool needsPreviousBeat = beatLength < 0;
    double beatToBufferEndFrames = std::abs(beatLength) *
            (needsPreviousBeat ? (1 - beatFractionBufferEnd)
                               : beatFractionBufferEnd);
    return static_cast<std::size_t>(beatToBufferEndFrames) *
            mixxx::kEngineChannelOutputCount;
}

std::size_t getbeatToBufferEndSamplesFromBpm(
        MetronomeGroupState* pGs,
        const mixxx::EngineParameters& engineParameters,
        double bpm) {
    // EngineParameters::sampleRate in reality returns the framerate.
    mixxx::audio::SampleRate framesPerSecond = engineParameters.sampleRate();

    std::size_t offset = (pGs->framesSinceLastClick %
                                 framesPerBeat(framesPerSecond, bpm)) *
            mixxx::kEngineChannelOutputCount;
    return engineParameters.samplesPerBuffer() - offset;
}

} // namespace

// static
QString MetronomeEffect::getId() {
    return QStringLiteral("org.mixxx.effects.metronome");
}

// static
EffectManifestPointer MetronomeEffect::getManifest() {
    auto pManifest = EffectManifestPointer::create();
    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Metronome"));
    pManifest->setAuthor(QObject::tr("The Mixxx Team"));
    pManifest->setVersion(QStringLiteral("1.0"));
    pManifest->setDescription(QObject::tr("Adds a metronome click sound to the stream"));
    pManifest->setEffectRampsFromDry(true);

    // Period
    // The maximum is at 128 + 1 allowing 128 as max value and
    // enabling us to pause time when the parameter is above
    EffectManifestParameterPointer period = pManifest->addParameter();
    period->setId(QStringLiteral("bpm"));
    period->setName(QObject::tr("BPM"));
    period->setDescription(QObject::tr("Set the beats per minute value of the click sound"));
    period->setValueScaler(EffectManifestParameter::ValueScaler::Logarithmic);
    period->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    period->setRange(40, 120, 208);

    // Period unit
    EffectManifestParameterPointer periodUnit = pManifest->addParameter();
    periodUnit->setId(QStringLiteral("sync"));
    periodUnit->setName(QObject::tr("Sync"));
    periodUnit->setDescription(QObject::tr(
            "Synchronizes the BPM with the track if it can be retrieved"));
    periodUnit->setValueScaler(EffectManifestParameter::ValueScaler::Toggle);
    periodUnit->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    periodUnit->setRange(0, 1, 1);

    EffectManifestParameterPointer gain = pManifest->addParameter();
    gain->setId(QStringLiteral("gain"));
    gain->setName(QObject::tr("Gain"));
    gain->setDescription(QObject::tr(
            "Set the gain of metronome click sound"));
    gain->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    gain->setUnitsHint(EffectManifestParameter::UnitsHint::Decibel);
    gain->setDefaultLinkType(EffectManifestParameter::LinkType::Linked);
    gain->setRange(-24.0, 0.0, 3.0); // decibel
    // 0db on the range above, assumes scale is linear default=(max-min)x+min (solve for x)
    // TODO: move this generally to ControlPotmeterBehavior?
    gain->setNeutralPointOnScale(24.0 / 27.0);

    return pManifest;
}

void MetronomeEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBpmParameter = parameters.value(QStringLiteral("bpm"));
    m_pSyncParameter = parameters.value(QStringLiteral("sync"));
    m_pGainParameter = parameters.value(QStringLiteral("gain"));
}

void MetronomeEffect::processChannel(
        MetronomeGroupState* pGs,
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

    const std::span<const CSAMPLE> click = clickForSampleRate(engineParameters.sampleRate());

    if (pOutput != pInput) {
        SampleUtil::copy(pOutput, pInput, engineParameters.samplesPerBuffer());
    }

    const bool shouldSync = m_pSyncParameter->toBool();
    const bool hasBeatInfo = groupFeatures.beat_length.has_value() &&
            groupFeatures.beat_fraction_buffer_end.has_value();

    if (enableState == EffectEnableState::Enabling) {
        if (shouldSync && hasBeatInfo) {
            // Assume that we have no remaining samples to play
            pGs->framesSinceLastClick = click.size();
        } else {
            // Force a immediate click by assuming remaining samples of a full click
            pGs->framesSinceLastClick = 0;
        }
    }

    const CSAMPLE_GAIN gain = db2ratio(static_cast<float>(m_pGainParameter->value()));

    if (pGs->framesSinceLastClick < click.size()) {
        // We have remaining samples from the previous call
        playMonoSamplesWithGain(click.subspan(pGs->framesSinceLastClick), output, gain);
    }
    pGs->framesSinceLastClick += engineParameters.framesPerBuffer();

    const std::size_t beatToBufferEndSamples = shouldSync && hasBeatInfo
            ? getbeatToBufferEndSamplesSynced(
                      *groupFeatures.beat_fraction_buffer_end,
                      *groupFeatures.beat_length)
            : getbeatToBufferEndSamplesFromBpm(
                      pGs, engineParameters, m_pBpmParameter->value());

    if (beatToBufferEndSamples > 0 && beatToBufferEndSamples <= output.size()) {
        // We have a beat in this call
        pGs->framesSinceLastClick = playMonoSamplesWithGain(click,
                output.last(beatToBufferEndSamples),
                gain);
    }
}

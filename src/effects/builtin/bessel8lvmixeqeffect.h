#pragma once

#include <QMap>

#include "control/pollingcontrolproxy.h"
#include "effects/builtin/lvmixeqbase.h"
#include "effects/effect.h"
#include "effects/effectprocessor.h"
#include "engine/effects/engineeffect.h"
#include "engine/effects/engineeffectparameter.h"
#include "engine/filters/enginefilterbessel8.h"
#include "engine/filters/enginefilterdelay.h"
#include "util/class.h"
#include "util/defs.h"
#include "util/sample.h"
#include "util/types.h"

class Bessel8LVMixEQEffectGroupState :
        public LVMixEQEffectGroupState<EngineFilterBessel8Low> {
  public:
        Bessel8LVMixEQEffectGroupState(const mixxx::EngineParameters& bufferParameters)
            : LVMixEQEffectGroupState<EngineFilterBessel8Low>(bufferParameters) {
        }
};

class Bessel8LVMixEQEffect : public EffectProcessorImpl<Bessel8LVMixEQEffectGroupState> {
  public:
    Bessel8LVMixEQEffect(EngineEffect* pEffect);
    virtual ~Bessel8LVMixEQEffect();

    static QString getId();
    static EffectManifestPointer getManifest();

    // See effectprocessor.h
    void processChannel(const ChannelHandle& handle,
                        Bessel8LVMixEQEffectGroupState* pState,
                        const CSAMPLE* pInput, CSAMPLE* pOutput,
                        const mixxx::EngineParameters& bufferParameters,
                        const EffectEnableState enableState,
                        const GroupFeatureState& groupFeatureState);

  private:
    QString debugString() const {
        return getId();
    }

    EngineEffectParameter* m_pPotLow;
    EngineEffectParameter* m_pPotMid;
    EngineEffectParameter* m_pPotHigh;

    EngineEffectParameter* m_pKillLow;
    EngineEffectParameter* m_pKillMid;
    EngineEffectParameter* m_pKillHigh;

    PollingControlProxy m_loFreqCorner;
    PollingControlProxy m_hiFreqCorner;

    DISALLOW_COPY_AND_ASSIGN(Bessel8LVMixEQEffect);
};

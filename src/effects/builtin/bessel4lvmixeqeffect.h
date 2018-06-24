#pragma once

#include <QMap>

#include "control/pollingcontrolproxy.h"
#include "effects/builtin/lvmixeqbase.h"
#include "effects/effect.h"
#include "effects/effectprocessor.h"
#include "engine/effects/engineeffect.h"
#include "engine/effects/engineeffectparameter.h"
#include "engine/filters/enginefilterbessel4.h"
#include "engine/filters/enginefilterdelay.h"
#include "util/class.h"
#include "util/defs.h"
#include "util/types.h"

class Bessel4LVMixEQEffectGroupState :
        public LVMixEQEffectGroupState<EngineFilterBessel4Low> {
  public:
      Bessel4LVMixEQEffectGroupState(const mixxx::EngineParameters& bufferParameters)
          : LVMixEQEffectGroupState<EngineFilterBessel4Low>(bufferParameters) {
      }
};

class Bessel4LVMixEQEffect : public EffectProcessorImpl<Bessel4LVMixEQEffectGroupState> {
  public:
    Bessel4LVMixEQEffect(EngineEffect* pEffect);
    virtual ~Bessel4LVMixEQEffect();

    static QString getId();
    static EffectManifestPointer getManifest();

    // See effectprocessor.h
    void processChannel(const ChannelHandle& handle,
                        Bessel4LVMixEQEffectGroupState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(Bessel4LVMixEQEffect);
};

#include <QtDebug>

#include "widget/weffectparameterbase.h"
#include "effects/effectsmanager.h"

WEffectParameterBase::WEffectParameterBase(QWidget* pParent, EffectsManager* pEffectsManager)
        : WLabel(pParent),
          m_pEffectsManager(pEffectsManager) {
    parameterUpdated();
}

void WEffectParameterBase::setEffectKnobParameterSlot(
        EffectParameterSlotBasePointer pEffectKnobParameterSlot) {
    m_pEffectParameterSlot = pEffectKnobParameterSlot;
    if (m_pEffectParameterSlot) {
        connect(m_pEffectParameterSlot.data(),
                &EffectParameterSlotBase::updated,
                this,
                &WEffectParameterBase::parameterUpdated);
    }
    parameterUpdated();
}

void WEffectParameterBase::parameterUpdated() {
    if (m_pEffectParameterSlot) {
        if (!m_pEffectParameterSlot->shortName().isEmpty()) {
            setText(m_pEffectParameterSlot->shortName());
        } else {
            setText(m_pEffectParameterSlot->name());
        }
        setBaseTooltip(QString("%1\n%2").arg(
                       m_pEffectParameterSlot->name(),
                       m_pEffectParameterSlot->description()));
    } else {
        setText(tr("None"));
        setBaseTooltip(tr("No effect loaded."));
    }
}

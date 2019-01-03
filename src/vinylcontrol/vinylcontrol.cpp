#include "vinylcontrol/vinylcontrol.h"
#include "control/controlproxy.h"
#include "control/controlobject.h"

VinylControl::VinylControl(UserSettingsPointer pConfig, QString group)
        : m_pConfig(pConfig),
          m_group(group),
          m_vinylControlInputGain(VINYL_PREF_KEY, "gain"),
          m_playButton(group, "play"),
          m_playPos(group, "playposition"),
          m_trackSamples(group, "track_samples"),
          m_trackSampleRate(group, "track_samplerate"),
          m_vinylSeek(group, "vinylcontrol_seek"),
          m_VCRate(group, "vinylcontrol_rate"),
          m_rateSlider(group, "rate"),
          m_duration(group, "duration"),
          m_mode(group, "vinylcontrol_mode"),
          m_enabled(group, "vinylcontrol_enabled"),
          m_wantenabled(group, "vinylcontrol_wantenabled"),
          m_cueing(group, "vinylcontrol_cueing"),
          m_scratching(group, "vinylcontrol_scratching"),
          m_rateRange(group, "rateRange"),
          m_vinylStatus(group, "vinylcontrol_status"),
          m_rateDir(group, "rate_dir"),
          m_loopEnabled(group, "loop_enabled"),
          m_signalenabled(group, "vinylcontrol_signal_enabled"),
          m_reverseButton(group, "reverse"),
          m_iLeadInTime(m_pConfig->getValueString(
                  ConfigKey(group, "vinylcontrol_lead_in_time")).toInt()),
          m_dVinylPosition(0.0),
          m_fTimecodeQuality(0.0f) {
    bool gainOk = false;
    double gain = m_pConfig->getValueString(ConfigKey(VINYL_PREF_KEY, "gain"))
            .toDouble(&gainOk);
    m_vinylControlInputGain.set(gainOk ? gain : 1.0);

    //Enabled or not -- load from saved value in case vinyl control is restarting
    m_bIsEnabled = m_wantenabled.toBool();
}

bool VinylControl::isEnabled() {
    return m_bIsEnabled;
}

void VinylControl::toggleVinylControl(bool enable) {
    if (m_pConfig) {
        m_pConfig->set(ConfigKey(m_group,"vinylcontrol_enabled"), ConfigValue((int)enable));
    }

    m_enabled.set(enable);

    // Reset the scratch control to make sure we don't get stuck moving forwards or backwards.
    // actually that might be a good thing
    //if (!enable)
    //    controlScratch->slotSet(0.0);
}

VinylControl::~VinylControl() {
    bool wasEnabled = m_bIsEnabled;
    m_enabled.set(false);
    m_vinylStatus.set(VINYL_STATUS_DISABLED);
    if (wasEnabled) {
        // if vinyl control is just restarting, indicate that it should
        // be enabled
        m_wantenabled.set(true);
    }
}

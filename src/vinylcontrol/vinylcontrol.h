#pragma once

#include <QString>

#include "control/controlproxylt.h"
#include "preferences/usersettings.h"
#include "util/types.h"
#include "vinylcontrol/vinylsignalquality.h"

class ControlProxy;

class VinylControl : public QObject {
  public:
    VinylControl(UserSettingsPointer pConfig, const QString& group);
    virtual ~VinylControl();

    virtual void toggleVinylControl(bool enable);
    virtual bool isEnabled();
    virtual void analyzeSamples(CSAMPLE* pSamples, size_t nFrames) = 0;
    virtual bool writeQualityReport(VinylSignalQualityReport* qualityReportFifo) = 0;

  protected:
    virtual float getAngle() = 0;

    UserSettingsPointer m_pConfig;
    const QString m_group;

    // The VC input gain preference.
    ControlProxyLt m_vinylControlInputGain;

    //The ControlObject used to start/stop playback of the song.
    ControlProxyLt m_playButton;
    //The ControlObject used to read the playback position in the song.
    ControlProxyLt m_playPos;
    ControlProxyLt m_trackSamples;
    ControlProxyLt m_trackSampleRate;
    //The ControlObject used to change the playback position in the song.
    ControlProxyLt m_vinylSeek;
    // this rate is used in engine buffer for transport
    // 1.0 = original rate
    ControlProxyLt m_VCRate;
    // Reflects the mean value (filtered for display) used of m_pVCRate during
    // VC and and is used to change the speed/pitch of the song without VC
    // 1.0 = original rate
    ControlProxyLt m_rateRatio;
    // The ControlObject used to get the duration of the current song.
    ControlProxyLt m_duration;
    // The ControlObject used to get the vinyl control mode
    // (absolute/relative/scratch)
    ControlProxyLt m_mode;
    // The ControlObject used to get if the vinyl control is
    // enabled or disabled.
    ControlProxyLt m_enabled;
    // The ControlObject used to get if the vinyl control should try to
    // enable itself
    ControlProxyLt m_wantenabled;
    // Should cueing mode be active?
    ControlProxyLt m_cueing;
    // Is pitch changing very quickly?
    ControlProxyLt m_scratching;
    ControlProxyLt m_vinylStatus;
    // looping enabled?
    ControlProxyLt m_loopEnabled;
    // show the signal in the skin?
    ControlProxyLt m_signalenabled;
    // When the user has pressed the "reverse" button.
    ControlProxyLt m_reverseButton;

    // The lead-in time...
    int m_iLeadInTime;

    // The position of the needle on the record as read by the VinylControl
    // implementation.
    double m_dVinylPosition;

    // Used as a measure of the quality of the timecode signal.
    float m_fTimecodeQuality;

    // Whether this VinylControl instance is enabled.
    bool m_bIsEnabled;
};

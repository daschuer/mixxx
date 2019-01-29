#ifndef MIXER_BASETRACKPLAYER_H
#define MIXER_BASETRACKPLAYER_H

#include <QObject>
#include <QScopedPointer>
#include <QString>

#include "preferences/usersettings.h"
#include "engine/channels/enginechannel.h"
#include "engine/channels/enginedeck.h"
#include "mixer/baseplayer.h"
#include "track/track.h"
#include "util/memory.h"
#include "util/parented_ptr.h"

class EngineMaster;
class ControlObject;
class ControlPotmeter;
class ControlProxy;
class EffectsManager;
class VisualsManager;

// Interface for not leaking implementation details of BaseTrackPlayer into the
// rest of Mixxx. Also makes testing a lot easier.
class BaseTrackPlayer : public BasePlayer {
    Q_OBJECT
  public:
    enum TrackLoadReset {
        RESET_NONE,
        RESET_PITCH,
        RESET_PITCH_AND_SPEED,
        RESET_SPEED
    };

    BaseTrackPlayer(QObject* pParent, const QString& group);
    virtual ~BaseTrackPlayer() {}

    virtual TrackPointer getLoadedTrack() const = 0;

  public slots:
    virtual void slotLoadTrack(TrackPointer pTrack, bool bPlay = false) = 0;

  signals:
    void newTrackLoaded(TrackPointer pLoadedTrack);
    void loadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack);
    void playerEmpty();
    void noPassthroughInputConfigured();
    void noVinylControlInputConfigured();
};

class BaseTrackPlayerImpl : public BaseTrackPlayer {
    Q_OBJECT
  public:
    BaseTrackPlayerImpl(QObject* pParent,
                        UserSettingsPointer pConfig,
                        EngineMaster* pMixingEngine,
                        EffectsManager* pEffectsManager,
                        VisualsManager* pVisualsManager,
                        EngineChannel::ChannelOrientation defaultOrientation,
                        const QString& group,
                        bool defaultMaster,
                        bool defaultHeadphones);
    virtual ~BaseTrackPlayerImpl();

    TrackPointer getLoadedTrack() const final;

    // TODO(XXX): Only exposed to let the passthrough AudioInput get
    // connected. Delete me when EngineMaster supports AudioInput assigning.
    EngineDeck* getEngineDeck() const;

    void setupEqControls();

    // For testing, loads a fake track.
    TrackPointer loadFakeTrack(bool bPlay, double filebpm);

  public slots:
    void slotLoadTrack(TrackPointer track, bool bPlay) final;
    void slotTrackLoaded(TrackPointer pNewTrack, TrackPointer pOldTrack);
    void slotLoadFailed(TrackPointer pTrack, QString reason);
    void slotSetReplayGain(mixxx::ReplayGain replayGain);
    void slotPlayToggled(double);

  private slots:
    void slotPassthroughEnabled(double v);
    void slotVinylControlEnabled(double v);
    void slotWaveformZoomValueChangeRequest(double pressed);
    void slotWaveformZoomUp(double pressed);
    void slotWaveformZoomDown(double pressed);
    void slotWaveformZoomSetDefault(double pressed);

  private:
    void setReplayGain(double value);

    void loadTrack(TrackPointer pTrack);
    TrackPointer unloadTrack();

    void connectLoadedTrack();
    void disconnectLoadedTrack();

    UserSettingsPointer m_pConfig;
    EngineMaster* m_pEngineMaster;
    TrackPointer m_pLoadedTrack;
    EngineDeck* m_pChannel;
    bool m_replaygainPending;

    // Waveform display related controls
    std::unique_ptr<ControlObject> m_pWaveformZoom;
    std::unique_ptr<ControlPushButton> m_pWaveformZoomUp;
    std::unique_ptr<ControlPushButton> m_pWaveformZoomDown;
    std::unique_ptr<ControlPushButton> m_pWaveformZoomSetDefault;

    ControlProxyLt m_loopInPoint;
    ControlProxyLt m_loopOutPoint;
    std::unique_ptr<ControlObject> m_pDuration;

    // TODO() these COs are reconnected during runtime
    // This may lock the engine
    parented_ptr<ControlProxy> m_pFileBPM;
    parented_ptr<ControlProxy> m_pKey;

    ControlProxyLt m_replayGain;
    parented_ptr<ControlProxy> m_pPlay;
    ControlProxyLt m_lowFilter;
    ControlProxyLt m_midFilter;
    ControlProxyLt m_highFilter;
    ControlProxyLt m_lowFilterKill;
    ControlProxyLt m_midFilterKill;
    ControlProxyLt m_highFilterKill;
    ControlProxyLt m_preGain;
    ControlProxyLt m_rateSlider;
    ControlProxyLt m_pitchAdjust;
    ControlProxyLt m_inputConfigured;
    parented_ptr<ControlProxy> m_pPassthroughEnabled;
    parented_ptr<ControlProxy> m_pVinylControlEnabled;
    ControlProxyLt m_vinylControlStatus;
};

#endif // MIXER_BASETRACKPLAYER_H

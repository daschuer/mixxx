#include <gtest/gtest.h>

#include <QtDebug>
#include <QScopedPointer>

#include "mixxxtest.h"
#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "control/controlproxy.h"
#include "engine/loopingcontrol.h"
#include "test/mockedenginebackendtest.h"
#include "util/memory.h"

// Due to rounding errors loop positions should be compared with EXPECT_NEAR instead of EXPECT_EQ.
// NOTE(uklotzde, 2017-12-10): The rounding errors currently only appeared with GCC 7.2.1.
constexpr double kLoopPositionMaxAbsError = 0.000000001;

class LoopingControlTest : public MockedEngineBackendTest {
  public:
    LoopingControlTest()
            : kTrackLengthSamples(300000000) {
    }

  protected:
    void SetUp() override {
        MockedEngineBackendTest::SetUp();
        m_quantizeEnabled.initialize(ConfigKey(m_sGroup1, "quantize"));
        m_quantizeEnabled.set(1.0);
        m_nextBeat.initialize(ConfigKey(m_sGroup1, "beat_next"));
        m_nextBeat.set(-1);
        m_closestBeat.initialize(ConfigKey(m_sGroup1, "beat_closest"));
        m_closestBeat.set(-1);
        m_trackSamples.initialize(ConfigKey(m_sGroup1, "track_samples"));
        m_trackSamples.set(kTrackLengthSamples);
        m_buttonLoopIn.initialize(ConfigKey(m_sGroup1, "loop_in"));
        m_buttonLoopOut.initialize(ConfigKey(m_sGroup1, "loop_out"));
        m_buttonLoopExit.initialize(ConfigKey(m_sGroup1, "loop_exit"));
        m_buttonReloopToggle.initialize(ConfigKey(m_sGroup1, "reloop_toggle"));
        m_buttonReloopAndStop.initialize(ConfigKey(m_sGroup1, "reloop_andstop"));
        m_buttonLoopDouble.initialize(ConfigKey(m_sGroup1, "loop_double"));
        m_buttonLoopHalve.initialize(ConfigKey(m_sGroup1, "loop_halve"));
        m_loopEnabled.initialize(ConfigKey(m_sGroup1, "loop_enabled"));
        m_loopStartPoint.initialize(ConfigKey(m_sGroup1, "loop_start_position"));
        m_loopEndPoint.initialize(ConfigKey(m_sGroup1, "loop_end_position"));
        m_loopScale.initialize(ConfigKey(m_sGroup1, "loop_scale"));
        m_buttonPlay.initialize(ConfigKey(m_sGroup1, "play"));
        m_playPosition.initialize(ConfigKey(m_sGroup1, "playposition"));
        m_buttonBeatMoveForward.initialize(ConfigKey(m_sGroup1, "loop_move_1_forward"));
        m_buttonBeatMoveBackward.initialize(ConfigKey(m_sGroup1, "loop_move_1_backward"));
        m_buttonBeatLoop2Activate.initialize(ConfigKey(m_sGroup1, "beatloop_2_activate"));
        m_buttonBeatLoop4Activate.initialize(ConfigKey(m_sGroup1, "beatloop_4_activate"));
        m_beatLoop1Enabled.initialize(ConfigKey(m_sGroup1, "beatloop_1_enabled"));
        m_beatLoop2Enabled.initialize(ConfigKey(m_sGroup1, "beatloop_2_enabled"));
        m_beatLoop4Enabled.initialize(ConfigKey(m_sGroup1, "beatloop_4_enabled"));
        m_beatLoop64Enabled.initialize(ConfigKey(m_sGroup1, "beatloop_64_enabled"));
        m_beatLoop.initialize(ConfigKey(m_sGroup1, "beatloop"));
        m_beatLoopSize.initialize(ConfigKey(m_sGroup1, "beatloop_size"));
        m_buttonBeatLoopActivate.initialize(ConfigKey(m_sGroup1, "beatloop_activate"));
        m_beatJumpSize.initialize(ConfigKey(m_sGroup1, "beatjump_size"));
        m_buttonBeatJumpForward.initialize(ConfigKey(m_sGroup1, "beatjump_forward"));
        m_buttonBeatJumpBackward.initialize(ConfigKey(m_sGroup1, "beatjump_backward"));
        m_buttonBeatLoopRoll1Activate.initialize(ConfigKey(m_sGroup1, "beatlooproll_1_activate"));
        m_buttonBeatLoopRoll2Activate.initialize(ConfigKey(m_sGroup1, "beatlooproll_2_activate"));
        m_buttonBeatLoopRoll4Activate.initialize(ConfigKey(m_sGroup1, "beatlooproll_4_activate"));
    }

    bool isLoopEnabled() {
        return m_loopEnabled.toBool();
    }

    void seekToSampleAndProcess(double new_pos) {
        m_pChannel1->getEngineBuffer()->queueNewPlaypos(new_pos, EngineBuffer::SEEK_STANDARD);
        ProcessBuffer();
    }

    const int kTrackLengthSamples;
    ControlProxyLt m_nextBeat;
    ControlProxyLt m_closestBeat;
    ControlProxyLt m_quantizeEnabled;
    ControlProxyLt m_trackSamples;
    ControlProxyLt m_buttonLoopIn;
    ControlProxyLt m_buttonLoopOut;
    ControlProxyLt m_buttonLoopExit;
    ControlProxyLt m_buttonReloopToggle;
    ControlProxyLt m_buttonReloopAndStop;
    ControlProxyLt m_buttonLoopDouble;
    ControlProxyLt m_buttonLoopHalve;
    ControlProxyLt m_loopEnabled;
    ControlProxyLt m_loopStartPoint;
    ControlProxyLt m_loopEndPoint;
    ControlProxyLt m_loopScale;
    ControlProxyLt m_playPosition;
    ControlProxyLt m_buttonPlay;
    ControlProxyLt m_buttonBeatMoveForward;
    ControlProxyLt m_buttonBeatMoveBackward;
    ControlProxyLt m_buttonBeatLoop2Activate;
    ControlProxyLt m_buttonBeatLoop4Activate;
    ControlProxyLt m_beatLoop1Enabled;
    ControlProxyLt m_beatLoop2Enabled;
    ControlProxyLt m_beatLoop4Enabled;
    ControlProxyLt m_beatLoop64Enabled;
    ControlProxyLt m_beatLoop;
    ControlProxyLt m_beatLoopSize;
    ControlProxyLt m_buttonBeatLoopActivate;
    ControlProxyLt m_beatJumpSize;
    ControlProxyLt m_buttonBeatJumpForward;
    ControlProxyLt m_buttonBeatJumpBackward;
    ControlProxyLt m_buttonBeatLoopRoll1Activate;
    ControlProxyLt m_buttonBeatLoopRoll2Activate;
    ControlProxyLt m_buttonBeatLoopRoll4Activate;
};

TEST_F(LoopingControlTest, LoopSet) {
    m_loopStartPoint.set(0);
    m_loopEndPoint.set(100);
    seekToSampleAndProcess(50);
    EXPECT_FALSE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(100, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopSetOddSamples) {
    m_loopStartPoint.set(1);
    m_loopEndPoint.set(101.5);
    seekToSampleAndProcess(50);
    EXPECT_EQ(1, m_loopStartPoint.get());
    EXPECT_EQ(101.5, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopInSetInsideLoopContinues) {
    m_loopStartPoint.set(0);
    m_loopEndPoint.set(100);
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    seekToSampleAndProcess(50);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(100, m_loopEndPoint.get());
    m_loopStartPoint.set(10);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(10, m_loopStartPoint.get());
    EXPECT_EQ(100, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopInSetAfterLoopOutStops) {
    m_loopStartPoint.set(0);
    m_loopEndPoint.set(100);
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    seekToSampleAndProcess(50);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(100, m_loopEndPoint.get());
    m_loopStartPoint.set(110);
    EXPECT_FALSE(isLoopEnabled());
    EXPECT_EQ(110, m_loopStartPoint.get());
    EXPECT_EQ(-1, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopOutSetInsideLoopContinues) {
    m_loopStartPoint.set(0);
    m_loopEndPoint.set(100);
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    seekToSampleAndProcess(50);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(100, m_loopEndPoint.get());
    m_loopEndPoint.set(80);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(80, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopOutSetBeforeLoopInIgnored) {
    m_loopStartPoint.set(10);
    m_loopEndPoint.set(100);
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    seekToSampleAndProcess(50);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(10, m_loopStartPoint.get());
    EXPECT_EQ(100, m_loopEndPoint.get());
    m_loopEndPoint.set(0);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(10, m_loopStartPoint.get());
    EXPECT_EQ(100, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopInButton_QuantizeDisabled) {
    m_quantizeEnabled.set(0);
    m_closestBeat.set(100);
    m_nextBeat.set(100);
    seekToSampleAndProcess(50);
    m_buttonLoopIn.set(1);
    m_buttonLoopIn.set(0);
    ProcessBuffer();
    EXPECT_EQ(50, m_loopStartPoint.get());
}

TEST_F(LoopingControlTest, LoopInButton_QuantizeEnabledNoBeats) {
    m_quantizeEnabled.set(1);
    m_closestBeat.set(-1);
    m_nextBeat.set(-1);
    seekToSampleAndProcess(50);
    m_buttonLoopIn.set(1);
    m_buttonLoopIn.set(0);
    EXPECT_EQ(50, m_loopStartPoint.get());
}

TEST_F(LoopingControlTest, LoopInButton_AdjustLoopInPointOutsideLoop) {
    m_loopStartPoint.set(1000);
    m_loopEndPoint.set(2000);
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    m_buttonLoopIn.set(1);
    seekToSampleAndProcess(50);
    m_buttonLoopIn.set(0);
    EXPECT_EQ(50, m_loopStartPoint.get());
}

TEST_F(LoopingControlTest, LoopInButton_AdjustLoopInPointInsideLoop) {
    m_loopStartPoint.set(1000);
    m_loopEndPoint.set(2000);
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    m_buttonLoopIn.set(1);
    seekToSampleAndProcess(1500);
    m_buttonLoopIn.set(0);
    EXPECT_EQ(1500, m_loopStartPoint.get());
}

TEST_F(LoopingControlTest, LoopOutButton_QuantizeDisabled) {
    m_quantizeEnabled.set(0);
    m_closestBeat.set(1000);
    m_nextBeat.set(1000);
    seekToSampleAndProcess(500);
    m_loopStartPoint.set(0);
    m_buttonLoopOut.set(1);
    m_buttonLoopOut.set(0);
    EXPECT_EQ(500, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopOutButton_QuantizeEnabledNoBeats) {
    m_quantizeEnabled.set(1);
    m_closestBeat.set(-1);
    m_nextBeat.set(-1);
    seekToSampleAndProcess(500);
    m_loopStartPoint.set(0);
    m_buttonLoopOut.set(1);
    m_buttonLoopOut.set(0);
    EXPECT_EQ(500, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopOutButton_AdjustLoopOutPointOutsideLoop) {
    m_loopStartPoint.set(1000);
    m_loopEndPoint.set(2000);
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    m_buttonLoopOut.set(1);
    seekToSampleAndProcess(3000);
    m_buttonLoopOut.set(0);
    EXPECT_EQ(3000, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopOutButton_AdjustLoopOutPointInsideLoop) {
    m_loopStartPoint.set(100);
    m_loopEndPoint.set(2000);
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    m_buttonLoopOut.set(1);
    seekToSampleAndProcess(1500);
    m_buttonLoopOut.set(0);
    EXPECT_EQ(1500, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopInOutButtons_QuantizeEnabled) {
    m_pTrack1->setBpm(60.0);
    m_quantizeEnabled.set(1);
    seekToSampleAndProcess(500);
    m_buttonLoopIn.set(1);
    m_buttonLoopIn.set(0);
    EXPECT_EQ(m_closestBeat.get(), m_loopStartPoint.get());

    m_beatJumpSize.set(4);
    m_buttonBeatJumpForward.set(1);
    m_buttonBeatJumpForward.set(0);
    ProcessBuffer();
    m_buttonLoopOut.set(1);
    m_buttonLoopOut.set(0);
    ProcessBuffer();
    EXPECT_EQ(m_closestBeat.get(), m_loopEndPoint.get());
    EXPECT_EQ(4, m_beatLoopSize.get());
    EXPECT_TRUE(m_beatLoop4Enabled.toBool());

    // Check that beatloop_4_enabled is reset to 0 when changing the loop size.
    m_beatJumpSize.set(1);
    m_buttonLoopOut.set(1);
    m_buttonBeatJumpForward.set(1);
    m_buttonBeatJumpForward.set(0);
    m_buttonLoopOut.set(0);
    ProcessBuffer();
    EXPECT_EQ(m_closestBeat.get(), m_loopEndPoint.get());
    EXPECT_FALSE(m_beatLoop4Enabled.toBool());
}

TEST_F(LoopingControlTest, ReloopToggleButton_TogglesLoop) {
    m_quantizeEnabled.set(0);
    m_closestBeat.set(-1);
    m_nextBeat.set(-1);
    seekToSampleAndProcess(500);
    m_loopStartPoint.set(0);
    m_buttonLoopOut.set(1);
    m_buttonLoopOut.set(0);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(500, m_loopEndPoint.get());
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    EXPECT_FALSE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(500, m_loopEndPoint.get());
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(500, m_loopEndPoint.get());
    // Ensure that the Loop Exit button works, and that it doesn't act as a
    // toggle.
    m_buttonLoopExit.set(1);
    m_buttonLoopExit.set(0);
    EXPECT_FALSE(isLoopEnabled());
    m_buttonLoopExit.set(1);
    m_buttonLoopExit.set(0);
    EXPECT_FALSE(isLoopEnabled());
}

TEST_F(LoopingControlTest, ReloopToggleButton_DoesNotJumpAhead) {
    m_loopStartPoint.set(1000);
    m_loopEndPoint.set(2000);
    seekToSampleAndProcess(0);

    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    seekToSampleAndProcess(50);
    EXPECT_LE(m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current, m_loopStartPoint.get());
}

TEST_F(LoopingControlTest, ReloopAndStopButton) {
    m_loopStartPoint.set(1000);
    m_loopEndPoint.set(2000);
    seekToSampleAndProcess(1500);
    m_buttonReloopToggle.set(1);
    m_buttonReloopToggle.set(0);
    m_buttonReloopAndStop.set(1);
    m_buttonReloopAndStop.set(0);
    ProcessBuffer();
    EXPECT_EQ(m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current, m_loopStartPoint.get());
    EXPECT_TRUE(m_loopEnabled.toBool());
}

TEST_F(LoopingControlTest, LoopScale_DoublesLoop) {
    seekToSampleAndProcess(0);
    m_buttonLoopIn.set(1);
    m_buttonLoopIn.set(0);
    seekToSampleAndProcess(500);
    m_buttonLoopOut.set(1);
    m_buttonLoopOut.set(0);
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(500, m_loopEndPoint.get());
    m_loopScale.set(2.0);
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(1000, m_loopEndPoint.get());
    m_loopScale.set(2.0);
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(2000, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopScale_HalvesLoop) {
    m_loopStartPoint.set(0);
    m_loopEndPoint.set(2000);
    seekToSampleAndProcess(1800);
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(2000, m_loopEndPoint.get());
    EXPECT_EQ(1800, m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current);
    EXPECT_FALSE(isLoopEnabled());
    m_loopScale.set(0.5);
    ProcessBuffer();
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(1000, m_loopEndPoint.get());

    // The loop was not enabled so halving the loop should not move the playhead
    // even though it is outside the loop.
    EXPECT_EQ(1800, m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current);

    m_buttonReloopToggle.set(1);
    EXPECT_TRUE(isLoopEnabled());
    m_loopScale.set(0.5);
    ProcessBuffer();
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(500, m_loopEndPoint.get());
    // Since the current sample was out of range of the new loop,
    // the current sample should reseek based on the new loop size.
    double target;
    double trigger = m_pChannel1->getEngineBuffer()->m_pLoopingControl->nextTrigger(
            false, 1800, &target);
    EXPECT_EQ(300, target);
    EXPECT_EQ(1800, trigger);
}

TEST_F(LoopingControlTest, LoopDoubleButton_IgnoresPastTrackEnd) {
    seekToSampleAndProcess(50);
    m_loopStartPoint.set(kTrackLengthSamples / 2.0);
    m_loopEndPoint.set(kTrackLengthSamples);
    EXPECT_EQ(kTrackLengthSamples / 2.0, m_loopStartPoint.get());
    EXPECT_EQ(kTrackLengthSamples, m_loopEndPoint.get());
    m_buttonLoopDouble.set(1);
    m_buttonLoopDouble.set(0);
    EXPECT_EQ(kTrackLengthSamples / 2.0, m_loopStartPoint.get());
    EXPECT_EQ(kTrackLengthSamples, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopDoubleButton_DoublesBeatloopSize) {
    m_pTrack1->setBpm(120.0);
    m_beatLoopSize.set(16.0);
    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    m_buttonLoopDouble.set(1.0);
    m_buttonLoopDouble.set(0.0);
    EXPECT_EQ(32.0, m_beatLoopSize.get());
}

TEST_F(LoopingControlTest, LoopDoubleButton_DoesNotResizeManualLoop) {
    seekToSampleAndProcess(500);
    m_buttonLoopIn.set(1.0);
    m_buttonLoopIn.set(0.0);
    seekToSampleAndProcess(1000);
    m_buttonLoopOut.set(1.0);
    m_buttonLoopOut.set(0.0);
    EXPECT_EQ(500, m_loopStartPoint.get());
    EXPECT_EQ(1000, m_loopEndPoint.get());
    m_buttonLoopDouble.set(1);
    m_buttonLoopDouble.set(0);
    EXPECT_EQ(500, m_loopStartPoint.get());
    EXPECT_EQ(1000, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopDoubleButton_UpdatesNumberedBeatloopActivationControls) {
    m_pTrack1->setBpm(120.0);
    m_beatLoopSize.set(2.0);
    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());
    EXPECT_FALSE(m_beatLoop4Enabled.toBool());

    m_buttonLoopDouble.set(1.0);
    m_buttonLoopDouble.set(0.0);
    EXPECT_FALSE(m_beatLoop2Enabled.toBool());
    EXPECT_TRUE(m_beatLoop4Enabled.toBool());
}

TEST_F(LoopingControlTest, LoopHalveButton_IgnoresTooSmall) {
    ProcessBuffer();
    m_loopStartPoint.set(0);
    m_loopEndPoint.set(40);
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(40, m_loopEndPoint.get());
    m_buttonLoopHalve.set(1);
    m_buttonLoopHalve.set(0);
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(40, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopHalveButton_HalvesBeatloopSize) {
    m_pTrack1->setBpm(120.0);
    m_beatLoopSize.set(64.0);
    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    m_buttonLoopHalve.set(1);
    m_buttonLoopHalve.set(0);
    EXPECT_EQ(32.0, m_beatLoopSize.get());
}

TEST_F(LoopingControlTest, LoopHalveButton_DoesNotResizeManualLoop) {
    seekToSampleAndProcess(500);
    m_buttonLoopIn.set(1.0);
    m_buttonLoopIn.set(0.0);
    seekToSampleAndProcess(1000);
    m_buttonLoopOut.set(1.0);
    m_buttonLoopOut.set(0.0);
    EXPECT_EQ(500, m_loopStartPoint.get());
    EXPECT_EQ(1000, m_loopEndPoint.get());
    m_buttonLoopHalve.set(1);
    m_buttonLoopHalve.set(0);
    EXPECT_EQ(500, m_loopStartPoint.get());
    EXPECT_EQ(1000, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopHalveButton_UpdatesNumberedBeatloopActivationControls) {
    m_pTrack1->setBpm(120.0);
    m_beatLoopSize.set(4.0);
    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    EXPECT_FALSE(m_beatLoop2Enabled.toBool());
    EXPECT_TRUE(m_beatLoop4Enabled.toBool());

    m_buttonLoopHalve.set(1.0);
    m_buttonLoopHalve.set(0.0);
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());
    EXPECT_FALSE(m_beatLoop4Enabled.toBool());
}

TEST_F(LoopingControlTest, LoopMoveTest) {
    m_pTrack1->setBpm(120);
    m_loopStartPoint.set(0);
    m_loopEndPoint.set(300);
    seekToSampleAndProcess(10);
    m_buttonReloopToggle.set(1);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(300, m_loopEndPoint.get());
    EXPECT_EQ(10, m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current);

    // Move the loop out from under the playposition.
    m_buttonBeatMoveForward.set(1.0);
    m_buttonBeatMoveForward.set(0.0);
    ProcessBuffer();
    EXPECT_EQ(44100, m_loopStartPoint.get());
    EXPECT_EQ(44400, m_loopEndPoint.get());
    ProcessBuffer();
    // Should seek to the corresponding offset within the moved loop
    EXPECT_EQ(44110, m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current);

    // Move backward so that the current position is outside the new location of the loop
    m_pChannel1->getEngineBuffer()->queueNewPlaypos(44300, EngineBuffer::SEEK_STANDARD);
    ProcessBuffer();
    m_buttonBeatMoveBackward.set(1.0);
    m_buttonBeatMoveBackward.set(0.0);
    ProcessBuffer();
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_NEAR(300, m_loopEndPoint.get(), kLoopPositionMaxAbsError);
    ProcessBuffer();
    EXPECT_NEAR(200,
            m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current,
            kLoopPositionMaxAbsError);

     // Now repeat the test with looping disabled (should not affect the
    // playhead).
    m_buttonReloopToggle.set(1);
    EXPECT_FALSE(isLoopEnabled());

    // Move the loop out from under the playposition.
    m_buttonBeatMoveForward.set(1.0);
    m_buttonBeatMoveForward.set(0.0);
    ProcessBuffer();
    EXPECT_EQ(44100, m_loopStartPoint.get());
    EXPECT_EQ(44400, m_loopEndPoint.get());
    // Should not seek inside the moved loop when the loop is disabled
    EXPECT_NEAR(200,
            m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current,
            kLoopPositionMaxAbsError);

    // Move backward so that the current position is outside the new location of the loop
    m_pChannel1->getEngineBuffer()->queueNewPlaypos(500, EngineBuffer::SEEK_STANDARD);
    ProcessBuffer();
    m_buttonBeatMoveBackward.set(1.0);
    m_buttonBeatMoveBackward.set(0.0);
    ProcessBuffer();
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_NEAR(300, m_loopEndPoint.get(), kLoopPositionMaxAbsError);
    EXPECT_NEAR(500,
            m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current,
            kLoopPositionMaxAbsError);
}

TEST_F(LoopingControlTest, LoopResizeSeek) {
    // Activating a new loop with a loop active should warp the playposition
    // the same as it does when we scale the loop larger and smaller so we
    // keep in sync with the beat.

    // Disable quantize for this test
    m_quantizeEnabled.set(0.0);

    m_pTrack1->setBpm(23520);
    m_loopStartPoint.set(0);
    m_loopEndPoint.set(600);
    seekToSampleAndProcess(500);
    m_buttonReloopToggle.set(1);
    EXPECT_TRUE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(600, m_loopEndPoint.get());
    EXPECT_EQ(500, m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current);

    // Activate a shorter loop
    m_buttonBeatLoop2Activate.set(1.0);

    ProcessBuffer();

    // The loop is resized and we should have seeked to a mid-beat part of the
    // loop.
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(450, m_loopEndPoint.get());
    ProcessBuffer();
    EXPECT_EQ(50, m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current);

    // But if looping is not enabled, no warping occurs.
    m_loopStartPoint.set(0);
    m_loopEndPoint.set(600);
    seekToSampleAndProcess(500);
    m_buttonReloopToggle.set(1);
    EXPECT_FALSE(isLoopEnabled());
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(600, m_loopEndPoint.get());
    EXPECT_EQ(500, m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current);

    m_buttonBeatLoop2Activate.set(1.0);
    ProcessBuffer();

    EXPECT_EQ(500, m_loopStartPoint.get());
    EXPECT_EQ(950, m_loopEndPoint.get());
    EXPECT_EQ(500, m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current);
}

TEST_F(LoopingControlTest, BeatLoopSize_SetAndToggle) {
    m_pTrack1->setBpm(120.0);
    // Setting beatloop_size should not activate a loop
    m_beatLoopSize.set(2.0);
    EXPECT_FALSE(m_loopEnabled.toBool());

    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());

    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());
}

TEST_F(LoopingControlTest, BeatLoopSize_SetWithoutTrackLoaded) {
    // Eject the track that is automatically loaded by the testing framework
    m_pChannel1->getEngineBuffer()->slotEjectTrack(1.0);
    m_beatLoopSize.set(5.0);
    EXPECT_EQ(5.0, m_beatLoopSize.get());
}

TEST_F(LoopingControlTest, BeatLoopSize_IgnoresPastTrackEnd) {
    // TODO: actually calculate that the beatloop would go beyond
    // the end of the track
    m_pTrack1->setBpm(60.0);
    seekToSampleAndProcess(m_trackSamples.get() - 400);
    m_beatLoopSize.set(64.0);
    EXPECT_NE(64.0, m_beatLoopSize.get());
    EXPECT_FALSE(m_beatLoop64Enabled.toBool());
}

TEST_F(LoopingControlTest, BeatLoopSize_SetsNumberedControls) {
    m_pTrack1->setBpm(120.0);
    m_beatLoopSize.set(2.0);
    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());
    EXPECT_FALSE(m_beatLoop4Enabled.toBool());

    m_beatLoopSize.set(4.0);
    EXPECT_FALSE(m_beatLoop2Enabled.toBool());
    EXPECT_TRUE(m_beatLoop4Enabled.toBool());
    EXPECT_TRUE(m_loopEnabled.toBool());
}

TEST_F(LoopingControlTest, BeatLoopSize_IsSetByNumberedControl) {
    m_pTrack1->setBpm(120.0);
    m_beatLoopSize.set(4.0);
    m_buttonBeatLoop2Activate.set(1.0);
    m_buttonBeatLoop2Activate.set(0.0);
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_EQ(2.0, m_beatLoopSize.get());

    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_EQ(2.0, m_beatLoopSize.get());
}

TEST_F(LoopingControlTest, BeatLoopSize_SetDoesNotStartLoop) {
    m_pTrack1->setBpm(120.0);
    m_beatLoopSize.set(16.0);
    EXPECT_FALSE(m_loopEnabled.toBool());
}

TEST_F(LoopingControlTest, BeatLoopSize_ResizeKeepsStartPosition) {
    seekToSampleAndProcess(50);
    m_pTrack1->setBpm(160.0);
    m_beatLoopSize.set(2.0);
    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    double oldStart = m_loopStartPoint.get();

    ProcessBuffer();

    m_beatLoopSize.set(1.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    double newStart = m_loopStartPoint.get();
    EXPECT_TRUE(oldStart == newStart);
}

TEST_F(LoopingControlTest, BeatLoopSize_ValueChangeDoesNotActivateLoop) {
    seekToSampleAndProcess(50);
    m_pTrack1->setBpm(160.0);
    m_beatLoopSize.set(2.0);
    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    EXPECT_TRUE(m_loopEnabled.toBool());

    m_buttonReloopToggle.set(1.0);
    m_buttonReloopToggle.set(0.0);
    EXPECT_FALSE(m_loopEnabled.toBool());
    m_beatLoopSize.set(4.0);
    EXPECT_FALSE(m_loopEnabled.toBool());
    EXPECT_FALSE(m_beatLoop4Enabled.toBool());
}

TEST_F(LoopingControlTest, BeatLoopSize_ValueChangeResizesBeatLoop) {
    seekToSampleAndProcess(50);
    m_pTrack1->setBpm(160.0);
    m_beatLoopSize.set(2.0);
    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    double oldLoopStart = m_loopStartPoint.get();
    double oldLoopEnd = m_loopEndPoint.get();
    double oldLoopLength = oldLoopEnd - oldLoopStart;

    m_buttonReloopToggle.set(1.0);
    m_buttonReloopToggle.set(0.0);
    EXPECT_FALSE(m_loopEnabled.toBool());
    m_beatLoopSize.set(4.0);

    double newLoopStart = m_loopStartPoint.get();
    double newLoopEnd = m_loopEndPoint.get();
    double newLoopLength = newLoopEnd - newLoopStart;
    EXPECT_EQ(oldLoopStart, newLoopStart);
    EXPECT_NE(oldLoopEnd, newLoopEnd);
    EXPECT_EQ(oldLoopLength * 2, newLoopLength);
}

TEST_F(LoopingControlTest, BeatLoopSize_ValueChangeDoesNotResizeManualLoop) {
    seekToSampleAndProcess(50);
    m_pTrack1->setBpm(160.0);
    m_quantizeEnabled.set(0);
    m_beatLoopSize.set(4.0);
    m_buttonLoopIn.set(1);
    m_buttonLoopIn.set(0);
    seekToSampleAndProcess(500);
    m_buttonLoopOut.set(1);
    m_buttonLoopOut.set(0);
    double oldLoopStart = m_loopStartPoint.get();
    double oldLoopEnd = m_loopEndPoint.get();

    m_beatLoopSize.set(8.0);
    double newLoopStart = m_loopStartPoint.get();
    double newLoopEnd = m_loopEndPoint.get();
    EXPECT_EQ(oldLoopStart, newLoopStart);
    EXPECT_EQ(oldLoopEnd, newLoopEnd);
}

TEST_F(LoopingControlTest, LegacyBeatLoopControl) {
    m_pTrack1->setBpm(120.0);
    m_beatLoop.set(2.0);
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_EQ(2.0, m_beatLoopSize.get());

    m_buttonReloopToggle.set(1.0);
    m_buttonReloopToggle.set(0.0);
    EXPECT_FALSE(m_beatLoop2Enabled.toBool());
    EXPECT_FALSE(m_loopEnabled.toBool());
    EXPECT_EQ(2.0, m_beatLoopSize.get());

    ProcessBuffer();

    m_beatLoop.set(6.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_EQ(6.0, m_beatLoopSize.get());
}

TEST_F(LoopingControlTest, Beatjump_JumpsByBeats) {
    m_pTrack1->setBpm(120.0);
    double beatLength = m_nextBeat.get();
    EXPECT_NE(0, beatLength);

    m_beatJumpSize.set(4.0);
    m_buttonBeatJumpForward.set(1.0);
    m_buttonBeatJumpForward.set(0.0);
    ProcessBuffer();
    EXPECT_EQ(beatLength * 4, m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current);
    m_buttonBeatJumpBackward.set(1.0);
    m_buttonBeatJumpBackward.set(0.0);
    ProcessBuffer();
    EXPECT_EQ(0, m_pChannel1->getEngineBuffer()->m_pLoopingControl->getSampleOfTrack().current);
}

TEST_F(LoopingControlTest, Beatjump_MovesActiveLoop) {
    m_pTrack1->setBpm(120.0);
    m_beatLoopSize.set(4.0);
    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    double beatLength = m_nextBeat.get();
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(beatLength * 4, m_loopEndPoint.get());

    m_beatJumpSize.set(1.0);
    m_buttonBeatJumpForward.set(1.0);
    m_buttonBeatJumpForward.set(0.0);
    EXPECT_EQ(beatLength, m_loopStartPoint.get());
    EXPECT_EQ(beatLength * 5, m_loopEndPoint.get());

    // jump backward with playposition outside the loop should not move the loop
    m_buttonBeatJumpBackward.set(1.0);
    m_buttonBeatJumpBackward.set(0.0);
    EXPECT_EQ(beatLength, m_loopStartPoint.get());
    EXPECT_EQ(beatLength * 5, m_loopEndPoint.get());

    seekToSampleAndProcess(beatLength);
    m_buttonBeatJumpBackward.set(1.0);
    m_buttonBeatJumpBackward.set(0.0);
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(beatLength * 4, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, Beatjump_MovesLoopBoundaries) {
    // Holding down the loop in/out buttons and using beatjump should
    // move only the loop in/out point, but not shift the entire loop forward/backward
    m_pTrack1->setBpm(120.0);
    m_beatLoopSize.set(4.0);
    m_buttonBeatLoopActivate.set(1.0);
    m_buttonBeatLoopActivate.set(0.0);
    double beatLength = m_nextBeat.get();
    EXPECT_EQ(0, m_loopStartPoint.get());
    EXPECT_EQ(beatLength * 4, m_loopEndPoint.get());

    m_buttonLoopIn.set(1.0);
    m_beatJumpSize.set(1.0);
    m_buttonBeatJumpForward.set(1.0);
    m_buttonBeatJumpForward.set(0.0);
    ProcessBuffer();
    m_buttonLoopIn.set(0.0);
    ProcessBuffer();
    EXPECT_EQ(beatLength, m_loopStartPoint.get());
    EXPECT_EQ(beatLength * 4, m_loopEndPoint.get());

    m_buttonLoopOut.set(1.0);
    m_buttonBeatJumpForward.set(1.0);
    m_buttonBeatJumpForward.set(0.0);
    ProcessBuffer();
    m_buttonLoopOut.set(0.0);
    ProcessBuffer();
    EXPECT_EQ(beatLength, m_loopStartPoint.get());
    EXPECT_EQ(beatLength * 2, m_loopEndPoint.get());
}

TEST_F(LoopingControlTest, LoopEscape) {
    m_loopStartPoint.set(100);
    m_loopEndPoint.set(200);
    m_buttonReloopToggle.set(1.0);
    m_buttonReloopToggle.set(0.0);
    ProcessBuffer();
    EXPECT_TRUE(isLoopEnabled());
    // seek outside a loop should disable it
    seekToSampleAndProcess(300);
    EXPECT_FALSE(isLoopEnabled());

    m_buttonReloopToggle.set(1.0);
    m_buttonReloopToggle.set(0.0);
    ProcessBuffer();
    EXPECT_TRUE(isLoopEnabled());
    // seek outside a loop should disable it
    seekToSampleAndProcess(50);
    EXPECT_FALSE(isLoopEnabled());
}

TEST_F(LoopingControlTest, BeatLoopRoll_Activation) {
    m_pTrack1->setBpm(120.0);

    m_buttonBeatLoopRoll2Activate.set(1.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());

    m_buttonBeatLoopRoll2Activate.set(0.0);
    EXPECT_FALSE(m_loopEnabled.toBool());
    EXPECT_FALSE(m_beatLoop2Enabled.toBool());
}

TEST_F(LoopingControlTest, BeatLoopRoll_Overlap) {
    m_pTrack1->setBpm(120.0);

    m_buttonBeatLoopRoll2Activate.set(1.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());

    m_buttonBeatLoopRoll4Activate.set(1.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop4Enabled.toBool());
    EXPECT_FALSE(m_beatLoop2Enabled.toBool());

    m_buttonBeatLoopRoll2Activate.set(0.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop4Enabled.toBool());
    EXPECT_FALSE(m_beatLoop2Enabled.toBool());

    m_buttonBeatLoopRoll4Activate.set(0.0);
    EXPECT_FALSE(m_loopEnabled.toBool());
    EXPECT_FALSE(m_beatLoop4Enabled.toBool());
}

TEST_F(LoopingControlTest, BeatLoopRoll_OverlapStackUnwind) {
    m_pTrack1->setBpm(120.0);

    // start a 2 beat loop roll
    m_buttonBeatLoopRoll2Activate.set(1.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());

    // start a 4 beat loop roll on top of the previous loop
    m_buttonBeatLoopRoll4Activate.set(1.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop4Enabled.toBool());
    EXPECT_FALSE(m_beatLoop2Enabled.toBool());

    // start a 1 beat loop roll on top of the previous loop
    m_buttonBeatLoopRoll1Activate.set(1.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop1Enabled.toBool());
    EXPECT_FALSE(m_beatLoop4Enabled.toBool());

    // stop the 4 beat loop roll, the 1 beat roll should continue
    m_buttonBeatLoopRoll4Activate.set(0.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop1Enabled.toBool());

    // stop the 1 beat loop roll, the 2 beat roll should continue
    m_buttonBeatLoopRoll1Activate.set(0.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop2Enabled.toBool());
    EXPECT_FALSE(m_beatLoop1Enabled.toBool());

    // stop the 2 beat loop roll
    m_buttonBeatLoopRoll2Activate.set(0.0);
    EXPECT_FALSE(m_loopEnabled.toBool());
    EXPECT_FALSE(m_beatLoop2Enabled.toBool());
}

TEST_F(LoopingControlTest, BeatLoopRoll_StartPoint) {
    m_pTrack1->setBpm(120.0);

    // start a 4 beat loop roll, start point should be overridden to play position
    m_loopStartPoint.set(8);
    m_buttonBeatLoopRoll4Activate.set(1.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop4Enabled.toBool());
    EXPECT_EQ(0, m_loopStartPoint.get());

    // move the start point, activate a 1 beat loop roll, new start point be preserved
    m_loopStartPoint.set(8);
    m_buttonBeatLoopRoll1Activate.set(1.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop1Enabled.toBool());
    EXPECT_EQ(8, m_loopStartPoint.get());

    // end the 1 beat loop roll
    m_buttonBeatLoopRoll1Activate.set(0.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_EQ(8, m_loopStartPoint.get());
    EXPECT_TRUE(m_beatLoop4Enabled.toBool());

    // end the 4 beat loop roll
    m_buttonBeatLoopRoll4Activate.set(0.0);
    EXPECT_FALSE(m_loopEnabled.toBool());
    EXPECT_FALSE(m_beatLoop4Enabled.toBool());

    // new loop should start back at 0
    m_buttonBeatLoopRoll4Activate.set(1.0);
    EXPECT_TRUE(m_loopEnabled.toBool());
    EXPECT_TRUE(m_beatLoop4Enabled.toBool());
    EXPECT_EQ(0, m_loopStartPoint.get());
}

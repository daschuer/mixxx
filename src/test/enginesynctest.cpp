// Tests for Master Sync.
// The following manual tests should probably be performed:
// * Quantize mode nudges tracks in sync, whether internal or deck master.
// * Flinging tracks with the waveform should work.
// * vinyl??

#include <string>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <QtDebug>

#include "preferences/usersettings.h"
#include "control/controlobject.h"
#include "control/controlproxylt.h"
#include "engine/controls/bpmcontrol.h"
#include "engine/sync/synccontrol.h"
#include "test/mockedenginebackendtest.h"
#include "test/mixxxtest.h"
#include "track/beatfactory.h"
#include "mixer/basetrackplayer.h"
#include "util/memory.h"


class EngineSyncTest : public MockedEngineBackendTest {
  public:
    std::string getMasterGroup() {
        Syncable* pMasterSyncable = m_pEngineSync->getMasterSyncable();
        if (pMasterSyncable) {
            return pMasterSyncable->getGroup().toStdString();
        }
        return "";
    }
    void assertIsMaster(QString group) {
        if (group == m_sInternalClockGroup) {
            ASSERT_EQ(1,
                      ControlObject::get(ConfigKey(m_sInternalClockGroup,
                                                          "sync_master")));
            ASSERT_EQ(NULL, m_pEngineSync->getMaster());
            ASSERT_EQ(m_sInternalClockGroup, getMasterGroup());
        } else {
            if (group == m_sGroup1) {
                ASSERT_EQ(m_pChannel1, m_pEngineSync->getMaster());
            } else if (group == m_sGroup2) {
                ASSERT_EQ(m_pChannel2, m_pEngineSync->getMaster());
            }
            ASSERT_EQ(group.toStdString(), getMasterGroup());
            ASSERT_EQ(SYNC_MASTER, ControlObject::get(ConfigKey(group, "sync_mode")));
            ASSERT_EQ(1, ControlObject::get(ConfigKey(group, "sync_enabled")));
            ASSERT_EQ(1, ControlObject::get(ConfigKey(group, "sync_master")));
        }
    }

    void assertIsFollower(QString group) {
        ASSERT_EQ(SYNC_FOLLOWER, ControlObject::get(ConfigKey(group, "sync_mode")));
        ASSERT_EQ(1, ControlObject::get(ConfigKey(group, "sync_enabled")));
        ASSERT_EQ(0, ControlObject::get(ConfigKey(group, "sync_master")));
    }

    void assertSyncOff(QString group) {
        if (group == m_sInternalClockGroup) {
            ASSERT_EQ(0,
                      ControlObject::get(ConfigKey(m_sInternalClockGroup,
                                                          "sync_master")));
        } else {
            ASSERT_EQ(SYNC_NONE, ControlObject::get(ConfigKey(group, "sync_mode")));
            ASSERT_EQ(0, ControlObject::get(ConfigKey(group, "sync_enabled")));
            ASSERT_EQ(0, ControlObject::get(ConfigKey(group, "sync_master")));
        }
    }

    void assertNoMaster() {
        ASSERT_EQ(NULL, m_pEngineSync->getMaster());
        ASSERT_EQ(NULL, m_pEngineSync->getMasterSyncable());
    }
};

TEST_F(EngineSyncTest, ControlObjectsExist) {
    // This isn't exhaustive, but certain COs have a habit of not being set up properly.
    ASSERT_TRUE(ControlObject::getControl(ConfigKey(m_sGroup1, "file_bpm")) != NULL);
}

TEST_F(EngineSyncTest, SetMasterSuccess) {
    // If we set the first channel to master, EngineSync should get that message.

    ControlProxyLt buttonMasterSync1(m_sGroup1, "sync_mode");
    buttonMasterSync1.set(SYNC_MASTER);
    ProcessBuffer();

    // The master sync should now be channel 1.
    assertIsMaster(m_sGroup1);

    ControlProxyLt buttonMasterSync2(m_sGroup2, "sync_mode");
    buttonMasterSync2.set(SYNC_FOLLOWER);
    ProcessBuffer();

    assertIsFollower(m_sGroup2);

    // Now set channel 2 to be master.
    buttonMasterSync2.set(SYNC_MASTER);
    ProcessBuffer();

    // Now channel 2 should be master, and channel 1 should be a follower.
    assertIsMaster(m_sGroup2);
    assertIsFollower(m_sGroup1);

    // Now back again.
    buttonMasterSync1.set(SYNC_MASTER);
    ProcessBuffer();

    // Now channel 1 should be master, and channel 2 should be a follower.
    assertIsMaster(m_sGroup1);
    assertIsFollower(m_sGroup2);

    // Now set channel 1 to follower, internal will be master because no track loaded.
    buttonMasterSync1.set(SYNC_FOLLOWER);
    ProcessBuffer();

    assertIsMaster(m_sInternalClockGroup);
    assertIsFollower(m_sGroup1);
    assertIsFollower(m_sGroup2);
}

TEST_F(EngineSyncTest, SetMasterWhilePlaying) {
    // Make sure we don't get two master lights if we change masters while playing.

    ControlObject::set(ConfigKey(m_sGroup1, "file_bpm"), 120.0);
    ControlObject::set(ConfigKey(m_sGroup2, "file_bpm"), 124.0);
    ControlObject::set(ConfigKey(m_sGroup3, "file_bpm"), 128.0);

    ControlProxyLt buttonMasterSync1(m_sGroup1, "sync_mode");
    buttonMasterSync1.set(SYNC_MASTER);
    ControlProxyLt buttonMasterSync2(m_sGroup2, "sync_mode");
    buttonMasterSync2.set(SYNC_FOLLOWER);
    ControlProxyLt buttonMasterSync3(m_sGroup3, "sync_mode");
    buttonMasterSync3.set(SYNC_FOLLOWER);

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup3, "play"), 1.0);

    ProcessBuffer();

    buttonMasterSync3.set(SYNC_MASTER);

    ProcessBuffer();

    assertIsFollower(m_sGroup1);
    assertIsFollower(m_sGroup2);
    assertIsMaster(m_sGroup3);
}

TEST_F(EngineSyncTest, SetEnabledBecomesMaster) {
    // If we set the first channel to follower, it should be master.
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_FOLLOWER);
    ProcessBuffer();

    // The master sync should now be internal.
    assertIsMaster(m_sInternalClockGroup);
}

TEST_F(EngineSyncTest, DisableInternalMasterWhilePlaying) {
    ControlProxyLt buttonMasterSync(m_sInternalClockGroup, "sync_master");
    buttonMasterSync.set(1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_FOLLOWER);
    ProcessBuffer();

    // The master sync should now be Internal.
    assertIsMaster(m_sInternalClockGroup);

    // Make sure deck 1 is playing.
    ControlObject::set(ConfigKey(m_sGroup1, "file_bpm"), 80.0);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ProcessBuffer();

    // Now unset Internal master.
    buttonMasterSync.set(0.0);
    ProcessBuffer();

    // This is not allowed, Internal should still be master.
    assertIsMaster(m_sInternalClockGroup);
    ASSERT_EQ(1, buttonMasterSync.get());
}

TEST_F(EngineSyncTest, DisableSyncOnMaster) {
    // Channel 1 follower, channel 2 master.
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_FOLLOWER);
    ControlObject::set(ConfigKey(m_sGroup2, "sync_master"), 1.0);

    assertIsFollower(m_sGroup1);
    assertIsMaster(m_sGroup2);

    // Unset enabled on channel2, it should work.
    ControlObject::set(ConfigKey(m_sGroup2, "sync_enabled"), 0.0);

    assertIsFollower(m_sGroup1);
    ASSERT_EQ(0, ControlObject::get(ConfigKey(m_sGroup2, "sync_enabled")));
    ASSERT_EQ(0, ControlObject::get(ConfigKey(m_sGroup2, "sync_master")));
}

TEST_F(EngineSyncTest, InternalMasterSetFollowerSliderMoves) {
    // If internal is master, and we turn on a follower, the slider should move.
    ControlObject::set(ConfigKey(m_sInternalClockGroup, "sync_master"), 1);
    ControlObject::set(ConfigKey(m_sInternalClockGroup, "bpm"), 100.0);

    // Set the file bpm of channel 1 to 160bpm.
    ControlObject::set(ConfigKey(m_sGroup1, "file_bpm"), 80.0);

    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_FOLLOWER);
    ProcessBuffer();

    EXPECT_FLOAT_EQ(getRateSliderValue(1.25),
                    ControlObject::get(ConfigKey(m_sGroup1, "rate")));
    EXPECT_FLOAT_EQ(100.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
}

TEST_F(EngineSyncTest, AnySyncDeckSliderStays) {
    // If there exists a sync deck, even if it's not playing, don't change the
    // master BPM if a new deck enables sync.

    ControlObject::set(ConfigKey(m_sGroup1, "file_bpm"), 80.0);
    ControlObject::set(ConfigKey(m_sGroup1, "sync_enabled"), 1.0);

    // After setting up the first deck, the internal BPM should be 80.
    EXPECT_FLOAT_EQ(80.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    ControlObject::set(ConfigKey(m_sGroup2, "file_bpm"), 100.0);
    ControlObject::set(ConfigKey(m_sGroup2, "sync_enabled"), 1.0);

    // After the second one, though, the internal BPM should still be 80.
    EXPECT_FLOAT_EQ(80.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
}

TEST_F(EngineSyncTest, InternalClockFollowsFirstPlayingDeck) {
    // Same as above, except we use the midi lights to change state.
    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");

    // Set up decks so they can be playing, and start deck 1.
    ControlObject::set(ConfigKey(m_sGroup1, "file_bpm"), 100.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "file_bpm"), 130.0);
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 0.0);
    ProcessBuffer();

    // Set channel 1 to be enabled
    buttonSyncEnabled1.set(1.0);
    ProcessBuffer();

    // The master sync should now be internal and the speed should match deck 1.
    assertIsMaster(m_sInternalClockGroup);
    assertIsFollower(m_sGroup1);
    EXPECT_FLOAT_EQ(100.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    // Set channel 2 to be enabled.
    buttonSyncEnabled2.set(1);
    ProcessBuffer();

    assertIsFollower(m_sGroup2);

    // Internal should still be master.
    assertIsMaster(m_sInternalClockGroup);

    // The rate should not have changed -- deck 1 still matches deck 2.
    EXPECT_FLOAT_EQ(getRateSliderValue(1.0),
                    ControlObject::get(ConfigKey(m_sGroup1, "rate")));

    // Reset channel 2 rate, set channel 2 to play, and process a buffer.
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);
    ProcessBuffer();

    // Keep double checking these.
    assertIsMaster(m_sInternalClockGroup);
    assertIsFollower(m_sGroup1);
    assertIsFollower(m_sGroup2);

    // Now disable sync on channel 1.
    buttonSyncEnabled1.set(0);
    ProcessBuffer();

    // Rate should now match channel 2.
    EXPECT_FLOAT_EQ(130.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
}


TEST_F(EngineSyncTest, SetExplicitMasterByLights) {
    // Same as above, except we use the midi lights to change state.
    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");
    ControlProxyLt buttonSyncMaster1(m_sGroup1, "sync_master");
    ControlProxyLt buttonSyncMaster2(m_sGroup2, "sync_master");

    // Set channel 1 to be master.
    buttonSyncMaster1.set(1.0);
    ProcessBuffer();

    // The master sync should now be channel 1.
    assertIsMaster(m_sGroup1);

    // Set channel 2 to be follower.
    buttonSyncEnabled2.set(1);

    assertIsFollower(m_sGroup2);

    // Now set channel 2 to be master.
    buttonSyncMaster2.set(1);

    // Now channel 2 should be master, and channel 1 should be a follower.
    assertIsFollower(m_sGroup1);
    assertIsMaster(m_sGroup2);

    // Now back again.
    buttonSyncMaster1.set(1);

    // Now channel 1 should be master, and channel 2 should be a follower.
    assertIsMaster(m_sGroup1);
    assertIsFollower(m_sGroup2);

    // Now set channel 1 to not-master, internal will be master.
    buttonSyncMaster1.set(0);

    assertIsMaster(m_sInternalClockGroup);
    assertIsFollower(m_sGroup1);
    assertIsFollower(m_sGroup2);
}

TEST_F(EngineSyncTest, RateChangeTest) {
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_MASTER);
    ControlObject::set(ConfigKey(m_sGroup2, "sync_mode"), SYNC_FOLLOWER);
    ProcessBuffer();

    // Set the file bpm of channel 1 to 160bpm.
    ControlObject::set(ConfigKey(m_sGroup1, "file_bpm"), 160.0);
    EXPECT_FLOAT_EQ(160.0, ControlObject::get(ConfigKey(m_sGroup1, "file_bpm")));
    ProcessBuffer();
    EXPECT_FLOAT_EQ(160.0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    // Set the rate of channel 1 to 1.2.
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.2));
    EXPECT_FLOAT_EQ(getRateSliderValue(1.2),
                    ControlObject::get(ConfigKey(m_sGroup1, "rate")));
    EXPECT_FLOAT_EQ(192.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));

    // Internal master should also be 192.
    EXPECT_FLOAT_EQ(192.0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    // Set the file bpm of channel 2 to 120bpm.
    ControlObject::set(ConfigKey(m_sGroup2, "file_bpm"), 120.0);
    EXPECT_FLOAT_EQ(120.0, ControlObject::get(ConfigKey(m_sGroup2, "file_bpm")));

    // rate slider for channel 2 should now be 1.6 = 160 * 1.2 / 120.
    EXPECT_FLOAT_EQ(getRateSliderValue(1.6),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
    EXPECT_FLOAT_EQ(192.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
}

TEST_F(EngineSyncTest, RateChangeTestWeirdOrder) {
    // This is like the test above, but the user loads the track after the slider has been tweaked.
    ControlProxyLt buttonMasterSync1(m_sGroup1, "sync_mode");
    buttonMasterSync1.set(SYNC_MASTER);
    ControlProxyLt buttonMasterSync2(m_sGroup2, "sync_mode");
    buttonMasterSync2.set(SYNC_FOLLOWER);
    ProcessBuffer();

    // Set the file bpm of channel 1 to 160bpm.
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(160.0);
    EXPECT_FLOAT_EQ(160.0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    // Set the file bpm of channel 2 to 120bpm.
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(120.0);

    // Set the rate slider of channel 1 to 1.2.
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.2));

    // Rate slider for channel 2 should now be 1.6 = (160 * 1.2 / 120) - 1.0.
    EXPECT_FLOAT_EQ(getRateSliderValue(1.6),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
    EXPECT_FLOAT_EQ(192.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));

    // Internal Master BPM should read the same.
    EXPECT_FLOAT_EQ(192.0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
}

TEST_F(EngineSyncTest, RateChangeTestOrder3) {
    // Set the file bpm of channel 1 to 160bpm.
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(160.0);
    EXPECT_FLOAT_EQ(160.0, ControlObject::get(ConfigKey(m_sGroup1, "file_bpm")));

    // Set the file bpm of channel 2 to 120bpm.
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(120.0);
    EXPECT_FLOAT_EQ(120.0, ControlObject::get(ConfigKey(m_sGroup2, "file_bpm")));

    // Turn on Master and Follower.
    ControlProxyLt buttonMasterSync1(m_sGroup1, "sync_mode");
    buttonMasterSync1.set(SYNC_MASTER);
    ProcessBuffer();

    assertIsMaster(m_sGroup1);

    ControlProxyLt buttonMasterSync2(m_sGroup2, "sync_mode");
    buttonMasterSync2.set(SYNC_FOLLOWER);
    ProcessBuffer();

    // Follower should immediately set its slider.
    EXPECT_FLOAT_EQ(getRateSliderValue(1.3333333333),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
    EXPECT_FLOAT_EQ(160.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
    EXPECT_FLOAT_EQ(160.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
}

TEST_F(EngineSyncTest, FollowerRateChange) {
    // Confirm that followers can change master sync rate as well.
    ControlProxyLt buttonMasterSync1(m_sGroup1, "sync_mode");
    buttonMasterSync1.set(SYNC_MASTER);
    ControlProxyLt buttonMasterSync2(m_sGroup2, "sync_mode");
    buttonMasterSync2.set(SYNC_FOLLOWER);
    ProcessBuffer();

    // Set the file bpm of channel 1 to 160bpm.
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(160.0);

    // Set the file bpm of channel 2 to 120bpm.
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(120.0);

    // Set the rate slider of channel 1 to 1.2.
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.2));

    // Rate slider for channel 2 should now be 1.6 = (160 * 1.2 / 120).
    EXPECT_FLOAT_EQ(getRateSliderValue(1.6),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
    EXPECT_FLOAT_EQ(192.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));

    // Try to twiddle the rate slider on channel 2.
    ControlProxyLt slider2(m_sGroup2, "rate");
    slider2.set(getRateSliderValue(0.8));
    ProcessBuffer();

    // Rates should still be changed even though it's a follower.
    EXPECT_FLOAT_EQ(getRateSliderValue(0.8),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
    EXPECT_FLOAT_EQ(96.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
    EXPECT_FLOAT_EQ(getRateSliderValue(0.6),
                    ControlObject::get(ConfigKey(m_sGroup1, "rate")));
    EXPECT_FLOAT_EQ(96.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
}

TEST_F(EngineSyncTest, InternalRateChangeTest) {
    ControlProxyLt buttonMasterSyncInternal(m_sInternalClockGroup, "sync_master");
    buttonMasterSyncInternal.set(SYNC_MASTER);
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_FOLLOWER);
    ControlProxyLt buttonMasterSync2(m_sGroup2, "sync_mode");
    buttonMasterSync2.set(SYNC_FOLLOWER);
    ProcessBuffer();

    assertIsMaster(m_sInternalClockGroup);
    assertIsFollower(m_sGroup1);
    assertIsFollower(m_sGroup2);

    // Set the file bpm of channel 1 to 160bpm.
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(160.0);
    EXPECT_FLOAT_EQ(160.0, ControlObject::get(ConfigKey(m_sGroup1, "file_bpm")));

    // Set the file bpm of channel 2 to 120bpm.
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(120.0);
    EXPECT_FLOAT_EQ(120.0, ControlObject::get(ConfigKey(m_sGroup2, "file_bpm")));

    // Set the internal rate to 150.
    ControlProxyLt masterSyncSlider(m_sInternalClockGroup, "bpm");
    masterSyncSlider.set(150.0);
    EXPECT_FLOAT_EQ(150.0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    // Set decks playing, and process a buffer to update all the COs.
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);

    ProcessBuffer();

    // Rate sliders for channels 1 and 2 should change appropriately.
    EXPECT_FLOAT_EQ(getRateSliderValue(0.9375),
                    ControlObject::get(ConfigKey(m_sGroup1, "rate")));
    EXPECT_FLOAT_EQ(150.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(getRateSliderValue(1.25),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
    EXPECT_FLOAT_EQ(150.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
    // TODO(rryan): Re-enable with a true mock of SyncableFollower.
    //EXPECT_FLOAT_EQ(0.9375, ControlObject::get(ConfigKey(m_sGroup1, "rateEngine")));
    //EXPECT_FLOAT_EQ(1.25, ControlObject::get(ConfigKey(m_sGroup2, "rateEngine")));

    // Set the internal rate to 140.
    masterSyncSlider.set(140.0);

    // Update COs again.
    ProcessBuffer();

    // Rate sliders for channels 1 and 2 should change appropriately.
    EXPECT_FLOAT_EQ(getRateSliderValue(.875),
                    ControlObject::get(ConfigKey(m_sGroup1, "rate")));
    EXPECT_FLOAT_EQ(140.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(getRateSliderValue(1.16666667),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
    EXPECT_FLOAT_EQ(140.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
    // TODO(rryan): Re-enable with a true mock of SyncableFollower.
    //EXPECT_FLOAT_EQ(0.5, ControlObject::get(ConfigKey(m_sGroup1, "rateEngine")));
    //EXPECT_FLOAT_EQ(0.6666667, ControlObject::get(ConfigKey(m_sGroup2, "rateEngine")));

}

TEST_F(EngineSyncTest, MasterStopSliderCheck) {
    // If the master is playing, and stop is pushed, the sliders should stay the same.
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(120.0);
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(128.0);

    ControlProxyLt buttonMasterSync1(m_sGroup1, "sync_mode");
    buttonMasterSync1.set(SYNC_MASTER);
    ControlProxyLt buttonMasterSync2(m_sGroup2, "sync_mode");
    buttonMasterSync2.set(SYNC_FOLLOWER);
    ProcessBuffer();

    assertIsMaster(m_sGroup1);
    assertIsFollower(m_sGroup2);

    ControlProxyLt channel1Play(m_sGroup1, "play");
    channel1Play.set(1.0);
    ControlProxyLt channel2Play(m_sGroup2, "play");
    channel2Play.set(1.0);

    ProcessBuffer();

    // TODO(rryan): Re-enable with a true mock of SyncableFollower.
    //EXPECT_FLOAT_EQ(0.9375, ControlObject::get(ConfigKey(m_sGroup2, "rateEngine")));
    EXPECT_FLOAT_EQ(120.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
    EXPECT_FLOAT_EQ(getRateSliderValue(0.9375),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));

    channel1Play.set(0.0);

    ProcessBuffer();

    // TODO(rryan): Re-enable with a true mock of SyncableFollower.
    //EXPECT_FLOAT_EQ(0.0, ControlObject::get(ConfigKey(m_sGroup2, "rateEngine")));
    EXPECT_FLOAT_EQ(120.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
    EXPECT_FLOAT_EQ(getRateSliderValue(0.9375),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
}

TEST_F(EngineSyncTest, EnableOneDeckInitsMaster) {
    // If Internal is master, and we turn sync on a playing deck, the playing deck sets the
    // internal master and the beat distances are now aligned.

    ControlProxyLt buttonMasterSyncInternal(m_sInternalClockGroup, "sync_master");
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");

    // Set internal to master and give it a beat distance.
    ControlObject::set(ConfigKey(m_sInternalClockGroup, "bpm"), 124.0);
    ControlObject::set(ConfigKey(m_sInternalClockGroup, "beat_distance"), 0.5);
    buttonMasterSyncInternal.set(SYNC_MASTER);
    ProcessBuffer();

    // Set up the deck to play.
    fileBpm1.set(130.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup1, "beat_distance"), 0.2);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);

    // Set the deck to follower.  We have to call requestEnableSync directly
    // because calling ProcessBuffer() tries to advance the beat_distance values.
    m_pEngineSync->requestEnableSync(m_pEngineSync->getSyncableForGroup(m_sGroup1), true);

    // Internal should still be master (only one playing deck).
    assertIsMaster(m_sInternalClockGroup);

    // Internal clock rate should match master but beat distance should match follower.
    EXPECT_FLOAT_EQ(130.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(0.2, ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));
    EXPECT_FLOAT_EQ(0.2,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup,
                                                        "beat_distance")));

    // Enable second deck, beat distance should still match original setting.
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(140.0);
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup2, "beat_distance"), 0.2);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);

    //pButtonSyncEnabled2->slotSet(1.0);
    m_pEngineSync->requestEnableSync(m_pEngineSync->getSyncableForGroup(m_sGroup2), true);

    EXPECT_FLOAT_EQ(130.0,
            ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
    EXPECT_FLOAT_EQ(0.2, ControlObject::get(ConfigKey(m_sGroup2, "beat_distance")));
    EXPECT_FLOAT_EQ(0.2,
            ControlObject::get(ConfigKey(m_sInternalClockGroup, "beat_distance")));
}

TEST_F(EngineSyncTest, EnableOneDeckInitializesMaster) {
    // If we turn sync on a playing deck, the playing deck initializes the internal clock master.

    ControlProxyLt buttonMasterSyncInternal(m_sInternalClockGroup, "sync_master");
    ControlProxyLt buttonSyncMasterEnabled1(m_sGroup1, "sync_master");

    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");

    // Set the deck to play.
    fileBpm1.set(130.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup1, "beat_distance"), 0.2);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);

    // Set the deck to follower.
    m_pEngineSync->requestEnableSync(m_pEngineSync->getSyncableForGroup(m_sGroup1), true);

    // Internal should still be master.
    assertIsMaster(m_sInternalClockGroup);

    // Internal clock rate should be set and beat distances reset.
    EXPECT_FLOAT_EQ(130.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(0.2, ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));
    EXPECT_FLOAT_EQ(0.2,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup,
                                                        "beat_distance")));
}

TEST_F(EngineSyncTest, LoadTrackInitializesMaster) {
    // If master sync is on when a track gets loaded, the internal clock
    // may or may not get synced to the new track depending on the state
    // of other decks and whether they have tracks loaded as well.

    // First eject the fake tracks that come with the testing framework.
    m_pChannel1->getEngineBuffer()->slotEjectTrack(1.0);
    m_pChannel2->getEngineBuffer()->slotEjectTrack(1.0);
    m_pChannel3->getEngineBuffer()->slotEjectTrack(1.0);

    // If sync is on and we load a track, that should initialize master.
    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    buttonSyncEnabled1.set(1.0);

    m_pMixerDeck1->loadFakeTrack(false, 140.0);

    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sGroup1, "bpm")));

    // If sync is on two decks and we load a track, that should still initialize
    // master.
    m_pChannel1->getEngineBuffer()->slotEjectTrack(1.0);
    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");
    buttonSyncEnabled2.set(1.0);

    m_pMixerDeck1->loadFakeTrack(false, 128.0);
    EXPECT_FLOAT_EQ(128.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(128.0,
                    ControlObject::get(ConfigKey(m_sGroup1, "bpm")));

    // If sync is on two decks and one deck is loaded but not playing, we should
    // still initialize to that deck.
    m_pMixerDeck2->loadFakeTrack(false, 110.0);
    EXPECT_FLOAT_EQ(128.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(128.0,
                    ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(128.0,
                    ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
}

TEST_F(EngineSyncTest, LoadTrackResetTempoOption) {
    // Make sure playing decks with master sync enabled do not change tempo when
    // the "Reset Speed/Tempo" preference is set and a track is loaded to another
    // deck with master sync enabled.
    m_pConfig->set(ConfigKey("[Controls]", "SpeedAutoReset"),
                   ConfigValue(BaseTrackPlayer::RESET_SPEED));

    // Enable sync on two stopped decks
    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    buttonSyncEnabled1.set(1.0);

    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");
    buttonSyncEnabled2.set(1.0);

    // If sync is on and we load a track, that should initialize master.
    TrackPointer track1 = m_pMixerDeck1->loadFakeTrack(false, 140.0);

    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sGroup1, "bpm")));

    // If sync is on two decks and we load a track while one is playing,
    // that should not change the playing deck.
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);

    TrackPointer track2 = m_pMixerDeck2->loadFakeTrack(false, 128.0);

    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sGroup2, "bpm")));

    // Repeat with RESET_PITCH_AND_SPEED
    m_pConfig->set(ConfigKey("[Controls]", "SpeedAutoReset"),
                   ConfigValue(BaseTrackPlayer::RESET_PITCH_AND_SPEED));
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    m_pMixerDeck1->slotLoadTrack(track1, true);
    m_pMixerDeck1->slotTrackLoaded(track1, m_pTrack1);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    m_pMixerDeck2->slotLoadTrack(track2, false);
    m_pMixerDeck2->slotTrackLoaded(track2, m_pTrack2);
    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sGroup2, "bpm")));

    // Repeat with RESET_NONE
    m_pConfig->set(ConfigKey("[Controls]", "SpeedAutoReset"),
                   ConfigValue(BaseTrackPlayer::RESET_NONE));
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.0));
    m_pMixerDeck1->slotLoadTrack(track1, true);
    m_pMixerDeck1->slotTrackLoaded(track1, m_pTrack1);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    m_pMixerDeck2->slotLoadTrack(track2, false);
    m_pMixerDeck2->slotTrackLoaded(track2, m_pTrack2);
    EXPECT_FLOAT_EQ(128.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(128.0,
                    ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(128.0,
                    ControlObject::get(ConfigKey(m_sGroup2, "bpm")));

    // Load two tracks with sync off and RESET_SPEED
    m_pConfig->set(ConfigKey("[Controls]", "SpeedAutoReset"),
                   ConfigValue(BaseTrackPlayer::RESET_SPEED));
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.5));
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.5));
    buttonSyncEnabled1.set(0.0);
    buttonSyncEnabled2.set(0.0);
    m_pMixerDeck1->slotLoadTrack(track1, true);
    m_pMixerDeck1->slotTrackLoaded(track1, m_pTrack1);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    m_pMixerDeck2->slotLoadTrack(track2, false);
    m_pMixerDeck2->slotTrackLoaded(track2, m_pTrack2);
    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(128.0,
                    ControlObject::get(ConfigKey(m_sGroup2, "bpm")));

    // Load two tracks with sync off and RESET_PITCH_AND_SPEED
    m_pConfig->set(ConfigKey("[Controls]", "SpeedAutoReset"),
                   ConfigValue(BaseTrackPlayer::RESET_PITCH_AND_SPEED));
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.5));
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.5));
    buttonSyncEnabled1.set(0.0);
    buttonSyncEnabled2.set(0.0);
    m_pMixerDeck1->slotLoadTrack(track1, true);
    m_pMixerDeck1->slotTrackLoaded(track1, m_pTrack1);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    m_pMixerDeck2->slotLoadTrack(track2, false);
    m_pMixerDeck2->slotTrackLoaded(track2, m_pTrack2);
    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(128.0,
                    ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
}

TEST_F(EngineSyncTest, EnableOneDeckSliderUpdates) {
    // If we enable a deck to be master, the internal slider should immediately update.
    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");

    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(130.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));

    // Set the deck to sync enabled.
    buttonSyncEnabled1.set(1.0);
    ProcessBuffer();

    // Internal should now be master (only one sync deck).
    assertIsMaster(m_sInternalClockGroup);

    // Internal clock rate should be set.
    EXPECT_FLOAT_EQ(130.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
}

TEST_F(EngineSyncTest, SyncToNonSyncDeck) {
    // If deck 1 is playing, and deck 2 presses sync, deck 2 should sync to deck 1 even if
    // deck 1 is not a sync deck.

    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");

    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(130.0);
    ProcessBuffer();
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(100.0);
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.0));

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);

    buttonSyncEnabled2.set(1.0);
    buttonSyncEnabled2.set(0.0);
    ProcessBuffer();

    // There should be no master, and deck2 should match rate of deck1.  Sync slider should be
    // updated with the value, however.
    assertNoMaster();
    EXPECT_FLOAT_EQ(130.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    assertSyncOff(m_sGroup2);
    EXPECT_FLOAT_EQ(getRateSliderValue(1.3),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));

    // Reset the pitch of deck 2.
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.0));

    // The same should work in reverse.
    buttonSyncEnabled1.set(1.0);
    buttonSyncEnabled1.set(0.0);
    ProcessBuffer();

    // There should be no master, and deck2 should match rate of deck1.
    EXPECT_EQ(0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "sync_master")));
    EXPECT_FLOAT_EQ(100.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_EQ(NULL, m_pEngineSync->getMaster());
    EXPECT_EQ(NULL, m_pEngineSync->getMasterSyncable());
    EXPECT_EQ(SYNC_NONE, ControlObject::get(ConfigKey(m_sGroup1, "sync_mode")));
    EXPECT_EQ(0, ControlObject::get(ConfigKey(m_sGroup1, "sync_enabled")));
    EXPECT_EQ(0, ControlObject::get(ConfigKey(m_sGroup1, "sync_master")));
    EXPECT_FLOAT_EQ(getRateSliderValue(100.0 / 130.0),
                    ControlObject::get(ConfigKey(m_sGroup1, "rate")));

    // Reset again.
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));

    // If deck 1 is not playing, however, deck 2 should stay at the same rate.
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);

    // The same should work in reverse.
    buttonSyncEnabled1.set(1.0);
    buttonSyncEnabled1.set(0.0);
    ProcessBuffer();

    // There should be no master, and deck2 should match rate of deck1.
    EXPECT_EQ(0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "sync_master")));
    EXPECT_FLOAT_EQ(100.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_EQ(NULL, m_pEngineSync->getMaster());
    EXPECT_EQ(NULL, m_pEngineSync->getMasterSyncable());
    EXPECT_EQ(SYNC_NONE, ControlObject::get(ConfigKey(m_sGroup2, "sync_mode")));
    EXPECT_EQ(0, ControlObject::get(ConfigKey(m_sGroup2, "sync_enabled")));
    EXPECT_EQ(0, ControlObject::get(ConfigKey(m_sGroup2, "sync_master")));
    EXPECT_FLOAT_EQ(getRateSliderValue(1.0),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
}

TEST_F(EngineSyncTest, MomentarySyncDependsOnPlayingStates) {
    // Like it says -- if the current deck is playing, and the target deck is
    // playing, they should sync even if there's no sync mode enabled.

    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");

    // Set up decks so they can be playing, and start deck 1.
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(100.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(130.0);
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);
    ProcessBuffer();

    // Set channel 1 to be enabled momentarily.
    buttonSyncEnabled1.set(1.0);
    buttonSyncEnabled1.set(0.0);
    ProcessBuffer();

    // The master sync should still be off and the speed should match deck 2.
    assertNoMaster();
    assertSyncOff(m_sGroup1);
    assertSyncOff(m_sGroup2);
    EXPECT_FLOAT_EQ(130.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    // Also works if deck 1 is not playing.
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);
    ProcessBuffer();
    buttonSyncEnabled1.set(1.0);
    buttonSyncEnabled1.set(0.0);
    ProcessBuffer();
    assertNoMaster();
    assertSyncOff(m_sGroup1);
    assertSyncOff(m_sGroup2);
    EXPECT_FLOAT_EQ(130.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    // Also works if neither deck is playing.
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 0.0);
    ProcessBuffer();
    buttonSyncEnabled1.set(1.0);
    buttonSyncEnabled1.set(0.0);
    ProcessBuffer();
    assertNoMaster();
    assertSyncOff(m_sGroup1);
    assertSyncOff(m_sGroup2);
    EXPECT_FLOAT_EQ(130.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));


    // But it doesn't work if deck 2 isn't playing and deck 1 is. (This would
    // cause deck1 to suddenly change bpm while playing back).
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 0.0);
    ProcessBuffer();
    buttonSyncEnabled1.set(1.0);
    buttonSyncEnabled1.set(0.0);
    ProcessBuffer();
    assertNoMaster();
    assertSyncOff(m_sGroup1);
    assertSyncOff(m_sGroup2);
    EXPECT_FLOAT_EQ(100.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
}

TEST_F(EngineSyncTest, EjectTrackSyncRemains) {
    ControlProxyLt buttonMasterSyncInternal(m_sInternalClockGroup, "sync_master");
    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    ControlProxyLt buttonEject1(m_sGroup1, "eject");

    buttonMasterSyncInternal.set(1.0);

    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(120.0);
    buttonSyncEnabled1.set(1.0);

    ProcessBuffer();
    buttonEject1.set(1.0);
    // When an eject happens, the bpm gets set to zero.
    fileBpm1.set(0.0);
    ProcessBuffer();

    assertIsFollower(m_sGroup1);
}

TEST_F(EngineSyncTest, FileBpmChangesDontAffectMaster) {
    // If filebpm changes, don't treat it like a rate change.
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(100.0);
    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    buttonSyncEnabled1.set(1.0);
    ProcessBuffer();

    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(120.0);
    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");
    buttonSyncEnabled2.set(1.0);
    ProcessBuffer();

    fileBpm1.set(160.0);
    EXPECT_FLOAT_EQ(100.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup,
                                                        "bpm")));
}

TEST_F(EngineSyncTest, ExplicitMasterPostProcessed) {
    // Regression test thanks to a bug.  Make sure that an explicit master
    // channel gets post-processed.
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_MASTER);
    ControlObject::set(ConfigKey(m_sGroup1, "file_bpm"), 160.0);
    ProcessBuffer();
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ProcessBuffer();

    EXPECT_FLOAT_EQ(0.0023219956, m_pChannel1->getEngineBuffer()->getVisualPlayPos());
}

TEST_F(EngineSyncTest, ZeroBPMRateAdjustIgnored) {
    // If a track isn't loaded (0 bpm), but the deck has sync enabled,
    // don't pay attention to rate changes.
    ControlObject::set(ConfigKey(m_sGroup1, "file_bpm"), 0.0);
    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    buttonSyncEnabled1.set(1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));
    ProcessBuffer();

    ControlObject::set(ConfigKey(m_sGroup2, "file_bpm"), 120.0);
    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");
    buttonSyncEnabled2.set(1.0);
    ProcessBuffer();

    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.3));

    EXPECT_FLOAT_EQ(getRateSliderValue(1.0),
                    ControlObject::get(
                            ConfigKey(m_sGroup2, "rate")));

    // Also try with explicit master/follower setting
    buttonSyncEnabled1.set(0.0);
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_MASTER);

    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.4));
    EXPECT_FLOAT_EQ(getRateSliderValue(1.0),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));

    buttonSyncEnabled1.set(0.0);
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_FOLLOWER);

    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(0.9));
    EXPECT_FLOAT_EQ(getRateSliderValue(1.0),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
}

TEST_F(EngineSyncTest, ZeroLatencyRateChange) {
    // Confirm that a rate change in an explicit master is instantly communicated
    // to followers.
    ControlObject::set(ConfigKey(m_sGroup1, "file_bpm"), 128.0);
    ControlObject::set(ConfigKey(m_sGroup2, "file_bpm"), 128.0);
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 128, 0.0);
    m_pTrack1->setBeats(pBeats1);
    BeatsPointer pBeats2 = BeatFactory::makeBeatGrid(*m_pTrack2, 128, 0.0);
    m_pTrack2->setBeats(pBeats2);

    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "quantize"), 1.0);
    // Make Channel2 master to weed out any channel ordering issues.
    ControlObject::set(ConfigKey(m_sGroup2, "sync_mode"), SYNC_MASTER);
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_FOLLOWER);
    // Exaggerate the effect with a high rate.
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(10.0));

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);

    EXPECT_EQ(ControlObject::get(ConfigKey(m_sGroup2, "beat_distance")),
              ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));

    ProcessBuffer();
    ProcessBuffer();
    ProcessBuffer();

    // Make sure we're actually going somewhere!
    EXPECT_GT(ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")),
              0);
    // Buffers should be in sync.
    EXPECT_EQ(ControlObject::get(ConfigKey(m_sGroup2, "beat_distance")),
              ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));
}

TEST_F(EngineSyncTest, HalfDoubleBpmTest) {
    ControlObject::set(ConfigKey(m_sGroup1, "file_bpm"), 70);
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 70, 0.0);
    m_pTrack1->setBeats(pBeats1);
    ControlObject::set(ConfigKey(m_sGroup2, "file_bpm"), 140);
    BeatsPointer pBeats2 = BeatFactory::makeBeatGrid(*m_pTrack2, 140, 0.0);
    m_pTrack2->setBeats(pBeats2);

    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "quantize"), 1.0);
    // Make Channel2 master to weed out any channel ordering issues.
    ControlObject::set(ConfigKey(m_sGroup2, "sync_mode"), SYNC_MASTER);
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_FOLLOWER);

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);
    ProcessBuffer();

    EXPECT_EQ(0.5,
              m_pChannel1->getEngineBuffer()->m_pSyncControl->m_masterBpmAdjustFactor);
    EXPECT_EQ(1.0,
              m_pChannel2->getEngineBuffer()->m_pSyncControl->m_masterBpmAdjustFactor);

    // Do lots of processing to make sure we get over the 0.5 beat_distance barrier.
    for (int i=0; i<50; ++i) {
        ProcessBuffer();
        // The beat distances are NOT as simple as x2 or /2.  Use the built-in functions
        // to do the proper conversion.
        EXPECT_FLOAT_EQ(m_pChannel1->getEngineBuffer()->m_pSyncControl->getBeatDistance(),
                  m_pChannel2->getEngineBuffer()->m_pSyncControl->getBeatDistance());
    }

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 0.0);

    // Now switch master and follower and check again.
    ControlObject::set(ConfigKey(m_sGroup2, "sync_mode"), SYNC_FOLLOWER);
    ControlObject::set(ConfigKey(m_sGroup1, "sync_mode"), SYNC_MASTER);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);

    ProcessBuffer();

    EXPECT_EQ(1.0,
              m_pChannel1->getEngineBuffer()->m_pSyncControl->m_masterBpmAdjustFactor);
    EXPECT_EQ(2.0,
              m_pChannel2->getEngineBuffer()->m_pSyncControl->m_masterBpmAdjustFactor);

    // Exaggerate the effect with a high rate.
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(2.0));

    for (int i=0; i<50; ++i) {
        ProcessBuffer();
        EXPECT_FLOAT_EQ(m_pChannel1->getEngineBuffer()->m_pSyncControl->getBeatDistance(),
                  m_pChannel2->getEngineBuffer()->m_pSyncControl->getBeatDistance());
    }
}

TEST_F(EngineSyncTest, HalfDoubleThenPlay) {
    // If a deck plays that had its multiplier set, we need to reset the
    // internal clock.
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(80.0);
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(175.0);
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 80, 0.0);
    m_pTrack1->setBeats(pBeats1);
    BeatsPointer pBeats2 = BeatFactory::makeBeatGrid(*m_pTrack2, 175, 0.0);
    m_pTrack2->setBeats(pBeats2);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));

    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    buttonSyncEnabled1.set(1.0);
    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");
    buttonSyncEnabled2.set(1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "quantize"), 1.0);

    EXPECT_FLOAT_EQ(175.0,
                ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);

    EXPECT_FLOAT_EQ(175.0,
                ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    ProcessBuffer();
    ProcessBuffer();
    ProcessBuffer();

    EXPECT_FLOAT_EQ(m_pChannel1->getEngineBuffer()->m_pSyncControl->getBeatDistance(),
              m_pChannel2->getEngineBuffer()->m_pSyncControl->getBeatDistance());

    // Now enable the other deck first.
    // Unset Play so that EngineBuffer immediately responds to the sync_enabled
    // changes rather than waiting for the buffer processing.
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 0.0);
    buttonSyncEnabled1.set(0.0);
    buttonSyncEnabled2.set(0.0);
    buttonSyncEnabled2.set(1.0);
    buttonSyncEnabled1.set(1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);

    ProcessBuffer();
    ProcessBuffer();
    ProcessBuffer();

    EXPECT_FLOAT_EQ(m_pChannel1->getEngineBuffer()->m_pSyncControl->getBeatDistance(),
              m_pChannel2->getEngineBuffer()->m_pSyncControl->getBeatDistance());
}

TEST_F(EngineSyncTest, HalfDoubleInternalClockTest) {
    // If we set the file_bpm CO's directly, the correct signals aren't fired.
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(70.0);
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(140.0);
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 70, 0.0);
    m_pTrack1->setBeats(pBeats1);
    BeatsPointer pBeats2 = BeatFactory::makeBeatGrid(*m_pTrack2, 140, 0.0);
    m_pTrack2->setBeats(pBeats2);

    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "quantize"), 1.0);
    // Make Channel2 master to weed out any channel ordering issues.
    ControlObject::set(ConfigKey(m_sGroup1, "sync_enabled"), 1);
    ControlObject::set(ConfigKey(m_sGroup2, "sync_enabled"), 1);

    EXPECT_FLOAT_EQ(140.0,
                ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(getRateSliderValue(1.0),
                    ControlObject::get(ConfigKey(m_sGroup1, "rate")));
    EXPECT_FLOAT_EQ(getRateSliderValue(1.0),
                    ControlObject::get(ConfigKey(m_sGroup2, "rate")));
}

TEST_F(EngineSyncTest, SyncPhaseToPlayingNonSyncDeck) {
    // If we press play on a sync deck, we will only sync phase to a non-sync
    // deck if there are no sync decks and the non-sync deck is playing.

    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    ControlObject::set(ConfigKey(m_sGroup1, "beat_distance"), 0.2);
    fileBpm1.set(130.0);
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 130, 0.0);
    m_pTrack1->setBeats(pBeats1);
    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 1.0);

    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    ControlObject::set(ConfigKey(m_sGroup2, "beat_distance"), 0.8);
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.0));
    BeatsPointer pBeats2 = BeatFactory::makeBeatGrid(*m_pTrack2, 100, 0.0);
    m_pTrack2->setBeats(pBeats2);
    fileBpm2.set(100.0);

    // Set the sync deck playing with nothing else active.
    buttonSyncEnabled1.set(1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "play"),1.0);

    // Internal clock rate should be set but beat distances not changed.
    EXPECT_FLOAT_EQ(100.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(100.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(0.2, ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));
    EXPECT_FLOAT_EQ(0.2, ControlObject::get(ConfigKey(m_sInternalClockGroup,
                                                        "beat_distance")));

    // Now make the second deck playing and see if it works.
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);

    ProcessBuffer();

    // The exact beat distance will be one buffer past .8, but this is good
    // enough to confirm that it worked.
    EXPECT_LT(0.8, ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));
    EXPECT_LT(0.8, ControlObject::get(ConfigKey(m_sInternalClockGroup,
                                                       "beat_distance")));

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);
    buttonSyncEnabled1.set(0.0);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), getRateSliderValue(1.0));

    ControlObject::set(ConfigKey(m_sGroup2, "play"), 0.0);
    buttonSyncEnabled2.set(0.0);
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.0));

    // But if there is a third deck that is sync-enabled, we match that.
    ControlProxyLt buttonSyncEnabled3(m_sGroup3, "sync_enabled");
    ControlProxyLt fileBpm3(m_sGroup3, "file_bpm");
    ControlObject::set(ConfigKey(m_sGroup3, "beat_distance"), 0.6);
    BeatsPointer pBeats3 = BeatFactory::makeBeatGrid(*m_pTrack3, 140, 0.0);
    m_pTrack3->setBeats(pBeats3);
    fileBpm3.set(140.0);
    // This will sync to the first deck here and not the second (lp1784185)
    buttonSyncEnabled3.set(1.0);
    ProcessBuffer();
    EXPECT_FLOAT_EQ(130.0, ControlObject::getControl(ConfigKey(m_sGroup3, "bpm"))->get());
    // revert that
    ControlObject::getControl(ConfigKey(m_sGroup3, "rate"))->set(getRateSliderValue(1.0));
    ProcessBuffer();
    EXPECT_FLOAT_EQ(140.0, ControlObject::getControl(ConfigKey(m_sGroup3, "bpm"))->get());
    // now we have Deck 3 with 140 bpm and sync enabled

    buttonSyncEnabled1.set(1.0);
    ProcessBuffer();

    ControlObject::set(ConfigKey(m_sGroup3, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ProcessBuffer();

    // We expect Deck 1 is Deck 3 bpm
    EXPECT_FLOAT_EQ(140.0,
                    ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
    EXPECT_FLOAT_EQ(140.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    // The exact beat distance will be one buffer past .6, but this is good
    // enough to confirm that it worked.
    EXPECT_GT(0.7, ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));
    EXPECT_GT(0.7, ControlObject::get(ConfigKey(m_sInternalClockGroup,
                                                       "beat_distance")));
}

TEST_F(EngineSyncTest, UserTweakBeatDistance) {
    // If a deck has a user tweak, and another deck stops such that the first
    // is used to reseed the master beat distance, make sure the user offset
    // is reset.
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(128.0);
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    fileBpm2.set(128.0);
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 128, 0.0);
    m_pTrack1->setBeats(pBeats1);
    BeatsPointer pBeats2 = BeatFactory::makeBeatGrid(*m_pTrack2, 128, 0.0);
    m_pTrack2->setBeats(pBeats2);

    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "quantize"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "sync_enabled"), 1);
    ControlObject::set(ConfigKey(m_sGroup2, "sync_enabled"), 1);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);

    // Spin the wheel, causing the useroffset for group1 to get set.
    ControlObject::set(ConfigKey(m_sGroup1, "wheel"), 0.4);
    for (int i = 0; i < 10; ++i) {
        ProcessBuffer();
    }
    // Play some more buffers with the wheel at 0.
    ControlObject::set(ConfigKey(m_sGroup1, "wheel"), 0);
    for (int i = 0; i < 10; ++i) {
        ProcessBuffer();
    }

    // Stop the second deck.  This causes the master beat distance to get
    // seeded with the beat distance from deck 1.
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 0.0);

    // Play a buffer, which is enough to see if the beat distances align.
    ProcessBuffer();

    // Ah, floating point.
    double difference = fabs(ControlObject::get(ConfigKey(m_sGroup1,
                                                        "beat_distance"))
                             - ControlObject::get(ConfigKey(m_sInternalClockGroup,
                                              "beat_distance")));
    EXPECT_LT(difference, .00001);

    EXPECT_FLOAT_EQ(0.0, m_pChannel1->getEngineBuffer()->m_pBpmControl->m_dUserOffset.getValue());
}

TEST_F(EngineSyncTest, MasterBpmNeverZero) {
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(128.0);

    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    buttonSyncEnabled1.set(1.0);

    fileBpm1.set(0.0);
    EXPECT_EQ(128.0,
              ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
}

TEST_F(EngineSyncTest, ZeroBpmNaturalRate) {
    // If a track has a zero bpm and a bad beatgrid, make sure the rate
    // doesn't end up something crazy when sync is enabled..
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(0.0);
    // Maybe the beatgrid ended up at zero also.
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 0.0, 0.0);
    m_pTrack1->setBeats(pBeats1);

    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    buttonSyncEnabled1.set(1.0);

    ProcessBuffer();

    // 0 bpm is what we want, the sync code will play the track back at rate
    // 1.0.
    EXPECT_EQ(0.0,
              ControlObject::get(ConfigKey(m_sGroup1, "local_bpm")));
}

TEST_F(EngineSyncTest, QuantizeImpliesSyncPhase) {
    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    ControlProxyLt buttonBeatsync1(m_sGroup1, "beatsync");
    ControlProxyLt buttonBeatsyncPhase1(m_sGroup1, "beatsync_phase");

    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    ControlObject::set(ConfigKey(m_sGroup1, "beat_distance"), 0.2);
    fileBpm1.set(130.0);
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 130, 0.0);
    m_pTrack1->setBeats(pBeats1);

    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    ControlObject::set(ConfigKey(m_sGroup2, "beat_distance"), 0.8);
    ControlObject::set(ConfigKey(m_sGroup2, "rate"), getRateSliderValue(1.0));
    BeatsPointer pBeats2 = BeatFactory::makeBeatGrid(*m_pTrack2, 100, 0.0);
    m_pTrack2->setBeats(pBeats2);
    fileBpm2.set(100.0);

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);
    ProcessBuffer();

    // first test without quantisation
    buttonSyncEnabled1.set(1.0);
    ProcessBuffer();
    ASSERT_DOUBLE_EQ(0.025155996658969195, ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));
    buttonSyncEnabled1.set(0.0);

    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 1.0);
    ProcessBuffer();

    buttonSyncEnabled1.set(1.0);
    ProcessBuffer();

    ASSERT_DOUBLE_EQ(0.077604743399185772, ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));
    buttonSyncEnabled1.set(0.0);
    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 0.0);
    ProcessBuffer();

    buttonBeatsyncPhase1.set(1.0);
    ProcessBuffer();

    // 0.11632139450713055 in case "beatsync_phase" fails
    ASSERT_DOUBLE_EQ(0.11610813658949772, ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));


    ControlObject::set(ConfigKey(m_sGroup1, "beat_distance"), 0.2);
    ControlObject::set(ConfigKey(m_sGroup1, "rate"), 1.0);
    ProcessBuffer();
    buttonBeatsync1.set(1.0);
    ProcessBuffer();

    // these values were determined experimentally
    // 0.15480806100370784 in case quantize is enabled
    ASSERT_DOUBLE_EQ(0.16263690384739582, ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));

    ProcessBuffer();

    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 1.0);
    buttonBeatsync1.set(1.0);
    ProcessBuffer();

    // these values were determined experimentally
    // 0.19933910991038406 in case quantize is disabled
    ASSERT_DOUBLE_EQ(0.19350798541791794, ControlObject::get(ConfigKey(m_sGroup1, "beat_distance")));

}

TEST_F(EngineSyncTest, SeekStayInPhase) {
    ControlObject::set(ConfigKey(m_sGroup1, "quantize"), 1.0);

    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    ControlObject::set(ConfigKey(m_sGroup1, "beat_distance"), 0.2);
    fileBpm1.set(130.0);
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 130, 0.0);
    m_pTrack1->setBeats(pBeats1);

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    ProcessBuffer();

    ControlObject::set(ConfigKey(m_sGroup1, "playposition"), 0.2);
    ProcessBuffer();

    ASSERT_DOUBLE_EQ(0.20464410501585786, ControlObject::get(ConfigKey(m_sGroup1, "playposition")));
}

TEST_F(EngineSyncTest, SyncWithoutBeatgrid) {
    // this tests bug lp1783020, notresetting rate when other deck has no beatgrid
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    fileBpm1.set(128.0);
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 128, 0.0);
    m_pTrack1->setBeats(pBeats1);
    m_pTrack2->setBeats(BeatsPointer());

    ControlObject::set(ConfigKey(m_sGroup1, "rate"), 0.5);

    // Play a buffer, which is enough to see if the beat distances align.
    ProcessBuffer();

    ControlObject::set(ConfigKey(m_sGroup1, "sync_enabled"), 1);

    ProcessBuffer();

    ASSERT_DOUBLE_EQ(0.5,
              ControlObject::get(ConfigKey(m_sGroup1, "rate")));
}

TEST_F(EngineSyncTest, QuantizeHotCueActivate) {
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    ControlObject::set(ConfigKey(m_sGroup1, "beat_distance"), 0.2);
    fileBpm1.set(130.0);
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 130, 0.0);
    m_pTrack1->setBeats(pBeats1);

    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");
    ControlProxyLt hotCueActivate(m_sGroup2, "hotcue_1_activate");
    ControlObject::set(ConfigKey(m_sGroup2, "beat_distance"), 0.8);
    BeatsPointer pBeats2 = BeatFactory::makeBeatGrid(*m_pTrack2, 100, 0.0);
    m_pTrack2->setBeats(pBeats2);
    fileBpm2.set(100.0);

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);

    // store a hot cue
    hotCueActivate.set(1.0);
    ProcessBuffer();
    hotCueActivate.set(0.0);
    ProcessBuffer();

    ControlObject::set(ConfigKey(m_sGroup2, "quantize"), 0.0);
    // preview a hot cue without quantize
    hotCueActivate.set(1.0);
    ProcessBuffer();

    // the value was determined experimentally
    ASSERT_DOUBLE_EQ(0.019349962207105064, ControlObject::get(ConfigKey(m_sGroup2, "beat_distance")));

    hotCueActivate.set(0.0);
    ProcessBuffer();

    ControlObject::set(ConfigKey(m_sGroup2, "quantize"), 1.0);
    // preview a hot cue with quantize
    hotCueActivate.set(1.0);
    ProcessBuffer();

    // the value was determined experimentally 
    ASSERT_DOUBLE_EQ(0.11997394884298185, ControlObject::get(ConfigKey(m_sGroup2, "beat_distance")));

    hotCueActivate.set(0.0);
    ProcessBuffer();
}

TEST_F(EngineSyncTest, ChangeBeatGrid) {
    // https://bugs.launchpad.net/mixxx/+bug/1808698

    ControlProxyLt buttonSyncEnabled1(m_sGroup1, "sync_enabled");
    ControlProxyLt buttonSyncEnabled2(m_sGroup2, "sync_enabled");
    ControlProxyLt fileBpm1(m_sGroup1, "file_bpm");
    ControlProxyLt fileBpm2(m_sGroup2, "file_bpm");

    // set beatgrid
    BeatsPointer pBeats1 = BeatFactory::makeBeatGrid(*m_pTrack1, 130, 0.0);
    m_pTrack1->setBeats(pBeats1);
    fileBpm1.set(130.0);
    buttonSyncEnabled1.set(1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);

    ProcessBuffer();

    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    // sync 0 bpm track to the first one
    buttonSyncEnabled2.set(1.0);
    ControlObject::set(ConfigKey(m_sGroup2, "play"), 1.0);

    ProcessBuffer();

    // expect no change in Deck 1
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));

    ControlObject::set(ConfigKey(m_sGroup1, "play"), 0.0);

    ProcessBuffer();

    // Load a new beatgrid, this happens when the analyser is finisched
    BeatsPointer pBeats2 = BeatFactory::makeBeatGrid(*m_pTrack2, 140, 0.0);
    m_pTrack2->setBeats(pBeats2);
    fileBpm2.set(140.0);

    ProcessBuffer();

    // we expect that the new beatgrid is allingend to the other playing track
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));


    ProcessBuffer();

    // Load a new beatgrid again, this happens when the user adjusts the beatgrid
    BeatsPointer pBeats2n = BeatFactory::makeBeatGrid(*m_pTrack2, 75, 0.0);
    m_pTrack2->setBeats(pBeats2n);
    fileBpm2.set(75.0);

    ProcessBuffer();

    // we expect that the new beatgrid is allingend to the other playing track
    // Not the case before fixing lp1808698
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sGroup1, "bpm")));
    // Expect to sync on half beats
    EXPECT_FLOAT_EQ(65.0, ControlObject::get(ConfigKey(m_sGroup2, "bpm")));
    EXPECT_FLOAT_EQ(130.0, ControlObject::get(ConfigKey(m_sInternalClockGroup, "bpm")));
}


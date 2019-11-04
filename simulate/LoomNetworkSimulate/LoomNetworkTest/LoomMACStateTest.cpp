#include "pch.h"
#include "gmock/gmock.h"
#include "../../../src/LoomMACState.h"
#include "../../../src/LoomRadio.h"

using namespace LoomNet;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::StrictMock;
using ::testing::NaggyMock;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Eq;

class MockSlotter {
public:
	MockSlotter(const NetworkConfig& config) {}

	MOCK_METHOD(Slotter::State, get_state, (), (const));
	MOCK_METHOD(Slotter::State, next_state, ());
	MOCK_METHOD(uint8_t, get_slot_wait, (), (const));
	MOCK_METHOD(uint16_t, get_slots_per_refresh, (), (const));
	MOCK_METHOD(void, reset, ());

	void set_next_state(const Slotter::State state, const uint8_t wait) {
		EXPECT_CALL(*this, next_state).Times(1).WillOnce(Return(state));
		ON_CALL(*this, get_state).WillByDefault(Return(state));
		ON_CALL(*this, get_slot_wait).WillByDefault(Return(wait));
	}
};

// setup some utility functions to test a sequence of state transitions
// in the MAC layer
class MACStateFixture : public ::testing::TestWithParam<const NetworkConfig&> {
public:
	static const NetworkConfig config_end;
	static const NetworkConfig config_router1;
	static const NetworkConfig config_router2;
	static const NetworkConfig config_coord;

protected:
	void SetUp() override {}
	void TearDown() override {}

	using MAC = MACState<StrictMock<MockSlotter>>;
	using State = MAC::State;

	void start(MAC& mac, const NetworkConfig& config) {
		// check the initial state of the MACState
		EXPECT_EQ(mac.get_state(), State::MAC_CLOSED);
		EXPECT_NE(mac.get_device_type(), DeviceType::ERROR);
		// make sure the slotter is reset on start
		EXPECT_CALL(mac.get_slotter(), reset).Times(1).RetiresOnSaturation();
		// start er up
		mac.begin();
		// check that the start state is correct
		if (mac.get_device_type() == DeviceType::COORDINATOR)
			EXPECT_EQ(mac.get_state(), State::MAC_SEND_REFRESH);
		else
			EXPECT_EQ(mac.get_state(), State::MAC_WAIT_REFRESH);
		// trigger a refresh
		m_data_sync = config.slot_length * 5;
		m_refresh_sync = config.slot_length * 100;
		m_now = TimeInterval(TimeInterval::SECOND, 500);
		mac.refresh_event(m_now, m_data_sync, m_refresh_sync);
		// check the state
		EXPECT_EQ(mac.get_state(), State::MAC_SLEEP_RDY);
		// verify the sleep time is correct
		// if we're an end device we only ever sent
		if (get_type(config.self_addr) == DeviceType::END_DEVICE)
			EXPECT_EQ(mac.wake_next_time(), m_data_sync);
		else
			EXPECT_EQ(mac.wake_next_time(), m_data_sync - config.min_drift);
	}

	TimeInterval m_now = TimeInterval(TIME_NONE);
	TimeInterval m_data_sync = TimeInterval(TIME_NONE);
	TimeInterval m_refresh_sync = TimeInterval(TIME_NONE);
};

const NetworkConfig MACStateFixture::config_coord = {
	SLOT_NONE, 24, 2, 1, 1, 0, 13, 11,
	ADDR_COORD, 3, 1,
	TimeInterval(TimeInterval::SECOND, 3),
	TimeInterval(TimeInterval::SECOND, 10),
	TimeInterval(TimeInterval::SECOND, 10)
};

const NetworkConfig MACStateFixture::config_router1 = {
	13, 24, 2, 1, 1, 7, 4, 7,
	0x1000, 2, 3,
	TimeInterval(TimeInterval::SECOND, 3),
	TimeInterval(TimeInterval::SECOND, 10),
	TimeInterval(TimeInterval::SECOND, 10)
};

const NetworkConfig MACStateFixture::config_router2 = {
	5, 24, 2, 1, 1, 3, 1, 2,
	0x1200, 0, 2,
	TimeInterval(TimeInterval::SECOND, 3),
	TimeInterval(TimeInterval::SECOND, 10),
	TimeInterval(TimeInterval::SECOND, 10)
};

const NetworkConfig MACStateFixture::config_end = {
	1, 24, 2, 1, 1, 1, SLOT_NONE, 0,
	0x1201, 0, 0,
	TimeInterval(TimeInterval::SECOND, 3),
	TimeInterval(TimeInterval::SECOND, 10),
	TimeInterval(TimeInterval::SECOND, 10)
};

TEST_P(MACStateFixture, RefreshCoord) {
	const NetworkConfig& config = GetParam();
	MAC test_mac(config);
	// start the MAC
	start(test_mac, config);
}

INSTANTIATE_TEST_SUITE_P(RefreshState, MACStateFixture,
	::testing::Values(	MACStateFixture::config_coord, 
						MACStateFixture::config_end, 
						MACStateFixture::config_router2, 
						MACStateFixture::config_router1));
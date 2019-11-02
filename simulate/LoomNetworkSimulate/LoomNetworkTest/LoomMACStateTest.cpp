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
class MACStateFixture : public ::testing::Test {
protected:
	void SetUp() override {}
	void TearDown() override {}

	using MAC = MACState<StrictMock<MockSlotter>>;
	using State = MAC::State;

	void start(MAC& mac, StrictMock<MockSlotter>& slot, const NetworkConfig& config) {
		// check the initial state of the MACState
		EXPECT_EQ(mac.get_state(), State::MAC_CLOSED);
		EXPECT_NE(mac.get_device_type(), DeviceType::ERROR);
		// make sure the slotter is reset on start
		EXPECT_CALL(slot, reset).Times(1).RetiresOnSaturation();
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
	}

	TimeInterval m_now = TimeInterval(TIME_NONE);
	StrictMock<MockSlotter> m_slot;
	TimeInterval m_data_sync = TimeInterval(TIME_NONE);
	TimeInterval m_refresh_sync = TimeInterval(TIME_NONE);
};

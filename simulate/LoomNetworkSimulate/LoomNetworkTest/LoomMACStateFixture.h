#pragma once
#include "pch.h"
#include <utility>
#include "gmock/gmock.h"
#include "../../../src/LoomMACState.h"

using namespace LoomNet;
using ::testing::Sequence;
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

	void start(std::pair<Sequence, Sequence>& seq) const {
		// and make sure it is reset at least once
		EXPECT_CALL(*this, reset)
			.Times(1)
			.InSequence(seq.first, seq.second)
			.RetiresOnSaturation();
		// set the first slotter state to wait refresh
		EXPECT_CALL(*this, get_state)
			.Times(AnyNumber())
			.InSequence(seq.first, seq.second)
			.WillRepeatedly(Return(Slotter::State::SLOT_WAIT_REFRESH));
	}

	void set_next_state(const Slotter::State next_state, const uint8_t wait, std::pair<Sequence, Sequence>& seq) const {
		// next_state call
		EXPECT_CALL(*this, next_state)
			.Times(1)
			.InSequence(seq.first, seq.second)
			.WillOnce(Return(next_state))
			.RetiresOnSaturation();
		// get_state and get_slot_wait calls after
		EXPECT_CALL(*this, get_state)
			.Times(AnyNumber())
			.InSequence(seq.first)
			.WillRepeatedly(Return(next_state));
		EXPECT_CALL(*this, get_slot_wait)
			.Times(AnyNumber())
			.InSequence(seq.second)
			.WillRepeatedly(Return(wait));
	}
};

// setup some utility functions to test a sequence of state transitions
// in the MAC layer
class MACStateFixture : public ::testing::TestWithParam<const NetworkConfig*> {
public:
	static const NetworkConfig config_end;
	static const NetworkConfig config_router1;
	static const NetworkConfig config_router2;
	static const NetworkConfig config_coord;

protected:
	void SetUp() override {}
	void TearDown() override {}

	class ErrorType {
	public:
		void operator()(uint8_t error, uint8_t expectect_state) const {
			ADD_FAILURE() << "Got error " << static_cast<int>(error) << " and state " << static_cast<int>(expectect_state) << " from the MAC";
		}
	};

	using MAC = MACState<StrictMock<MockSlotter>, ErrorType>;
	using State = MAC::State;

	void start(MAC& mac, const NetworkConfig& config) {
		// check the initial state of the MACState
		EXPECT_EQ(mac.get_state(), State::MAC_CLOSED);
		EXPECT_NE(mac.get_parent_addr(), ADDR_ERROR);
		EXPECT_EQ(mac.get_recv_timeout(), config.min_drift);
		// start er up
		mac.begin();
		// start the MAC
		// check that the start state is correct
		if (mac.get_parent_addr() == ADDR_NONE)
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
#include "pch.h"
#include "gmock/gmock.h"
#include "../../../src/LoomMAC.h"
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

class FakeRadio : public Radio {
public:
	FakeRadio()
		: m_state(State::DISABLED) {}

	State get_state() const override { return m_state; }
	TimeInterval get_time() const override { return TIME_NONE; }
	void enable() override { m_state = State::SLEEP; }
	void disable() override { m_state = State::DISABLED; }
	void sleep() override { m_state = State::SLEEP; }
	void wake() override { m_state = State::IDLE; }
	Packet recv(TimeInterval& recv_stamp) override { return Packet(PacketCtrl::NONE, ADDR_NONE); }
	void send(const Packet& send) override {};

private:
	State m_state;
};

// a mock radio class
class MockRadio : public Radio {
public:
	MOCK_METHOD(TimeInterval, get_time, (), (const));
	MOCK_METHOD(State, get_state, (), (const));
	MOCK_METHOD(void, enable, (), (override));
	MOCK_METHOD(void, disable, (), (override));
	MOCK_METHOD(void, sleep, (), (override));
	MOCK_METHOD(void, wake, (), (override));
	MOCK_METHOD(Packet, recv, (TimeInterval& recv_stamp), (override));
	MOCK_METHOD(void, send, (const Packet& send), (override));

	void DelagateStateToFake() {
		ON_CALL(*this, get_time).WillByDefault([this] { return m_radio.get_time(); });
		ON_CALL(*this, get_state).WillByDefault([this] { return m_radio.get_state(); });
		ON_CALL(*this, enable).WillByDefault([this] { m_radio.enable(); });
		ON_CALL(*this, disable).WillByDefault([this] { m_radio.disable(); });
		ON_CALL(*this, sleep).WillByDefault([this] { m_radio.sleep(); });
		ON_CALL(*this, wake).WillByDefault([this] { m_radio.wake(); });
		ON_CALL(*this, send).WillByDefault([this](const Packet& send) { m_radio.send(send); });
		ON_CALL(*this, recv).WillByDefault([this](TimeInterval& recv) { return m_radio.recv(recv); });
		// get_state should be called any number of times, as should get_time
		EXPECT_CALL(*this, get_state).Times(AnyNumber());
		EXPECT_CALL(*this, get_time).Times(AnyNumber());
	}

	State get_underlying_state() const { return m_radio.get_state(); }

private:
	FakeRadio m_radio;
};

// setup some utility functions to test a sequence of state transitions
// in the MAC layer
class MACFixture : public ::testing::Test {
protected:
	void SetUp() override {}
	void TearDown() override {}

	TimeInterval run_until_state_change(MAC& mac, 
			TimeInterval& time, 
			const TimeInterval& timeout) const {

		const TimeInterval end_time = time + timeout;
		const TimeInterval start_time = time;
		const MAC::State last_state = mac.get_status();
		EXPECT_TRUE(last_state == MAC::State::MAC_DATA_WAIT || last_state == MAC::State::MAC_REFRESH_WAIT) << "Invalid state for input parameters!";
		while (mac.get_status() == last_state && time < end_time) {
			if (last_state == MAC::State::MAC_REFRESH_WAIT)
				mac.check_for_refresh();
			else
				mac.check_for_data();
			// increment the time by one slot
			time = time + m_drift.min_drift;
		}
		EXPECT_LT(time, end_time);
		return time - start_time;
	}

	const Drift m_drift = Drift(TimeInterval(TimeInterval::SECOND, 1), 
								TimeInterval(TimeInterval::SECOND, 5), 
								TimeInterval(TimeInterval::SECOND, 10));
};

TEST_F(MACFixture, RefreshTimeout) {
	StrictMock<MockRadio> radio;
	
	// delegate to a fake state machine
	radio.DelagateStateToFake();

	// initialize the MAC (should make no calls to the radio)
	const Slotter slots(5, 10, 3, 2, 2, 1, SLOT_NONE, 0);
	MAC test_mac(0x1101, DeviceType::END_DEVICE, slots, m_drift, radio);
	EXPECT_EQ(test_mac.get_status(), MAC::State::MAC_REFRESH_WAIT);
	EXPECT_EQ(test_mac.get_last_error(), MAC::Error::MAC_OK);

	{
		// check that the MAC times out at exactly when it's supposed to
		TimeInterval cur_time(TimeInterval::SECOND, 0);
		EXPECT_CALL(radio, get_time)
			.WillRepeatedly(ReturnPointee(&cur_time));
		InSequence seq;
		EXPECT_CALL(radio, enable).Times(1);
		EXPECT_CALL(radio, wake).Times(1);
		EXPECT_CALL(radio, recv).Times(AnyNumber());
		EXPECT_CALL(radio, send).Times(0);
		EXPECT_CALL(radio, sleep).Times(1);
		EXPECT_CALL(radio, disable).Times(1);
		// run the MAC until it times out
		const TimeInterval took = run_until_state_change(test_mac, cur_time, m_drift.slot_length * slots.get_total_slots() * 3);
		// the MAC is supposed to wait exactly one refresh cycle
		const TimeInterval refresh_wait = m_drift.slot_length * (slots.get_slots_per_refresh() + REFRESH_CYCLE_SLOTS) + m_drift.max_drift;
		EXPECT_EQ(test_mac.get_status(), MAC::State::MAC_CLOSED);
		EXPECT_EQ(took, refresh_wait);
		EXPECT_EQ(test_mac.get_last_error(), MAC::Error::REFRESH_TIMEOUT);
	}
}
// TODO: this doesn't really make sense I think, but I'm not sure how to fix it
TEST_F(MACFixture, CoordinatorOperation) {
	StrictMock<MockRadio> radio;

	// delegate to a fake state machine
	radio.DelagateStateToFake();

	// initialize the MAC as a Coordinator (should make no calls to the radio)
	const Slotter slots(SLOT_NONE, 10, 3, 2, 2, 1, 5, 3);
	MAC test_mac(ADDR_COORD, DeviceType::COORDINATOR, slots, m_drift, radio);
	EXPECT_EQ(test_mac.get_status(), MAC::State::MAC_REFRESH_WAIT);
	EXPECT_EQ(test_mac.get_last_error(), MAC::Error::MAC_OK);

	// check that the MAC affects the radio exactly how it's supposed to
	// can get the time or the state, but cannot recieve
	TimeInterval cur_time(TimeInterval::SECOND, 0);
	EXPECT_CALL(radio, get_time)
		.WillRepeatedly(ReturnPointee(&cur_time));
	EXPECT_CALL(radio, get_state).Times(AnyNumber());
	EXPECT_CALL(radio, recv).Times(0);
	// in order: enable -> wake -> send -> sleep
	EXPECT_CALL(radio, enable).Times(1);
	for (int loop = 0; loop < 3; loop++) {
		InSequence seq;
		EXPECT_CALL(radio, wake).Times(1);
		const uint8_t next_data_slots = cur_time.get_time() + REFRESH_CYCLE_SLOTS - m_drift.max_drift.get_time();
		const uint8_t next_refresh_slots = cur_time.get_time() + slots.get_slots_per_refresh() - m_drift.max_drift.get_time();
		Packet expected_packet = RefreshPacket::Factory(ADDR_COORD,
			TimeInterval(TimeInterval::SECOND, next_data_slots),
			TimeInterval(TimeInterval::SECOND, next_refresh_slots),
			0);
		expected_packet.set_framecheck();
		EXPECT_CALL(radio, send(expected_packet)).Times(1);
		EXPECT_CALL(radio, sleep).Times(1);
		// the MAC should send a refresh packet here
		run_until_state_change(test_mac, cur_time, m_drift.slot_length * REFRESH_CYCLE_SLOTS);
		// for every data cycle
		for (uint8_t cur_cycle = 0; cur_cycle < slots.get_cycles_per_refresh(); cur_cycle++) {
			// check the current wake time to make sure it's correct
			EXPECT_EQ(test_mac.get_status(), MAC::State::MAC_SLEEP_RDY) << "Error: " << static_cast<int>(test_mac.get_last_error());
			// and will go to sleep until slot #5, where it's supposed to recieve
			EXPECT_EQ(test_mac.sleep_next_wake_time(), m_drift.slot_length * next_data_slots);
			// for every device we're supposed to recieve
			for (uint8_t recv_device = 0; recv_device < slots.get_recv_count(); recv_device++) {
				// we expect the radio to wake->recv->sleep
				// calls are still required to be in sequence
				EXPECT_CALL(radio, wake).Times(1);
				EXPECT_CALL(radio, recv).Times(AtLeast(1));
				EXPECT_CALL(radio, sleep).Times(1);
				// wake the MAC
				test_mac.sleep_wake_ack();
				EXPECT_EQ(test_mac.get_status(), MAC::State::MAC_DATA_WAIT);
				// check for data, advancing the time
				run_until_state_change(test_mac, cur_time, m_drift.slot_length);
			}
		}
			
		// TODO: something here

	}
		
}
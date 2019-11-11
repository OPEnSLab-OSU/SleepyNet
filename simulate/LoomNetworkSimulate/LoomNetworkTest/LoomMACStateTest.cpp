#include "LoomMACStateFixture.h"

TEST_P(MACStateFixture, StartToRefresh) {
	const NetworkConfig& config = *GetParam();
	MAC test_mac(config);
	const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
	std::pair<Sequence, Sequence> seq;
	// set the slotters start expectations
	slot.start(seq);
	// and set the next state to what it would actually be
	if (get_type(config.self_addr) == DeviceType::END_DEVICE)
		slot.set_next_state(Slotter::State::SLOT_SEND_W_SYNC, 5, seq);
	else
		slot.set_next_state(Slotter::State::SLOT_RECV_W_SYNC, 5, seq);
	// with those expectations, start the MAC!
	start(test_mac, config);
	// verify the sleep time is correct
	// if we're an end device we only ever sent
	if (get_type(config.self_addr) == DeviceType::END_DEVICE)
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync);
	else
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync - config.min_drift);
}

TEST_P(MACStateFixture, RefreshToRefresh) {
	const NetworkConfig& config = *GetParam();
	MAC test_mac(config);
	const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
	std::pair<Sequence, Sequence> seq;
	// set the slotters start expectations
	slot.start(seq);
	// set the next state to refresh
	slot.set_next_state(Slotter::State::SLOT_WAIT_REFRESH, 0, seq);
	// and set start expectations again, since the slotter will be reset
	slot.start(seq);
	// with those expectations, start the MAC!
	start(test_mac, config);
	// verify the sleep time is correct
	// if we're an end device we only ever sent
	if (get_type(config.self_addr) == DeviceType::COORDINATOR)
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_refresh_sync);
	else
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_refresh_sync - config.max_drift);
	// wake the MAC
	test_mac.wake_event();
	// check the state
	if (get_type(config.self_addr) == DeviceType::COORDINATOR)
		EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_REFRESH);
	else
		EXPECT_EQ(test_mac.get_state(), State::MAC_WAIT_REFRESH);

}

TEST_P(MACStateFixture, RecvSuccessFull) {
	const NetworkConfig& config = *GetParam();
	MAC test_mac(config);
	const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
	std::pair<Sequence, Sequence> seq;
	// set the slotters start expectations
	slot.start(seq);
	// and set the next state to what it would actually be
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	// with those expectations, start the MAC!
	start(test_mac, config);
	// verify the sleep time
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5 - config.min_drift);
	// wake the MAC
	test_mac.wake_event();
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
	// recieve the packet!
	test_mac.recv_event(PacketCtrl::DATA_TRANS, 0x0001);
	// test the state, current packet type, and sending address
	EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK_W_DATA);
	EXPECT_EQ(test_mac.get_cur_send_addr(), 0x0001);
	// send a data with ack packet
	test_mac.send_event(PacketCtrl::DATA_ACK_W_DATA);
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK);
	// recive the packet
	test_mac.recv_event(PacketCtrl::DATA_ACK, 0x0001);
	// and that's a full transaction!
	EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
	EXPECT_EQ(test_mac.get_last_sent_status(), MAC::SendStatus::MAC_SEND_SUCCESSFUL);
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10 - config.min_drift);
}

TEST_P(MACStateFixture, RecvSuccessPartial) {
	const NetworkConfig& config = *GetParam();
	MAC test_mac(config);
	const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
	std::pair<Sequence, Sequence> seq;
	// set the slotters start expectations
	slot.start(seq);
	// and set the next state to what it would actually be
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	// with those expectations, start the MAC!
	start(test_mac, config);
	// verify the sleep time
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5 - config.min_drift);
	// wake the MAC
	test_mac.wake_event();
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
	// recieve the packet!
	test_mac.recv_event(PacketCtrl::DATA_TRANS, 0x0001);
	// test the state, current packet type, and sending address
	EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK_W_DATA);
	EXPECT_EQ(test_mac.get_cur_send_addr(), 0x0001);
	// send a data with ack packet
	test_mac.send_event(PacketCtrl::DATA_ACK);
	EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10 - config.min_drift);
}

TEST_P(MACStateFixture, RecvFailFull) {
	const NetworkConfig& config = *GetParam();
	MAC test_mac(config);
	const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
	std::pair<Sequence, Sequence> seq;
	// set the slotters start expectations
	slot.start(seq);
	// and set the next state to what it would actually be
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	// with those expectations, start the MAC!
	start(test_mac, config);
	// verify the sleep time
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5 - config.min_drift);
	// wake the MAC
	test_mac.wake_event();
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
	// recieve the packet!
	test_mac.recv_event(PacketCtrl::DATA_TRANS, 0x0001);
	// test the state, current packet type, and sending address
	EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK_W_DATA);
	EXPECT_EQ(test_mac.get_cur_send_addr(), 0x0001);
	// send a data with ack packet
	test_mac.send_event(PacketCtrl::DATA_ACK_W_DATA);
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK);
	// fail to recive the packet
	test_mac.recv_fail();
	// and that's a full transaction!
	EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
	EXPECT_EQ(test_mac.get_last_sent_status(), MAC::SendStatus::MAC_SEND_NO_ACK);
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10 - config.min_drift);
}

TEST_P(MACStateFixture, RecvFailPartial) {
	const NetworkConfig& config = *GetParam();
	MAC test_mac(config);
	const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
	std::pair<Sequence, Sequence> seq;
	// set the slotters start expectations
	slot.start(seq);
	// and set the next state to what it would actually be
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	// with those expectations, start the MAC!
	start(test_mac, config);
	// verify the sleep time
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5 - config.min_drift);
	// wake the MAC
	test_mac.wake_event();
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
	// recieve the packet!
	test_mac.recv_fail();
	// test the state, current packet type, and sending address
	EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10 - config.min_drift);
}

TEST_P(MACStateFixture, RecvWrongAddressFull) {
	const NetworkConfig& config = *GetParam();
	MAC test_mac(config);
	const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
	std::pair<Sequence, Sequence> seq;
	// set the slotters start expectations
	slot.start(seq);
	// and set the next state to what it would actually be
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	// with those expectations, start the MAC!
	start(test_mac, config);
	// verify the sleep time
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5 - config.min_drift);
	// wake the MAC
	test_mac.wake_event();
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
	// recieve the packet!
	test_mac.recv_event(PacketCtrl::DATA_TRANS, 0x0001);
	// test the state, current packet type, and sending address
	EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK_W_DATA);
	EXPECT_EQ(test_mac.get_cur_send_addr(), 0x0001);
	// send a data with ack packet
	test_mac.send_event(PacketCtrl::DATA_ACK_W_DATA);
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK);
	// fail to recive the packet
	test_mac.recv_event(PacketCtrl::DATA_ACK, 0x0002);
	// and that's a full transaction!
	EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
	EXPECT_EQ(test_mac.get_last_sent_status(), MAC::SendStatus::MAC_SEND_WRONG_RESPONSE);
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10 - config.min_drift);
}

TEST_P(MACStateFixture, RecvWrongTypeFull) {
	const NetworkConfig& config = *GetParam();
	MAC test_mac(config);
	const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
	std::pair<Sequence, Sequence> seq;
	// set the slotters start expectations
	slot.start(seq);
	// and set the next state to what it would actually be
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	// with those expectations, start the MAC!
	start(test_mac, config);
	// verify the sleep time
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5 - config.min_drift);
	// wake the MAC
	test_mac.wake_event();
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
	// recieve the packet!
	test_mac.recv_event(PacketCtrl::DATA_TRANS, 0x0001);
	// test the state, current packet type, and sending address
	EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK_W_DATA);
	EXPECT_EQ(test_mac.get_cur_send_addr(), 0x0001);
	// send a data with ack packet
	test_mac.send_event(PacketCtrl::DATA_ACK_W_DATA);
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK);
	// fail to recive the packet
	test_mac.recv_event(PacketCtrl::DATA_TRANS, 0x0001);
	// and that's a full transaction!
	EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
	EXPECT_EQ(test_mac.get_last_sent_status(), MAC::SendStatus::MAC_SEND_WRONG_RESPONSE);
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10 - config.min_drift);
}

TEST_P(MACStateFixture, RecvWrongTypePartial) {
	const NetworkConfig& config = *GetParam();
	MAC test_mac(config);
	const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
	std::pair<Sequence, Sequence> seq;
	// set the slotters start expectations
	slot.start(seq);
	// and set the next state to what it would actually be
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	slot.set_next_state(Slotter::State::SLOT_RECV, 5, seq);
	// with those expectations, start the MAC!
	start(test_mac, config);
	// verify the sleep time
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5 - config.min_drift);
	// wake the MAC
	test_mac.wake_event();
	// check the state and current packet type
	EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
	EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
	// recieve the packet!
	test_mac.recv_event(PacketCtrl::DATA_ACK, 0x0001);
	// test the state, current packet type, and sending address
	EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
	EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10 - config.min_drift);
}

TEST_P(MACStateFixture, SendSuccessFull) {
	const NetworkConfig& config = *GetParam();
	// we can't run this test for a coordinator
	if (config.self_addr != ADDR_COORD) {
		MAC test_mac(config);
		const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
		std::pair<Sequence, Sequence> seq;
		// set the slotters start expectations
		slot.start(seq);
		// and set the next state to what it would actually be
		slot.set_next_state(Slotter::State::SLOT_SEND, 5, seq);
		slot.set_next_state(Slotter::State::SLOT_SEND, 5, seq);
		// with those expectations, start the MAC!
		start(test_mac, config);
		// verify the sleep time
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5);
		// wake the MAC
		test_mac.wake_event();
		// check the state and current packet type
		EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
		EXPECT_EQ(test_mac.get_cur_send_addr(), test_mac.get_parent_addr());
		// send the packet!
		test_mac.send_event(PacketCtrl::DATA_TRANS);
		// test the state, current packet type, and sending address
		EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK_W_DATA);
		// recieve the packet!
		test_mac.recv_event(PacketCtrl::DATA_ACK_W_DATA, test_mac.get_parent_addr());
		// check the state and current packet type
		EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK);
		EXPECT_EQ(test_mac.get_cur_send_addr(), test_mac.get_parent_addr());
		// recive the packet
		test_mac.send_event(PacketCtrl::DATA_ACK);
		// and that's a full transaction!
		EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
		EXPECT_EQ(test_mac.get_last_sent_status(), MAC::SendStatus::MAC_SEND_SUCCESSFUL);
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10);
	}
}

TEST_P(MACStateFixture, SendSuccessPartial) {
	const NetworkConfig& config = *GetParam();
	// we can't run this test for a coordinator
	if (config.self_addr != ADDR_COORD) {
		MAC test_mac(config);
		const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
		std::pair<Sequence, Sequence> seq;
		// set the slotters start expectations
		slot.start(seq);
		// and set the next state to what it would actually be
		slot.set_next_state(Slotter::State::SLOT_SEND, 5, seq);
		slot.set_next_state(Slotter::State::SLOT_SEND, 5, seq);
		// with those expectations, start the MAC!
		start(test_mac, config);
		// verify the sleep time
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5);
		// wake the MAC
		test_mac.wake_event();
		// check the state and current packet type
		EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
		EXPECT_EQ(test_mac.get_cur_send_addr(), test_mac.get_parent_addr());
		// send the packet!
		test_mac.send_event(PacketCtrl::DATA_TRANS);
		// test the state, current packet type, and sending address
		EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK_W_DATA);
		// recieve the packet!
		test_mac.recv_event(PacketCtrl::DATA_ACK, test_mac.get_parent_addr());
		// and that's a full transaction!
		EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
		EXPECT_EQ(test_mac.get_last_sent_status(), MAC::SendStatus::MAC_SEND_SUCCESSFUL);
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10);
	}
}

TEST_P(MACStateFixture, SendFail) {
	const NetworkConfig& config = *GetParam();
	// we can't run this test for a coordinator
	if (config.self_addr != ADDR_COORD) {
		MAC test_mac(config);
		const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
		std::pair<Sequence, Sequence> seq;
		// set the slotters start expectations
		slot.start(seq);
		// and set the next state to what it would actually be
		slot.set_next_state(Slotter::State::SLOT_SEND, 5, seq);
		slot.set_next_state(Slotter::State::SLOT_SEND, 5, seq);
		// with those expectations, start the MAC!
		start(test_mac, config);
		// verify the sleep time
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5);
		// wake the MAC
		test_mac.wake_event();
		// check the state and current packet type
		EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
		EXPECT_EQ(test_mac.get_cur_send_addr(), test_mac.get_parent_addr());
		// send the packet!
		test_mac.send_event(PacketCtrl::DATA_TRANS);
		// test the state, current packet type, and sending address
		EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK_W_DATA);
		// recieve the packet!
		test_mac.recv_fail();
		// and that's a full transaction!
		EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
		EXPECT_EQ(test_mac.get_last_sent_status(), MAC::SendStatus::MAC_SEND_NO_ACK);
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10);
	}
}

TEST_P(MACStateFixture, SendWrongType) {
	const NetworkConfig& config = *GetParam();
	// we can't run this test for a coordinator
	if (config.self_addr != ADDR_COORD) {
		MAC test_mac(config);
		const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
		std::pair<Sequence, Sequence> seq;
		// set the slotters start expectations
		slot.start(seq);
		// and set the next state to what it would actually be
		slot.set_next_state(Slotter::State::SLOT_SEND, 5, seq);
		slot.set_next_state(Slotter::State::SLOT_SEND, 5, seq);
		// with those expectations, start the MAC!
		start(test_mac, config);
		// verify the sleep time
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5);
		// wake the MAC
		test_mac.wake_event();
		// check the state and current packet type
		EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
		EXPECT_EQ(test_mac.get_cur_send_addr(), test_mac.get_parent_addr());
		// send the packet!
		test_mac.send_event(PacketCtrl::DATA_TRANS);
		// test the state, current packet type, and sending address
		EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK_W_DATA);
		// recieve the packet!
		test_mac.recv_event(PacketCtrl::DATA_TRANS, test_mac.get_parent_addr());
		// and that's a full transaction!
		EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
		EXPECT_EQ(test_mac.get_last_sent_status(), MAC::SendStatus::MAC_SEND_WRONG_RESPONSE);
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10);
	}
}

TEST_P(MACStateFixture, SendWrongAddr) {
	const NetworkConfig& config = *GetParam();
	// we can't run this test for a coordinator
	if (config.self_addr != ADDR_COORD) {
		MAC test_mac(config);
		const StrictMock<MockSlotter>& slot = test_mac.get_slotter();
		std::pair<Sequence, Sequence> seq;
		// set the slotters start expectations
		slot.start(seq);
		// and set the next state to what it would actually be
		slot.set_next_state(Slotter::State::SLOT_SEND, 5, seq);
		slot.set_next_state(Slotter::State::SLOT_SEND, 5, seq);
		// with those expectations, start the MAC!
		start(test_mac, config);
		// verify the sleep time
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 5);
		// wake the MAC
		test_mac.wake_event();
		// check the state and current packet type
		EXPECT_EQ(test_mac.get_state(), State::MAC_SEND_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_TRANS);
		EXPECT_EQ(test_mac.get_cur_send_addr(), test_mac.get_parent_addr());
		// send the packet!
		test_mac.send_event(PacketCtrl::DATA_TRANS);
		// test the state, current packet type, and sending address
		EXPECT_EQ(test_mac.get_state(), State::MAC_RECV_RDY);
		EXPECT_EQ(test_mac.get_cur_packet_type(), PacketCtrl::DATA_ACK_W_DATA);
		// recieve the packet!
		test_mac.recv_event(PacketCtrl::DATA_ACK, 0x0001);
		// and that's a full transaction!
		EXPECT_EQ(test_mac.get_state(), State::MAC_SLEEP_RDY);
		EXPECT_EQ(test_mac.get_last_sent_status(), MAC::SendStatus::MAC_SEND_WRONG_RESPONSE);
		EXPECT_EQ(test_mac.wake_next_time(), m_now + m_data_sync + config.slot_length * 10);
	}
}

INSTANTIATE_TEST_SUITE_P(RefreshState, MACStateFixture,
	::testing::Values(	&MACStateFixture::config_coord, 
						&MACStateFixture::config_router2, 
						&MACStateFixture::config_router1,
						&MACStateFixture::config_end));
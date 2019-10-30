#include "LoomMAC2.h"

using namespace LoomNet;

MACState::MACState(const NetworkConfig& config)
	: m_state(State::MAC_CLOSED)
	, m_cur_packet_type(PacketCtrl::NONE)
	, m_slot(slot)
	, m_net_start(TIME_NONE)
	, m_self_type(self_type)
	, m_drift(drift) {}

uint16_t MACState::get_cur_addr() const {
	// if we're currently sending, return the address of our parent
	using SlotState = Slotter::State;
	const SlotState state = m_slot.get_state();
	if (state == SlotState::SLOT_SEND || state == SlotState::SLOT_SEND_W_SYNC)
		return get_parent(m_self_addr, m_self_type);
	// else if we're recieveing, calculate the current sending address from
	// the number of router children we have
	if (state == SlotState::SLOT_RECV || state == SlotState::SLOT_RECV_W_SYNC)
		return get_child(	m_self_addr, 
							m_self_type, 
							m_child_router_count, 
							m_slot.get_cur_device());
	// else we shouldn't be calling this function
	return ADDR_NONE;
}

TimeInterval MACState::wake_next_time() const {
	const Slotter::State state = m_slot.get_state();
	// waiting for a refresh
	if (state == Slotter::State::SLOT_WAIT_REFRESH) 
		return m_refresh_wait;
	// synchronizing (first action after a refresh)
	else if (state == Slotter::State::SLOT_RECV_W_SYNC || state == Slotter::State::SLOT_SEND_W_SYNC) 
		return m_data_wait;
	// normal transmission/recieve
	else 
		return m_slot_length * (m_slots_since_refresh + m_slot.get_slot_wait());
}

void MACState::begin() {
	if (m_self_type == DeviceType::COORDINATOR) {
		// tell the network to send a refresh packet
		m_state = State::MAC_SEND_REFRESH;
	}
	else
		m_state = State::MAC_WAIT_REFRESH;
	m_cur_packet_type = PacketCtrl::REFRESH_INITIAL;
}

// an end device or router has recieved a refresh packet, or a coordinator has sent the refresh packet, so the network starts now!
void MACState::refresh_event(const TimeInterval& timestamp, const TimeInterval& data, const TimeInterval& refresh) {
	if (m_state != State::MAC_WAIT_REFRESH && m_state != State::MAC_SEND_REFRESH) 
		m_halt_error(State::MAC_WAIT_REFRESH);
	// set the network
	m_data_wait = timestamp + data;
	m_refresh_wait = timestamp + refresh;
	m_slots_since_refresh = 0;
	// advance the slotter
	m_slot.next_state();
	// go to sleep
	m_state = State::MAC_SLEEP_RDY;
}

void MACState::wake_event() {
	const Slotter::State cur_state = m_slot.get_state();
	// increment the slot wait counter, if the wait was not synchronized
	if (cur_state == Slotter::State::SLOT_SEND || cur_state == Slotter::State::SLOT_RECV) 
		m_slots_since_refresh += m_slot.get_slot_wait();
	// if refresh, begin the MAC anew
	if (cur_state == Slotter::State::SLOT_WAIT_REFRESH)
		begin();
	// else we must be transmitting or recieving data
	else {
		if (cur_state == Slotter::State::SLOT_SEND || cur_state == Slotter::State::SLOT_SEND_W_SYNC)
			m_state = State::MAC_SEND_RDY;
		else if (cur_state == Slotter::State::SLOT_RECV || cur_state == Slotter::State::SLOT_RECV_W_SYNC)
			m_state = State::MAC_RECV_RDY;
		else m_halt_error(Error::MAC_INVALID_SLOTTER_STATE);
		// data time!
		m_cur_packet_type = PacketCtrl::DATA_TRANS;
	}
	// move the slotter forward
	m_slot.next_state();
}

void MACState::send_event(const PacketCtrl type) {
	if (m_state != State::MAC_SEND_RDY) 
		return m_halt_error(State::MAC_SEND_RDY);
	// based on the current packet type, wait for a new packet type or sleep
	if (type == PacketCtrl::DATA_TRANS || type == PacketCtrl::DATA_ACK_W_DATA) {
		if (m_cur_packet_type != PacketCtrl::DATA_ACK || m_cur_packet_type != PacketCtrl::DATA_ACK_W_DATA)
			m_halt_error(Error::MAC_INVALID_SEND_PACKET);
		// ready to recieve an acknowledgement!
		m_state = State::MAC_RECV_RDY;
		if (type == PacketCtrl::DATA_TRANS)
			m_cur_packet_type = PacketCtrl::DATA_ACK_W_DATA;
		else
			m_cur_packet_type = PacketCtrl::DATA_ACK;
	}
	// else the packet must be an ACK, so go to sleep
	else {
		if (m_cur_packet_type != PacketCtrl::DATA_ACK_W_DATA && m_cur_packet_type != PacketCtrl::DATA_ACK)
			m_halt_error(Error::MAC_INVALID_SEND_PACKET);
		m_state = State::MAC_SLEEP_RDY;
	}
}

void MACState::recv_event(const PacketCtrl type) {
	if (m_state != State::MAC_RECV_RDY)
		return m_halt_error(State::MAC_RECV_RDY);
	// check for an invalid packet type
	// if the packet type is not what we expected, BUT we allow an ACK when we expect a DATA_ACK
	if (type != m_cur_packet_type
		&& (m_cur_packet_type != PacketCtrl::DATA_ACK_W_DATA && type != PacketCtrl::DATA_ACK)) {
		// we were waiting for something but got something else
		m_last_send_status = SendStatus::MAC_SEND_WRONG_RESPONSE;
		m_state = State::MAC_SLEEP_RDY;
	}
	// packet type is valid!
	// based on the packet type recieved, send a new packet type or sleep
	else if (type == PacketCtrl::DATA_TRANS || type == PacketCtrl::DATA_ACK_W_DATA) {
		// send an ACK packet, but choose type based on the type
		if (type == PacketCtrl::DATA_TRANS)
			m_cur_packet_type = PacketCtrl::DATA_ACK_W_DATA;
		else {
			// we got an ACK as well, so send was good!
			m_last_send_status = SendStatus::MAC_SEND_SUCCESSFUL;
			m_cur_packet_type = PacketCtrl::DATA_ACK;
		}
		m_state = State::MAC_SEND_RDY;
	}
	// else it must be an ACK packet, so the send was good and go to sleep
	else {
		m_last_send_status = SendStatus::MAC_SEND_SUCCESSFUL;
		m_state = State::MAC_SLEEP_RDY;
	}
}
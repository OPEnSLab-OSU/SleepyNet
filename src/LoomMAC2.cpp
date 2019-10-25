#include "LoomMAC2.h"

using namespace LoomNet;

MACState::MACState(const DeviceType self_type, const Slotter& slot, const Drift& drift)
	: m_state(State::MAC_CLOSED)
	, m_cur_packet_type(PacketCtrl::NONE)
	, m_slot(slot)
	, m_net_start(TIME_NONE)
	, m_self_type(self_type)
	, m_drift(drift) {}

uint16_t MACState::get_cur_addr() const {
	
}

TimeInterval MACState::wake_next_time() const {
	const Slotter::State state = m_slot.get_state();
	// waiting for a refresh
	if (state == Slotter::State::SLOT_WAIT_REFRESH) return m_refresh_wait;
	// synchronizing (first action after a refresh)
	else if (state == Slotter::State::SLOT_RECV_W_SYNC
		|| state == Slotter::State::SLOT_SEND_W_SYNC) return m_data_wait;
	// normal transmission/recieve
	else return m_drift.slot_length * (m_slots_since_refresh + m_slot.get_slot_wait());
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
	if (m_state != State::MAC_WAIT_REFRESH && m_state != State::MAC_SEND_REFRESH) return m_halt_error(State::MAC_WAIT_REFRESH);
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
	if (cur_state == Slotter::State::SLOT_SEND
		|| cur_state == Slotter::State::SLOT_RECV) 
		m_slots_since_refresh += m_slot.get_slot_wait();
	// if refresh, begin the MAC anew
	if (cur_state == Slotter::State::SLOT_WAIT_REFRESH)
		begin();
	// else we must be transmitting or recieving data
	else {
		if (cur_state == Slotter::State::SLOT_SEND
			|| cur_state == Slotter::State::SLOT_SEND_W_SYNC) {
			m_state = State::MAC_SEND_RDY;
			// set the current send address to our parent
			m_cur_send_addr = get_parent(m_self_addr, m_self_type);
		}
		else if (cur_state == Slotter::State::SLOT_RECV
			|| cur_state == Slotter::State::SLOT_RECV_W_SYNC) {
			m_state = State::MAC_RECV_RDY;
			// TODO: set the current send address to our first child
			// maybe in the slotter?
		}
		else return m_halt_error(Error::MAC_INVALID_SLOTTER_STATE);
		// data time!
		m_cur_packet_type = PacketCtrl::DATA_TRANS;
	}
	// move the slotter forward
	m_slot.next_state();
}
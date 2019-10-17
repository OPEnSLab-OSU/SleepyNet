#include "LoomMAC.h"

LoomNet::MAC::MAC(const uint16_t self_addr, const DeviceType self_type, const Slotter& slot, const Drift& timing, Radio& radio)
	: m_slot(slot)
	, m_state(State::MAC_REFRESH_WAIT)
	, m_send_type(SendType::NONE)
	, m_last_error(Error::MAC_OK)
	, m_cur_send_addr(ADDR_NONE)
	, m_time_wake_start(TimeInterval::NONE, 0)
	, m_staging(PacketCtrl::NONE, ADDR_NONE)
	, m_staged(false)
	, m_next_refresh(TimeInterval::NONE, 0)
	, m_next_data(TimeInterval::NONE, 0)
	, m_fail_count(0)
	, m_radio(radio)
	, m_self_addr(self_addr)
	, m_self_type(self_type)
	, m_timings(timing) {
	// error check
	if (slot.get_state() == Slotter::State::SLOT_ERROR
		|| self_type == DeviceType::ERROR
		|| self_addr == ADDR_ERROR
		|| self_addr == ADDR_NONE)
		m_halt_error(Error::INVALID_CONFIG);
}

void LoomNet::MAC::reset() {
	m_state = State::MAC_REFRESH_WAIT;
	m_send_type = SendType::NONE;
	m_last_error = Error::MAC_OK;
	m_staged = false;
	m_staging = Packet(PacketCtrl::NONE, ADDR_NONE);
	m_next_refresh = TIME_NONE;
	m_next_data = TIME_NONE;
	m_time_wake_start = TIME_NONE;
	m_cur_send_addr = ADDR_NONE;
	m_slot.reset();
	m_fail_count = 0;
	// reset radio
	if (m_radio.get_state() == Radio::State::IDLE) m_radio.sleep();
	m_radio.disable();
	m_radio.enable();
	m_radio.wake();
}

void LoomNet::MAC::sleep_wake_ack() {
	// TODO: timing stuff here
	// in the meantime, we assume that the timing is always correct
	// update our state
	const Slotter::State cur_state = m_slot.get_state();
	if (cur_state == Slotter::State::SLOT_WAIT_REFRESH) {
		m_state = State::MAC_REFRESH_WAIT;
		m_send_type = SendType::NONE;
		// wake the radio premptivly to improve recieving response time
		m_radio.wake();
	}
	else if (cur_state == Slotter::State::SLOT_SEND || cur_state == Slotter::State::SLOT_SEND_W_SYNC) {
		m_state = State::MAC_DATA_SEND_RDY;
		m_send_type = SendType::MAC_DATA;
		// set the current send address to our parent
		m_cur_send_addr = get_parent(m_self_addr, m_self_type);
	}
	else if (cur_state == Slotter::State::SLOT_RECV || cur_state == Slotter::State::SLOT_RECV_W_SYNC) {
		m_state = State::MAC_DATA_WAIT;
		m_send_type = SendType::MAC_ACK_WITH_DATA;
		// wake the radio premptivly to improve recieving response time
		m_radio.wake();
	}
	else m_state = State::MAC_CLOSED;
	// reset the wake time to what it's supposed to be
	m_time_wake_start = sleep_next_wake_time();
	// move the slotter forward
	m_slot.next_state();
}

// simulation time is in slots remaining
// we do one slot for debugging purposes

LoomNet::TimeInterval LoomNet::MAC::sleep_next_wake_time() const {
	const Slotter::State state = m_slot.get_state();
	const TimeInterval rel_sleep = m_timings.slot_length * m_slot.get_slot_wait();
	if (state == Slotter::State::SLOT_WAIT_REFRESH)
		return m_next_refresh;
	// else if it's the first cycle, we have to account for synchronizing the data
	// cycle
	else if ((state == Slotter::State::SLOT_RECV_W_SYNC || state == Slotter::State::SLOT_SEND_W_SYNC) && !m_next_data.is_none())
		return m_next_data + rel_sleep;
	else
		return m_time_wake_start + rel_sleep + m_timings.slot_length;
}

// make sure the address is correct!

bool LoomNet::MAC::send_fragment(const Packet& frag) {
	// sanity check
	if (m_state == State::MAC_DATA_SEND_RDY && !m_staged) {
		// write to the "network"
		m_staging = frag;
		// set the ACK bit if needed
		if (m_send_type == SendType::MAC_ACK_WITH_DATA)
			m_staging.set_control(PacketCtrl::DATA_ACK_W_DATA);
		else
			m_staging.set_control(PacketCtrl::DATA_TRANS);
		// set the self address
		m_staging.set_src(m_self_addr);
		// commit the framecheck
		m_staging.set_framecheck();
		m_staged = true;
		// wake the radio if needed
		if (m_radio.get_state() == Radio::State::SLEEP) m_radio.wake();
		m_radio.send(m_staging);
		// change the state!
		if (m_send_type == SendType::MAC_DATA) {
			m_state = State::MAC_DATA_WAIT;
			m_send_type = SendType::MAC_ACK_NO_DATA;
		}
		else if (m_send_type == SendType::MAC_ACK_WITH_DATA) {
			m_state = State::MAC_DATA_WAIT;
			m_send_type = SendType::NONE;
		}
		else {
			m_state = State::MAC_SLEEP_RDY;
			m_radio.sleep();
		}
		return true;
	}
	return false;
}

LoomNet::Packet LoomNet::MAC::get_staged_packet() {
	if (m_staged) {
		if (m_state == State::MAC_DATA_RECV_RDY && m_send_type == SendType::MAC_ACK_WITH_DATA) {
			// next state!
			m_state = State::MAC_DATA_SEND_RDY;
			// additionally, set the send endpoint to the recieved address
			m_cur_send_addr = m_staging.get_src();
		}
		else {
			// next state!
			m_state = State::MAC_SLEEP_RDY;
			m_radio.sleep();
		}
		// get the packet
		m_staged = false;
		return m_staging;
	}
	return Packet{ PacketCtrl::ERROR, ADDR_ERROR };
}

void LoomNet::MAC::send_pass() {
	if (m_state == State::MAC_DATA_SEND_RDY) {
		// check if we need to send an ACK
		if (m_send_type == SendType::MAC_ACK_WITH_DATA) {
			// send a regular ACK instead of a fancy one
			m_send_ack();
		}
		// set the state to sleep
		m_state = State::MAC_SLEEP_RDY;
		// radio is already asleep
		if (m_radio.get_state() == Radio::State::IDLE)
			m_radio.sleep();
	}
}

LoomNet::MAC::State LoomNet::MAC::check_for_data() {
	if (m_state == State::MAC_DATA_WAIT) {
		// check our network
		TimeInterval stamp(TIME_NONE);
		const Packet& recv(m_radio.recv(stamp));
		m_check_count++;
		// check that the packet is not emptey, is not corrupted, and not from ourselves
		if (recv.get_control() != PacketCtrl::NONE
			&& recv.check_packet(m_cur_send_addr)
			&& recv.get_src() != m_self_addr) {
			// Serial.print("Took: ");
			// Serial.println(m_check_count);
			m_check_count = 0;
			// a packet! wow.
			// check to see if it's the right kind of packet
			const PacketCtrl ctrl = recv.get_control();
			if (ctrl == PacketCtrl::DATA_TRANS && m_send_type == SendType::MAC_ACK_WITH_DATA) {
				// first stage packet, recieve it then signal we're ready to send
				m_staging = recv;
				m_staged = true;
				m_state = State::MAC_DATA_RECV_RDY;
			}
			else if (ctrl == PacketCtrl::DATA_ACK_W_DATA && m_send_type == SendType::MAC_ACK_NO_DATA) {
				// second stage packet with data, send an ACK and tell the device
				// can recieve
				m_send_ack();
				// ready for next reply
				m_staging = recv;
				m_staged = true;
				m_state = State::MAC_DATA_RECV_RDY;
			}
			else if (ctrl == PacketCtrl::DATA_ACK
				&& (m_send_type == SendType::MAC_ACK_NO_DATA || m_send_type == SendType::NONE)) {

				// clear the staged packet, since it sent successfully
				m_staged = false;
				// set fail count to zero
				m_fail_count = 0;
				// got an ACK! Guess we're finished with this transaction
				m_state = State::MAC_SLEEP_RDY;
				m_radio.sleep();
			}
			else {
				// TODO: make error handling less brittle
				m_halt_error(Error::WRONG_PACKET_TYPE);
			}
		}
		else {
			if (recv.get_control() != PacketCtrl::NONE) {
				// Serial.println("Discarded corrputed packet");
			}
			// check the timeout
			const TimeInterval delta = m_radio.get_time() - m_time_wake_start;
			if (delta >= m_timings.min_drift) {
				m_check_count = 0;
				// I suppose you have nothing to say for yourself
				// very well
				// increment fail counter depending on if we transmitted and didn't get anything back
				if ((m_send_type == SendType::MAC_ACK_NO_DATA
					|| m_send_type == SendType::NONE)
					&& ++m_fail_count >= FAIL_MAX) {
					// reset the slotter, to trigger a refresh
					m_slot.reset();
				}
				m_send_type = SendType::NONE;
				if (m_staged) m_state = State::MAC_DATA_SEND_FAIL;
				else {
					m_state = State::MAC_SLEEP_RDY;
					m_radio.sleep();
				}
			}
		}
	}
	return m_state;
}

void LoomNet::MAC::data_pass() {
	// we don't or can't recieve whatever is happening for this slot
	// ignore all and move on
	if (m_state == State::MAC_DATA_WAIT) {
		m_state = State::MAC_SLEEP_RDY;
		if (m_radio.get_state() == Radio::State::IDLE) m_radio.sleep();
	}
}

void LoomNet::MAC::check_for_refresh() {
	// wake the radio if needed, since this is the first function called on startup
	if (m_radio.get_state() == Radio::State::DISABLED) {
		m_radio.enable();
		m_radio.wake();
	}
	// reset fail count since fail count is per batch
	m_fail_count = 0;
	// calculate the number of slots remaining until the next refresh using preconfigured values
	const uint32_t slots_until_refresh = m_slot.get_slots_per_refresh();
	// if we're a router or end device, just check and see if anyone has transmitted a signal
	// and if we haven't already (this should only happen on first power on) set the idle timestamp
	if (m_time_wake_start.is_none())
		m_time_wake_start = m_radio.get_time();
	if (m_self_type != DeviceType::COORDINATOR) {
		// check our "network"
		TimeInterval stamp(TIME_NONE);
		const Packet& recv(m_radio.recv(stamp));
		if (recv.get_control() != PacketCtrl::NONE) {
			// guess we got a refresh packet!
			// Additionally, we are supposed to do retransmission here, but I
			// have to ignore that for now
			// TODO: Retransmission
			if (recv.get_control() == PacketCtrl::REFRESH_INITIAL
				&& recv.check_packet(m_self_addr)) {
				const RefreshPacket& ref_frag = recv.as<RefreshPacket>();
				// set the next data and refresh cycle based on the data
				m_next_data = ref_frag.get_data_interval() + stamp;
				m_next_refresh = ref_frag.get_refresh_interval() + stamp;
				m_state = State::MAC_SLEEP_RDY;
				m_radio.sleep();
				// increment the slotter state, if we haven't already via sleep_wake_ack
				if (m_slot.get_state() == Slotter::State::SLOT_WAIT_REFRESH)
					m_slot.next_state();
			}
		}
		// if we haven't recieved anything, track how many slots it's been
		// if it's been over the reasonable number of slots, fail
		else {
			const TimeInterval delta = m_radio.get_time() - m_time_wake_start;
			const TimeInterval refresh_cycle_length = m_timings.slot_length * REFRESH_CYCLE_SLOTS;
			if (m_next_refresh.is_none()) {
				if (delta >= m_timings.slot_length * (slots_until_refresh + REFRESH_CYCLE_SLOTS)) {
					// first refresh didn't work, so hard fail
					m_halt_error(Error::REFRESH_TIMEOUT);
				}
			}
			else if (delta >= refresh_cycle_length - m_timings.max_drift) {
				// no refresh, but I guess we can just guess the values we got are still correct
				// create values based on preconfigured settings and previous timings
				m_next_data = m_time_wake_start + refresh_cycle_length;
				// subtract what we've already waited from the total slots until refersh
				m_next_refresh = m_time_wake_start + m_timings.slot_length * slots_until_refresh;
				m_state = State::MAC_SLEEP_RDY;
				m_radio.sleep();
			}
		}
	}
	// else we're a coordinator and we need to do the refresh ourselves
	else {
		// write to the network
		// TODO: exact timings
		// subtract the drift we correct with when waking up to transmit
		TimeInterval next_data_relative(m_timings.slot_length * REFRESH_CYCLE_SLOTS - m_timings.max_drift);
		TimeInterval next_refresh_relative(m_timings.slot_length * slots_until_refresh - m_timings.max_drift);
		// downcast both time intervals to the right sizes
		next_data_relative.downcast(UCHAR_MAX);
		next_refresh_relative.downcast(USHRT_MAX);
		// error check our timing stuff
		if (next_data_relative.is_none() || next_refresh_relative.is_none()) {
			m_halt_error(Error::REFRESH_PACKET_ERR);
			return;
		}
		// else send the packet!
		Packet frag = RefreshPacket::Factory(m_self_addr,
			next_data_relative,
			next_refresh_relative,
			0);
		frag.set_framecheck();
		const TimeInterval time_now(m_radio.get_time());
		m_radio.send(frag);
		// next state!
		m_next_data = next_data_relative + time_now;
		m_next_refresh = next_refresh_relative + m_timings.max_drift + time_now;
		m_state = State::MAC_SLEEP_RDY;
		m_radio.sleep();
		// increment the slotter state, if we haven't already via sleep_wake_ack
		if (m_slot.get_state() == Slotter::State::SLOT_WAIT_REFRESH)
			m_slot.next_state();
	}
}

void LoomNet::MAC::m_halt_error(const Error error) {
	m_last_error = error;
	m_state = State::MAC_CLOSED;
	// turn off radio (safely)
	while (m_radio.get_state() != Radio::State::DISABLED) {
		switch (m_radio.get_state()) {
		case Radio::State::IDLE:
			m_radio.sleep();
		case Radio::State::SLEEP:
			m_radio.disable();
		default: return;
		}
	}
}

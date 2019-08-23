#pragma once
#include <cstdint>
#include <map>
#include <array>
#include "LoomNetworkFragment.h"
#include "LoomNetworkUtility.h"
#include "LoomSlotter.h"

/** 
 * Loom Medium Access Control 
 * Operates synchronously
 */

/** This class will serve entirely for simulation for now */
namespace LoomNet {
	class MAC {
	public:

		enum class State {
			MAC_SLEEP_RDY,
			MAC_DATA_SEND_RDY,
			MAC_DATA_WAIT,
			MAC_DATA_RECV_RDY,
			MAC_DATA_SEND_FAIL,
			MAC_REFRESH_WAIT,
			MAC_CLOSED
		};

		enum class SendType {
			MAC_ACK_WITH_DATA,
			MAC_ACK_NO_DATA,
			MAC_DATA,
			NONE
		};

		enum class Error {
			MAC_OK,
			SEND_FAILED,
			WRONG_PACKET_TYPE,
			REFRESH_TIMEOUT,
		};

		MAC(	const uint16_t self_addr, 
				const DeviceType self_type, 
				const Slotter& slot, 
				const NetworkSim& network)
			: m_slot(slot)
			, m_state(State::MAC_REFRESH_WAIT)
			, m_send_type(SendType::NONE)
			, m_last_error(Error::MAC_OK)
			, m_cur_send_addr(ADDR_NONE)
			, m_time_idle_start(TimeInterval::NONE, 0)
			, m_staging(PacketCtrl::NONE, ADDR_NONE)
			, m_staged(false)
			, m_last_refresh_interval(TimeInterval::NONE, 0)
			, m_last_data_interval(TimeInterval::NONE, 0)
			, m_last_refresh_stamp(TimeInterval::NONE, 0)
			, m_network(network)
			, m_self_addr(self_addr)
			, m_self_type(self_type) {}

		bool operator==(const MAC& rhs) const {
			return (rhs.m_slot == m_slot)
				&& (rhs.m_self_addr == m_self_addr)
				&& (rhs.m_self_type == m_self_type);
		}

		State get_status() const { return m_state; }
		Error get_last_error() const { return m_last_error; }

		void reset() {
			m_state = State::MAC_REFRESH_WAIT;
			m_send_type = SendType::NONE;
			m_last_error = Error::MAC_OK;
			m_staged = false;
			m_staging = Packet(PacketCtrl::NONE, ADDR_NONE);
			m_last_refresh_interval = TIME_NONE;
			m_last_data_interval = TIME_NONE;
			m_last_refresh_stamp = TIME_NONE;
			m_time_idle_start = TIME_NONE;
			m_cur_send_addr = ADDR_NONE;
			m_slot.reset();
		}

		void sleep_wake_ack() {
			// TODO: timing stuff here
			// in the meantime, we assume that the timing is always correct
			// update our state
			const Slotter::State cur_state = m_slot.get_state();
			if (cur_state == Slotter::State::SLOT_WAIT_REFRESH) {
				m_state = State::MAC_REFRESH_WAIT;
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
			}
			else m_state = State::MAC_CLOSED;
			// move the slotter forward
			m_slot.next_state();
			// reset time slot idle counter
			m_time_idle_start = TimeInterval(TimeInterval::SECOND, m_network.get_time());
		}

		// simulation time is in slots remaining
		// we do one slot for debugging purposes
		TimeInterval sleep_next_wake_time() const {
			const Slotter::State state = m_slot.get_state();
			const TimeInterval time(TimeInterval::SECOND, m_network.get_time());
			if (state == Slotter::State::SLOT_WAIT_REFRESH) {
				if (m_last_refresh_interval == TIME_NONE) return TIME_NONE;
				const TimeInterval refresh_time = m_last_refresh_interval + m_last_refresh_stamp;
				return refresh_time > time
					? refresh_time - time
					: TIME_NONE;
			}
				
			// else if it's the first cycle, we have to account for synchronizing the data
			// cycle
			else if ((state == Slotter::State::SLOT_RECV_W_SYNC || state == Slotter::State::SLOT_SEND_W_SYNC) && m_last_refresh_stamp != TIME_NONE) {
				const TimeInterval data_time = m_last_data_interval + m_last_refresh_stamp;
				return data_time > time
					? data_time - time + TimeInterval(TimeInterval::SECOND, m_slot.get_slot_wait())
					: time + TimeInterval(TimeInterval::SECOND, m_slot.get_slot_wait());
			}
			else
				return { TimeInterval::Unit::SECOND, m_slot.get_slot_wait() };
		}

		uint16_t get_cur_send_address() const { return m_cur_send_addr; }

		// make sure the address is correct!
		bool send_fragment(const Packet& frag) {
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
				std::array<uint8_t, 255> m_out;
				for (uint8_t i = 0; i < m_staging.get_raw_length(); i++) m_out[i] = m_staging.get_raw()[i];
				m_network.net_write(m_out);
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
					m_send_type = SendType::NONE;
				}
				return true;
			}
			return false;
		}

		Packet get_staged_packet() {
			if (m_staged) {
				if (m_state == State::MAC_DATA_RECV_RDY) {
					// next state!
					if (m_send_type == SendType::MAC_ACK_WITH_DATA) {
						m_state = State::MAC_DATA_SEND_RDY;
						// additionally, set the send endpoint to the recieved address
						m_cur_send_addr = m_staging.get_src();
					}
					else {
						m_state = State::MAC_SLEEP_RDY;
						m_send_type = SendType::NONE;
					}
				}
				else if (m_state == State::MAC_DATA_SEND_FAIL) {
					// next state!
					m_state = State::MAC_SLEEP_RDY;
					m_send_type = SendType::NONE;
				}
				// get the packet
				m_staged = false;
				return m_staging;
			}
			return Packet{ PacketCtrl::ERROR, ADDR_ERROR };
		}

		void send_pass() {
			if (m_state == State::MAC_DATA_SEND_RDY) {
				// check if we need to send an ACK
				if (m_send_type == SendType::MAC_ACK_WITH_DATA) {
					// send a regular ACK instead of a fancy one
					m_send_ack();
				}
				// set the state to sleep
				m_state = State::MAC_SLEEP_RDY;
				m_send_type = SendType::NONE;
			}
		}

		State check_for_data() {
			if (m_state == State::MAC_DATA_WAIT) {
				// check our "network"
				const std::array<uint8_t, 255>& buf = m_network.net_read(true);
				const Packet recv(buf.data(), static_cast<uint8_t>(buf.size()));
				if (recv.get_control() != PacketCtrl::NONE) {
					// a packet! wow.
					// if the framecheck fails to verify, ignore the packet
					if (!recv.check_packet(m_cur_send_addr)) {
						// TODO: Add a "failed count" here
						m_send_type = SendType::NONE;
						if (m_staged) m_state = State::MAC_DATA_SEND_FAIL;
						else m_state = State::MAC_SLEEP_RDY;
						return m_state;
					}
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
						m_send_type = SendType::NONE;
						m_staging = recv;
						m_staged = true;
						m_state = State::MAC_DATA_RECV_RDY;
					}
					else if (ctrl == PacketCtrl::DATA_ACK 
						&& (m_send_type == SendType::MAC_ACK_NO_DATA || m_send_type == SendType::NONE)) {
						
						// clear the staged packet, since it sent successfully
						m_staged = false;
						// got an ACK! Guess we're finished with this transaction
						m_state = State::MAC_SLEEP_RDY;
						m_send_type = SendType::NONE;
					}
					else {
						// TODO: make error handling less brittle
						m_last_error = Error::WRONG_PACKET_TYPE;
						m_state = State::MAC_CLOSED;
					}
				}
				else {
					// check the timeout
					// TODO: Implement in terms of real time units
					const TimeInterval delta = TimeInterval(TimeInterval::SECOND, m_network.get_time()) - m_time_idle_start;
					if (delta.get_time() >= LOOPS_PER_SLOT) {
						// I suppose you have nothing to say for yourself
						// very well
						m_send_type = SendType::NONE;
						if (m_staged) m_state = State::MAC_DATA_SEND_FAIL;
						else m_state = State::MAC_SLEEP_RDY;
					}
				}
			}
			return m_state;
		}

		void check_for_refresh() {
			// calculate the number of slots remaining until the next refresh using preconfigured values
			const uint32_t slots_until_refresh = (m_slot.get_total_slots() + CYCLE_GAP) * (CYCLES_PER_BATCH - 1) - CYCLE_GAP
				+ BATCH_GAP + REFRESH_CYCLE_SLOTS;
			// if we're a router or end device, just check and see if anyone has transmitted a signal
			const TimeInterval time_now(TimeInterval::SECOND, m_network.get_time());
			if (m_self_type != DeviceType::COORDINATOR) {
				// check our "network"
				const std::array<uint8_t, 255> & buf = m_network.net_read(false);
				const Packet recv(buf.data(), static_cast<uint8_t>(buf.size()));
				if (recv.get_control() != PacketCtrl::NONE) {
					// reset the idle counter
					m_time_idle_start = TIME_NONE;
					// guess we got a refresh packet!
					// ignore everything in it for now, we'll deal with timing stuff later
					// TODO: timing stuff
					// Additionally, we are supposed to do retransmission here, but I
					// have to ignore that for now
					// TODO: Retransmission
					if (recv.get_control() == PacketCtrl::REFRESH_INITIAL
						&& recv.check_packet(m_self_addr)) {
						const RefreshPacket& ref_frag = recv.as<RefreshPacket>();
						// set the next data and refresh cycle based on the data
						// TODO: Do things based off of m_next_data_cycle
						m_last_data_interval = ref_frag.get_data_interval();
						m_last_refresh_interval = ref_frag.get_refresh_interval();
						m_last_refresh_stamp = time_now;
						m_state = State::MAC_SLEEP_RDY;
						// increment the slotter state, if we haven't already via sleep_wake_ack
						if (m_slot.get_state() == Slotter::State::SLOT_WAIT_REFRESH)
							m_slot.next_state();
					}
				}
				// if we haven't recieved anything, track how many slots it's been
				// if it's been over the reasonable number of slots, fail
				else {
					const TimeInterval delta = TimeInterval(TimeInterval::SECOND, m_network.get_time()) - m_time_idle_start;
					if (m_last_refresh_interval == TIME_NONE
						&& delta.get_time() >= slots_until_refresh + REFRESH_CYCLE_SLOTS) {
						// first refresh didn't work, so hard fail
						m_state = State::MAC_CLOSED;
						m_last_error = Error::REFRESH_TIMEOUT;
					}
					else if (delta.get_time() >= REFRESH_CYCLE_SLOTS) {
						// no refresh, but I guess we can just guess the values we got are still correct
						m_last_refresh_stamp = time_now;
						m_state = State::MAC_SLEEP_RDY;
					}
				}
			}
			// else we're a coordinator and we need to do the refresh ourselves
			else {
				// write to the network
				// TODO: exact timings
				m_last_data_interval = TimeInterval(TimeInterval::Unit::SECOND, REFRESH_CYCLE_SLOTS);
				m_last_refresh_interval = TimeInterval(TimeInterval::Unit::SECOND, slots_until_refresh);
				m_last_refresh_stamp = time_now;
				Packet frag = RefreshPacket::Factory(m_self_addr, m_last_data_interval, m_last_refresh_interval, 0);
				frag.set_framecheck();
				std::array<uint8_t, 255> temp;
				for (uint8_t i = 0; i < temp.size(); i++) temp[i] = frag.get_raw()[i];
				m_network.net_write(temp);
				// next state!
				m_state = State::MAC_SLEEP_RDY;
				// increment the slotter state, if we haven't already via sleep_wake_ack
				if (m_slot.get_state() == Slotter::State::SLOT_WAIT_REFRESH)
					m_slot.next_state();
			}
		}

		const Slotter& get_slotter() const { return m_slot; }

	private:

		void m_send_ack() {
			// send a regular ACK packet
			const Packet frag = ACKPacket::Factory(m_self_addr);
			std::array<uint8_t, 255> temp;
			for (uint8_t i = 0; i < temp.size(); i++) temp[i] = frag.get_raw()[i];
			m_network.net_write(temp);
		}

		Slotter m_slot;
		State m_state;
		SendType m_send_type;
		Error m_last_error;
		uint16_t m_cur_send_addr;
		TimeInterval m_time_idle_start;
		Packet m_staging;
		bool m_staged;
		TimeInterval m_last_refresh_interval;
		TimeInterval m_last_data_interval;
		TimeInterval m_last_refresh_stamp;
		const uint16_t m_self_addr;
		const DeviceType m_self_type;
		// variables used for simulation
		const NetworkSim& m_network;
	};
}

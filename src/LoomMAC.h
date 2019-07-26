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
			MAC_SEND_FAIL,
			MAC_WAIT_REFRESH,
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
			WRONG_PACKET_TYPE
		};

		MAC(	const uint16_t self_addr, 
				const DeviceType self_type, 
				const Slotter& slot, 
				const NetworkSim& network)
			: m_slot(slot)
			, m_state(State::MAC_WAIT_REFRESH)
			, m_send_type(SendType::NONE)
			, m_last_error(Error::MAC_OK)
			, m_cur_send_addr(ADDR_NONE)
			, m_slot_idle(0)
			, m_staging{}
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
			m_state = State::MAC_WAIT_REFRESH;
			m_last_error = Error::MAC_OK;
			m_slot.reset();
		}

		void sleep_wake_ack() {
			// TODO: timing stuff here
			// in the meantime, we assume that the timing is always correct
			// update our state
			const Slotter::State cur_state = m_slot.get_state();
			if (cur_state == Slotter::State::SLOT_WAIT_REFRESH) m_state = State::MAC_WAIT_REFRESH;
			else if (cur_state == Slotter::State::SLOT_WAIT_PARENT) {
				m_state = State::MAC_DATA_SEND_RDY;
				m_send_type = SendType::MAC_DATA;
			}
			else if (cur_state == Slotter::State::SLOT_WAIT_CHILD) {
				m_state = State::MAC_DATA_WAIT;
				m_send_type = SendType::MAC_ACK_WITH_DATA;
			}
			else m_state = State::MAC_CLOSED;
			// move the slotter forward
			m_slot.next_state();
			// reset our slot idle counter
			m_slot_idle = 0;
		}

		// simulation time is in slots remaining
		// we do one slot for debugging purposes
		TimeInterval sleep_next_wake_time() const {
			const Slotter::State state = m_slot.get_state();
			if (state == Slotter::State::SLOT_WAIT_REFRESH || state == Slotter::State::SLOT_WAIT_NEXT_DATA)
				return { TimeInterval::Unit::SECOND, 1 };
			else
				return { TimeInterval::Unit::SECOND, m_slot.get_slot_wait() };
		}

		uint16_t get_cur_send_address() const { return m_cur_send_addr; }

		bool send_fragment(DataFragment frag) {
			if (m_state == State::MAC_DATA_SEND_RDY) {
				if (frag.get_next_hop() != m_cur_send_addr) return false;
				// set the ACK bit if needed
				if (m_send_type == SendType::MAC_ACK_WITH_DATA)
					frag.set_is_ack(true);
				// commit the framecheck
				frag.calc_framecheck(frag.get_fragment_length() - 2);

				// write to the "network"
				for (auto i = 0; i < m_staging.size(); i++) m_staging[i] = frag[i];
				m_network.net_write(m_staging);
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

		DataFragment get_staged_packet() {
			if (m_state == State::MAC_DATA_RECV_RDY) {
				// next state!
				if (m_send_type == SendType::MAC_ACK_WITH_DATA)
					m_state = State::MAC_DATA_SEND_RDY;
				else
					m_state = State::MAC_SLEEP_RDY;
			}
			else if (m_state == State::MAC_SEND_FAIL) {
				// next state!
				m_state = State::MAC_SLEEP_RDY;
			}
			else return { ADDR_ERROR, ADDR_ERROR, ADDR_ERROR, 0, nullptr, 0, ADDR_ERROR };
			// get the packet
			return { m_staging.data(), m_staging.size() };
		}

		void send_pass() {
			if (m_state == State::MAC_DATA_SEND_RDY) {
				// check if we need to send an ACK
				if (m_send_type == SendType::MAC_ACK_WITH_DATA) {
					// send a regular ACK instead of a fancy one
					const ACKFragment frag(m_self_addr);
					std::array<uint8_t, 255> temp;
					for (auto i = 0; i < temp.size(); i++) temp[i] = frag[i];
					m_network.net_write(temp);
				}
				// set the state to sleep
				m_state = State::MAC_SLEEP_RDY;
				m_send_type = SendType::NONE;
			}
		}

		State check_for_data() {
			if (m_state == State::MAC_DATA_WAIT) {
				// check our "network"
				const std::array<uint8_t, 255>& recv = m_network.net_read();
				if (recv[0]) {
					// a packet! wow.
					// if the framecheck fails to verify, ignore the packet
					if (!check_packet(recv, m_cur_send_addr)) {
						// TODO: Add a "failed count" here
						return m_state = State::MAC_SLEEP_RDY;
					}
					// check to see if it's the right kind of packet
					const PacketCtrl& ctrl = get_packet_control(recv);
					if (ctrl == PacketCtrl::DATA_TRANS && m_send_type == SendType::MAC_ACK_WITH_DATA) {
						// first stage packet, recieve it then signal we're ready to send
						return m_state = State::MAC_DATA_RECV_RDY;
					}
					else if (ctrl == PacketCtrl::DATA_ACK_W_DATA && m_send_type == SendType::MAC_ACK_NO_DATA) {
						// second stage packet with data, send an ACK and tell the device
						// can recieve
						const ACKFragment frag(m_self_addr);
						std::array<uint8_t, 255> temp;
						for (auto i = 0; i < temp.size(); i++) temp[i] = frag[i];
						m_network.net_write(temp);
						// ready for next reply
						m_send_type = SendType::NONE;
						return m_state = State::MAC_DATA_RECV_RDY;
					}
					else if (ctrl == PacketCtrl::DATA_ACK 
						&& (m_send_type == SendType::MAC_ACK_NO_DATA || m_send_type == SendType::NONE)) {
						
						// got an ACK! Guess we're all done.
						m_state = State::MAC_SLEEP_RDY;
						m_send_type = SendType::NONE;
					}
					else {
						m_last_error = Error::WRONG_PACKET_TYPE;
						m_state = State::MAC_CLOSED;
					}
				}
				else {
					// increment the "timeout".
					if (++m_slot_idle == LOOPS_PER_SLOT) {
						// I suppose you have nothing to say for yourself
						// very well
						m_slot_idle = 0;
						return m_state = State::MAC_SEND_FAIL;
					}
				}
			}
		}

		bool check_for_refresh() {
			// if we're a router or end device, just check and see if anyone has transmitted a signal
			if (m_self_type != DeviceType::COORDINATOR) {

			}
			// else we're a coordinator and we need to do the refresh ourselves
			// TODO: timing stuff here?
			else {
			}
			return false;
		}

	private:

		Slotter m_slot;
		State m_state;
		SendType m_send_type;
		Error m_last_error;
		uint16_t m_cur_send_addr;
		uint8_t m_slot_idle;
		std::array<uint8_t, 255> m_staging;
		const uint16_t m_self_addr;
		const DeviceType m_self_type;
		// variables used for simulation
		const NetworkSim& m_network;
	};
}

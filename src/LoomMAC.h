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
			MAC_DATA_RECV_RDY,
			MAC_DATA_WAIT,
			MAC_WAIT_REFRESH,
			MAC_CLOSED
		};

		enum class Error {
			MAC_OK,
			SEND_FAILED
		};

		MAC(	const uint16_t self_addr, 
				const DeviceType self_type, 
				const Slotter& slot, 
				const NetworkSim& network)
			: m_slot(slot)
			, m_state(State::MAC_WAIT_REFRESH)
			, m_last_error(Error::MAC_OK)
			, m_cur_send_addr(ADDR_NONE)
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
			else if (cur_state == Slotter::State::SLOT_WAIT_PARENT)
				m_state = State::MAC_DATA_SEND_RDY;
			else if (cur_state == Slotter::State::SLOT_WAIT_CHILD)
				m_state = State::MAC_DATA_WAIT;
			else m_state = State::MAC_CLOSED;
			// move the slotter forward
			m_slot.next_state();
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
				// commit the framecheck
				frag.calc_framecheck(frag.get_fragment_length() - 2);
				// write to the "network"
				std::array<uint8_t, 255> temp;
				for (auto i = 0; i < temp.size(); i++) temp[i] = frag[i];
				m_network.net_write(temp);
				// change the state!
				m_state = State::MAC_DATA_WAIT;
				return true;
			}
			return false;
		}

		void send_pass() {
			if (m_state == State::MAC_DATA_SEND_RDY) {
				// update the state for the next sleep interval
				m_state = State::MAC_SLEEP_RDY;
			}
		}

		State check_for_data() {
			if (m_state == State::MAC_DATA_WAIT) {
				// check our "network"
				// normally we'd implement a timeout 
			}
		}

		DataFragment get_recv_fragment();

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
		Error m_last_error;
		uint16_t m_cur_send_addr;
		const uint16_t m_self_addr;
		const DeviceType m_self_type;
		// variables used for simulation
		const NetworkSim& m_network;
	};
}

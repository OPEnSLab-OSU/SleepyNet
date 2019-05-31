#pragma once
#include <cstdint>
#include <map>
#include <array>
#include "LoomNetworkFragment.h"
#include "LoomNetworkUtility.h"

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
			MAC_WAIT_REFRESH,
			MAC_CLOSED
		};

		enum class Error {
			MAC_OK,
			SEND_FAILED,
		};

		MAC(uint16_t self_addr, std::map<uint16_t, std::array<uint8_t, 255>>& network_sim)
			: m_state(State::MAC_WAIT_REFRESH)
			, m_last_error(Error::MAC_OK)
			, m_network(network_sim)
			, m_self_addr(self_addr) {
			// add our address to the map (SIMULATION ONLY)
			m_network.emplace(self_addr, std::array<uint8_t, 255>());
		}

		State get_state() const { return m_state; }
		Error get_last_error() const { return m_last_error; }
		void reset() {
			m_state = State::MAC_WAIT_REFRESH;
			m_last_error = Error::MAC_OK;
		}

		void sleep_wake_ack() {

		}
		// simulation no time
		TimeInverval sleep_next_wake_time() const { return TimeInverval(TimeInverval::Unit::SECOND, 0); };
		uint16_t get_cur_send_address() const {

		}
		bool send_fragment(const Fragment& frag) {
			if (frag.get_next_hop() != m_cur_send_addr) {
				// uh oh
				return false;
			}
			// write to the "network"
			std::array<uint8_t, 255> & send_ray = m_network.at(frag.get_next_hop());
			// clear the array
			for (auto i = 0; i < send_ray.size(); i++) send_ray[i] = 0;
			// write our data
			if (!frag.to_raw(send_ray)) return false;
			// data is sent! now to change the state
			// check if we have data to "recieve"
			std::array<uint8_t, 255> & recv_ray = m_network.at(m_self_addr);

			return true;
		}
		bool send_pass();
		uint16_t get_cur_recv_address() const;
		Fragment recv_fragment();
		bool check_for_refresh();

	private:
		State m_state;
		Error m_last_error;
		uint16_t m_cur_send_addr;
		const uint16_t m_self_addr;

		// varibles used for simulation
		std::map<uint16_t, std::array<uint8_t, 255>> &m_network;
	};
}

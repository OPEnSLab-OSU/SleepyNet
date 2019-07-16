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
			MAC_DATA_TRANS_RDY,
			MAC_WAIT_REFRESH,
			MAC_CLOSED
		};

		enum class Error {
			MAC_OK,
			SEND_FAILED,
		};

		MAC(const uint16_t self_addr, const DeviceType self_type, std::map<uint16_t, std::array<uint8_t, 255>>& network_sim)
			: m_state(State::MAC_WAIT_REFRESH)
			, m_last_error(Error::MAC_OK)
			, m_cur_send_addr(ADDR_NONE)
			, m_network(network_sim)
			, m_self_addr(self_addr)
			, m_self_type(self_type) {
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
		TimeInterval sleep_next_wake_time() const { return { TimeInterval::Unit::SECOND, 0 }; };
		uint16_t get_cur_send_address() const {

		}
		bool send_fragment(const DataFragment& frag) {
			if (frag.get_next_hop() != m_cur_send_addr) {
				// uh oh
				return false;
			}
			// write to the "network"
			std::array<uint8_t, 255> & send_ray = m_network.at(frag.get_next_hop());
			// write the network fragment to the array
			for (auto i = 0; i < send_ray.size(); i++) send_ray[i] = frag.get_raw[i];
			// check if we have data to "recieve"
			std::array<uint8_t, 255> & recv_ray = m_network.at(m_self_addr);
			// data is sent! now to change the state

			return true;
		}
		bool send_pass();
		uint16_t get_cur_recv_address() const;
		Fragment recv_fragment();

		bool check_for_refresh() {
			// if we're a router or end device, just check and see if anyone has transmitted a signal
			if (m_self_type != DeviceType::COORDINATOR) {
				if (m_network.at(m_self_addr)[2]) {
					// parse refresh data, change state
				}
			}
			// else we're a coordinator and we need to do the refresh ourselves
			// TODO: timing stuff here?
			else {
				for (auto buf : m_network) {
					buf = 
				}
			}
		}

	private:
		State m_state;
		Error m_last_error;
		uint16_t m_cur_send_addr;
		const uint16_t m_self_addr;
		const DeviceType m_self_type;

		// varibles used for simulation
		std::map<uint16_t, std::array<uint8_t, 255>> &m_network;
	};
}

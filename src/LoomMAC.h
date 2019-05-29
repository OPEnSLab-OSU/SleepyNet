#pragma once
#include <cstdint>
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

		State get_state() const { return m_state; }
		Error get_last_error() const { return m_last_error; }
		void reset();

		void sleep_wake_ack();
		TimeInverval sleep_next_wake_time() const;
		uint16_t get_cur_send_address() const;
		bool send_fragment(const Fragment& frag);
		bool send_pass();
		uint16_t get_cur_recv_address() const;
		Fragment recv_fragment();
		bool check_for_refresh();

	private:
		uint16_t m_mac_slot;
		Fragment m_recv;
		State m_state;
		Error m_last_error;

	};
}

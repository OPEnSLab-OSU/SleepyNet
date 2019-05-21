#pragma once
#include <cstdint>
#include "LoomNetworkFragment.h"
#include "LoomTimeInterval.h"

/** 
 * Loom Medium Access Control 
 * Operates synchronously
 */

/** This class will serve entirely for simulation for now */

class LoomMAC {
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

	void sleep_wake_ack();
	LoomTimeInverval sleep_next_wake_time() const;
	uint16_t get_cur_send_address() const;
	bool send_fragment(const LoomNetworkFragment& frag);
	bool send_pass();
	uint16_t get_cur_recv_address() const;
	LoomNetworkFragment recv_fragment();
	bool check_for_refresh();

private:
	enum class InternalState {
		SLEEP_FOR_REFRESH,
		SLEEP_FOR_DATA,
	};

	LoomNetworkFragment m_recv;
	State m_state;
	InternalState m_state;
	Error m_last_error;

};
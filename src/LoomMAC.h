#pragma once
#include <cstdint>
#include "LoomNetworkFragment.h"

/** Loom Medium Access Control */

/** This class will serve entirely for simulation for now */

class LoomMAC {
public:

	enum Status : uint8_t {
		MAC_SLEEP_RDY = 1,
		MAC_RECV_RDY = (1 << 2),
		MAC_SEND_RDY = (1 << 3),
	};

	enum class Error {
		MAC_OK,
		SEND_FAILED,
	};

	Status get_status() const { return m_status; }
	Error get_last_error() const { return m_last_error; }

	void mac_sleep_wake_ack();

	// avoid duplication?
	bool send_fragment(const LoomNetworkFragment& frag)

private:
	enum class InternalState {
		SLEEP_FOR_REFRESH,
		SLEEP_FOR_DATA,
	};

	Status m_status;
	InternalState m_state;
	Error m_last_error;

};
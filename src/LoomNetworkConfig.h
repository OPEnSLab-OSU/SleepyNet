#pragma once
#include "LoomNetworkUtility.h"
#include "LoomNetworkTime.h"
#include <ArduinoJson.h>
#include <stdint.h>
/** 
 * Convert the Loom Network topology JSON into information used by the network
 * For now this will only populate LoomRouter with information, as LoomMAC has not been written.
 */

namespace LoomNet {
	struct NetworkConfig {
		const uint8_t send_slot;
		const uint16_t total_slots;
		const uint8_t cycles_per_refresh;
		const uint8_t cycle_gap;
		const uint8_t batch_gap;
		const uint8_t send_count;
		const uint8_t recv_slot;
		const uint8_t recv_count;

		const uint8_t self_addr;
		const uint8_t child_router_count;
		const uint8_t child_node_count;

		const TimeInterval min_drift;
		const TimeInterval max_drift;
		const TimeInterval slot_length;
	};

	const NetworkConfig NETWORK_ERROR = { SLOT_ERROR, 0, 0, 0, 0, 0, SLOT_ERROR, 0, ADDR_ERROR, 0, 0, TIME_NONE, TIME_NONE, TIME_NONE };

	uint16_t get_addr(const JsonObjectConst& topology, const char* name);
	NetworkConfig read_network_topology(const JsonObjectConst& topology, const char* self_name);
}

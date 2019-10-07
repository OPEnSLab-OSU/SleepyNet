#pragma once
#include "LoomNetworkInfo.h"
#include "LoomNetworkUtility.h"
#include "LoomNetworkTime.h"
#include <ArduinoJson.h>
#include <stdint.h>
/** 
 * Convert the Loom Network topology JSON into information used by the network
 * For now this will only populate LoomRouter with information, as LoomMAC has not been written.
 */

namespace LoomNet {
	uint16_t get_addr(const JsonObjectConst& topology, const char* name);
	NetworkInfo read_network_topology(const JsonObjectConst& topology, const char* self_name);
}

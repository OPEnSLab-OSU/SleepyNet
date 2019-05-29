#pragma once
#include "LoomRouter.h"
#include "LoomNetworkUtility.h"
#include <ArduinoJson.h>
#include <cstdint>
#include <limits>

/** 
 * Convert the Loom Network topology JSON into information used by the network
 * For now this will only populate LoomRouter with information, as LoomMAC has not been written.
 */

static uint16_t m_recurse_traverse(const JsonArrayConst& children, const char* self_name, const uint8_t router_count = 0, const uint8_t depth = 0) {
	uint8_t node_count = 1;
	uint8_t cur_router_count = 1;

	for (JsonVariant device : children) {
		if (device == NULL || !device.is<JsonObject>()) return std::numeric_limits<uint16_t>::max();
		// get the device type
		const uint8_t type = device["type"].as<uint8_t>() | 255;
		if (type == 255) return std::numeric_limits<uint16_t>::max();

		uint16_t node_address_part = 0;
		bool found = false;
		// compare the device name
		const char* name = device["name"].as<const char*>();
		if (!strncmp(name, self_name, LoomNet::STRING_MAX)) {
			// we found it!
			// if it's a node, return the node count for processing later
			if (type == 0) node_address_part = node_count;
			// else if it's a router, return the router count
			else if (type == 1)
				node_address_part = (depth == 0) ? (static_cast<uint16_t>(cur_router_count) << 8)
				: (static_cast<uint16_t>(cur_router_count) << 12);
			// else leave it at zero, and add the routers
			found = true;
		}
		else {
			// if it's a node, incrememnt node counter
			if (type == 0) node_count++;
			// else recurse through the router children array
			else {
				// check all the children for our name
				node_address_part = m_recurse_traverse(device["children"].as<JsonArrayConst>(), self_name, cur_router_count, depth + 1);
				// increment the router counter
				if (!node_address_part) cur_router_count++;
				else found = true;
			}
		}

		if (found) {
			// recursed twice, add the second router count
			if (depth == 2) return node_address_part | (static_cast<uint16_t>(router_count) << 8);
			// else recursed once, add the first router count
			if (depth == 1) return node_address_part | (static_cast<uint16_t>(router_count) << 12);
			// else we're good to go
			return node_address_part;
		}
	}
	// guess we didn't find anything
	return 0;
}

namespace LoomNet {
	Router read_network_topology(const JsonObjectConst& topology, const char* self_name) {
		// coordinator special case
		const char* name = topology["root"]["name"].as<const char*>();
		if (name != NULL && !strncmp(name, self_name, STRING_MAX)) {
			return {
				DeviceType::COORDINATOR,
				ADDR_COORD,
				ADDR_NONE,
			};
		}
		const auto thing = topology["root"]["children"].as<JsonArrayConst>();
		// search the tree for our device name, keeping track of how many routers we've traversed
		const uint16_t address = m_recurse_traverse(topology["root"]["children"].as<JsonArrayConst>(), self_name);
		// figure out device type and parent address from there
		// device type
		DeviceType type;
		// if theres any node address, it's an end device
		if (address & 0x00FF) type = DeviceType::END_DEVICE;
		// else it's a router
		else {
			// if theres a first router in the address, check if there's a second
			if (address & 0x0F00) type = DeviceType::SECOND_ROUTER;
			else type = DeviceType::FIRST_ROUTER;
		}
		// address parent
		uint16_t parent;
		// remove node address from end device
		if (type == DeviceType::END_DEVICE) parent = address & 0xFF00;
		// remove second router address from secound router
		else if (type == DeviceType::SECOND_ROUTER) parent = address & 0xF000;
		// parent of first router is always coordinator
		else parent = ADDR_COORD;
		// return these things!
		return {
			type,
			address,
			parent,
		};
	}
}

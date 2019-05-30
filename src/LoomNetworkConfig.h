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

static uint16_t m_recurse_traverse(const JsonArrayConst& children, const char* self_name, const uint16_t router_count = 0, const uint8_t depth = 0) {
	uint16_t node_count = 1;
	uint16_t cur_router_count = 1;

	for (JsonObjectConst device : children) {
		if (device.isNull()) return LoomNet::ADDR_ERROR;
		// get the device type
		const uint8_t type = device["type"] | static_cast<uint8_t>(255);
		if (type == 255) return LoomNet::ADDR_ERROR;

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
				node_address_part = (depth == 0) ? (cur_router_count << 12)
				: (cur_router_count << 8);
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
			if (depth == 2) return node_address_part | (router_count << 8);
			// else recursed once, add the first router count
			if (depth == 1) return node_address_part | (router_count << 12);
			// else we're good to go
			return node_address_part;
		}
	}
	// guess we didn't find anything
	return LoomNet::ADDR_NONE;
}

static JsonObjectConst m_find_router(const JsonArrayConst& my_array, const uint16_t router_index) {
	uint16_t cur_router_index = 0;
	// iterate through the children array, looking for our router
	for (JsonObjectConst obj : my_array) {
		// error checks
		if (obj.isNull()) return JsonObjectConst();
		// get the device type
		const uint8_t type = obj["type"] | static_cast<uint8_t>(255);
		if (type == 255) return JsonObjectConst();
		// if it's a router, check if it's our router
		if (type == 1) {
			if (cur_router_index == router_index) return obj;
			// else increment the router count and keep looking
			else cur_router_index++;
		}
	}
	return JsonObjectConst();
}

namespace LoomNet {
	Router read_network_topology(const JsonObjectConst& topology, const char* self_name) {
		// device type
		DeviceType type = DeviceType::ERROR;
		// address
		uint16_t address = ADDR_ERROR;
		// parent
		uint16_t parent = ADDR_ERROR;
		// root array of node children
		const JsonArrayConst root_array = topology["root"]["children"].as<JsonArrayConst>();
		// coordinator special case
		const char* name = topology["root"]["name"].as<const char*>();
		if (name != NULL && !strncmp(name, self_name, STRING_MAX)) {
			type = DeviceType::COORDINATOR;
			address = ADDR_COORD;
			parent = ADDR_NONE;
		}
		// else its a router or end device!
		else {
			// search the tree for our device name, keeping track of how many routers we've traversed
			address = m_recurse_traverse(root_array, self_name);
			// error if not found
			if (address == ADDR_NONE || address == ADDR_ERROR) return ROUTER_ERROR;
			// figure out device type and parent address from there

			// if theres any node address, it's an end device
			if (address & 0x00FF) type = DeviceType::END_DEVICE;
			// else it's a router
			else {
				// if theres a first router in the address, check if there's a second
				if (address & 0x0F00) type = DeviceType::SECOND_ROUTER;
				else type = DeviceType::FIRST_ROUTER;
			}
			// remove node address from end device
			if (type == DeviceType::END_DEVICE) parent = address & 0xFF00;
			// remove second router address from secound router
			else if (type == DeviceType::SECOND_ROUTER) parent = address & 0xF000;
			// parent of first router is always coordinator
			if (type == DeviceType::FIRST_ROUTER || !parent) parent = ADDR_COORD;
		}
		// next, we need to find our devices children in the JSON
		// find the array of children we would like to search
		JsonArrayConst ray = JsonArrayConst();
		if (type == DeviceType::COORDINATOR) ray = root_array;
		else if (type == DeviceType::FIRST_ROUTER || type == DeviceType::SECOND_ROUTER) {
			// first level search
			JsonObjectConst obj = m_find_router(root_array, (address >> 12) - 1);
			if (type == DeviceType::SECOND_ROUTER) {
				// we need to search another layer for the second router, so do that
				obj = m_find_router(obj["children"], ((address & 0x0F00) >> 8) - 1);
			}
			if (obj.isNull() || obj["children"].isNull()) return ROUTER_ERROR;
			// we found it! neat.
			ray = obj["children"];
		}
		// next, measure how many end devices and routers are children are underneath our device
		uint8_t router_count = 0;
		uint8_t node_count = 0;
		// iterate through the children array, counting nodes and routers
		for (JsonObjectConst obj : ray) {
			if (obj.isNull()) return ROUTER_ERROR;
			// get the device type
			const uint8_t type = obj["type"] | static_cast<uint8_t>(255);
			// if it's a router, increment routers
			if (type == 1) router_count++;
			// else if it's a node, incremement nodes
			else if (type == 0) node_count++;
			// else uh oh
			else return ROUTER_ERROR;
		}
		// return these things!
		return {
			type,
			address,
			parent,
			router_count,
			node_count,
		};
	}
}

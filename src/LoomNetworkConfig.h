#pragma once
#include "LoomRouter.h"
#include "LoomSlotter.h"
#include "LoomNetworkUtility.h"
#include "LoomNetworkInfo.h"
#include <ArduinoJson.h>
#include <cstdint>
#include <limits>

/** 
 * Convert the Loom Network topology JSON into information used by the network
 * For now this will only populate LoomRouter with information, as LoomMAC has not been written.
 */

static uint16_t m_recurse_traverse(const JsonObjectConst& parent, const char* self_name, JsonObjectConst& self_obj, uint8_t& self_depth, const uint16_t router_count = 0, const uint8_t depth = 0) {
	uint16_t node_count = 1;
	uint16_t cur_router_count = 1;
	const JsonArrayConst children = parent["children"];

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
			self_obj = device;
			self_depth = depth;
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
				node_address_part = m_recurse_traverse(device, self_name, self_obj, self_depth, cur_router_count, depth + 1);
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

static uint8_t m_count_slots(const JsonArrayConst& children) {
	if (children.isNull()) return 0;
	// traverse the tree, adding up slots in a depth-first manner
	uint8_t total = 0;
	for (JsonObjectConst device : children) {
		// get device type
		const uint8_t type = device["type"] | 255;
		// if the device is an end device, add a slot
		if (type == 255) return 0;
		else if (type == 0) total++;
		// if the device is a first or second router, search the children recursivley
		else if (type == 1) {
			// add a slot for sensing capabilities
			if (!(device["sensor"].as<bool>() | true)) total++;
			// search children
			total += m_count_slots(device["children"]);
		}
	}
	return total;
}

static uint8_t m_count_slots_layer(const JsonObjectConst& obj, const uint8_t layer, const char* self_name, const LoomNet::DeviceType self_type, bool& found_device) {
	// create a breadth-first array of the nodes so we can traverse them in reverse order
	const JsonArrayConst children = obj["children"];
	// if we're on the right layer, count all the slots of the objects to the left of this one
	uint8_t total = 0;
	if (layer == 0) {
		// if we found an end device, add one to the total
		if (children.isNull()) total++;
		// else search the children
		else {
			// count only routers first
			for (const auto device : children) {
				const uint8_t type = device["type"] | static_cast<uint8_t>(255);
				if (type == 255) return LoomNet::SLOT_ERROR;
				if (type == 1) {
					if ((self_type == LoomNet::DeviceType::FIRST_ROUTER
						|| self_type == LoomNet::DeviceType::SECOND_ROUTER)
						&& !strncmp(device["name"].as<const char*>(), self_name, LoomNet::STRING_MAX)) {
						// we found our device! return how many slots we counted
						found_device = true;
						return total;
					}
					// else continue counting
					else {
						total += m_count_slots(device["children"]);
						// add a slot for sensing capabilities
						if (!(device["sensor"].as<bool>() | true)) total++;
					}
				}
			}
			// next count end devices
			for (const auto device : children) {
				const uint8_t type = device["type"] | static_cast<uint8_t>(255);
				if (type == 255) return LoomNet::SLOT_ERROR;
				if (type == 0) {
					if (self_type == LoomNet::DeviceType::END_DEVICE
						&& !strncmp(device["name"].as<const char*>(), self_name, LoomNet::STRING_MAX)) {
						// we found our device! return how many slots we counted
						found_device = true;
						return total;
					}
					// else continue counting
					else total++;
				}
			}
		}
	}
	// else recurse until we are
	else 
		for (const auto device : children) {
			if (!found_device)
				total += m_count_slots_layer(device, layer - 1, self_name, self_type, found_device);
			else return total;
		}
			
	return total;
}

static uint8_t m_count_slots_layer_call(const JsonObjectConst& obj, const uint8_t layer, const char* self_name, const LoomNet::DeviceType self_type) {
	bool found_device = false;
	return m_count_slots_layer(obj, layer, self_name, self_type, found_device);
}

namespace LoomNet {
	NetworkInfo read_network_topology(const JsonObjectConst& topology, const char* self_name) {
		// device type
		DeviceType type = DeviceType::ERROR;
		// layer
		const uint8_t layer = 0;
		// address
		uint16_t address = ADDR_ERROR;
		// parent
		uint16_t parent = ADDR_ERROR;
		// root array of node children
		const JsonObjectConst root_obj = topology["root"];
		// track a object with a json refrence to our object
		JsonObjectConst self_obj;
		uint8_t depth = 0;
		// coordinator special case
		const char* name = root_obj["name"].as<const char*>();
		if (name != NULL && !strncmp(name, self_name, STRING_MAX)) {
			type = DeviceType::COORDINATOR;
			address = ADDR_COORD;
			parent = ADDR_NONE;
		}
		// else its a router or end device!
		else {
			// search the tree for our device name, keeping track of how many routers we've traversed
			address = m_recurse_traverse(root_obj, self_name, self_obj, depth);
			// error if not found
			if (address == ADDR_NONE || address == ADDR_ERROR) return { ROUTER_ERROR, SLOTTER_ERROR };
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
		JsonArrayConst ray = self_obj["children"];
		if (type == DeviceType::COORDINATOR) ray = root_obj["children"];
		// next, measure how many end devices and routers are children are underneath our device
		uint8_t router_count = 0;
		uint8_t node_count = 0;
		// iterate through the children array, counting nodes and routers
		for (JsonObjectConst obj : ray) {
			if (obj.isNull()) return { ROUTER_ERROR, SLOTTER_ERROR };
			// get the device type
			const uint8_t type = obj["type"] | static_cast<uint8_t>(255);
			// if it's a router, increment routers
			if (type == 1) router_count++;
			// else if it's a node, incremement nodes
			else if (type == 0) node_count++;
			// else uh oh
			else return { ROUTER_ERROR, SLOTTER_ERROR };
		}
		// next, we need to determine the device's layer and slot information
		// collect the slot for the devices self and first child
		// get self slot
		uint8_t self_slot = SLOT_ERROR;
		if (type != DeviceType::COORDINATOR) self_slot = m_count_slots_layer_call(root_obj, depth, self_obj["name"], type);
		else if (type != DeviceType::ERROR) self_slot = SLOT_NONE;
		// if the device is a router, get the first child and it's slot
		uint8_t child_slot = SLOT_ERROR;
		uint8_t child_slot_count = 0;
		if (type != DeviceType::END_DEVICE) {
			// find the highest priority child
			JsonObjectConst highest_child;
			const JsonArrayConst children = self_obj["children"];
			for (const JsonObjectConst child : children) {
				const uint8_t type = child["type"] | static_cast<uint8_t>(255);
				if (type == 1) {
					highest_child = child;
					break;
				}
				else if (type == 0 && highest_child.isNull()) highest_child = child;
				else break;
			}
			// count it's slots
			if (!highest_child.isNull()) {
				const uint8_t type = highest_child["type"] | static_cast<uint8_t>(255);
				const DeviceType detailed_type = type == 0
					? DeviceType::END_DEVICE
					: (depth == 1
						? DeviceType::FIRST_ROUTER
						: DeviceType::SECOND_ROUTER);
				child_slot = m_count_slots_layer_call(root_obj, depth + 1, highest_child["name"], detailed_type);
				child_slot_count = m_count_slots(self_obj["children"]);
			}
		}
		else if (type != DeviceType::ERROR) child_slot = SLOT_NONE;
		// return it all! jesus christ
		return {
			{
				type,
				address,
				parent,
				router_count,
				node_count,
			},
			{
				self_slot,
				child_slot,
				child_slot_count,
				topology["config"]["cycles_per_refresh"].as<uint8_t>()
			}
		};
	}
}
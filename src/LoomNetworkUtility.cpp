#include "LoomNetworkUtility.h"

using namespace LoomNet;

static uint16_t short_min(uint16_t a, uint16_t b) { return a < b ? a : b; }

DeviceType LoomNet::get_type(const uint16_t addr) {
	// error check
	if (addr == ADDR_NONE || addr == ADDR_ERROR) return DeviceType::ERROR;
	// coordinator check
	if (addr == ADDR_COORD) return DeviceType::COORDINATOR;
	// figure out device type and parent address from there
	// if theres any node address, it's an end device
	if (addr & 0x00FF) return DeviceType::END_DEVICE;
	// else it's a router
	// if theres a first router in the address, check if there's a second
	if (addr & 0x0F00) return DeviceType::SECOND_ROUTER;
	return DeviceType::FIRST_ROUTER;
}

uint16_t LoomNet::get_parent(const uint16_t addr, const DeviceType type) {
	if (addr == ADDR_NONE || addr == ADDR_ERROR || type == DeviceType::ERROR)
		return ADDR_ERROR;
	// remove node address from end device
	if (type == DeviceType::END_DEVICE) {
		const uint16_t router_parent = addr & 0xFF00;
		if (router_parent) return router_parent;
		return ADDR_COORD;
	}
	// remove second router address from secound router
	if (type == DeviceType::SECOND_ROUTER) return addr & 0xF000;
	// parent of first router is always coordinator
	if (type == DeviceType::FIRST_ROUTER) return ADDR_COORD;
	// huh
	return ADDR_ERROR;
}

uint16_t LoomNet::get_child(const uint16_t self_addr, const DeviceType type, const uint8_t child_router_count, const uint8_t child_num) {
	if (self_addr == ADDR_NONE 
		|| self_addr == ADDR_ERROR 
		|| type == DeviceType::ERROR 
		|| type == DeviceType::END_DEVICE
		|| (type == DeviceType::SECOND_ROUTER
			&& child_router_count > 0))
		return ADDR_ERROR;
	// if coordinator, add first routers
	uint16_t child_addr = self_addr;
	const uint16_t router_number = short_min(child_router_count, child_num);
	if (type == DeviceType::COORDINATOR)
		child_addr |= router_number << 12;
	// if first router, add second routers
	else if (type == DeviceType::FIRST_ROUTER)
		child_addr |= router_number << 8;
	// else it's a second router, and doesn't need a router component in the address
	// add the end device
	return child_addr | (child_num - router_number);
}
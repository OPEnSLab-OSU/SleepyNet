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
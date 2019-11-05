#include "LoomRouter.h"

LoomNet::Router::Router(const uint16_t self_addr, const uint8_t router_count, const uint8_t node_count)
	: m_dev_type(get_type(self_addr))
	, m_self_addr(self_addr)
	, m_addr_parent(get_parent(self_addr, m_dev_type))
	, m_node_child_count(node_count)
	, m_router_child_count(router_count) {}

// NOTE: it is presumed this function will never route to itself, and does not check if the address returned
// it actually present in the network

uint16_t LoomNet::Router::route(const uint16_t dst_addr) const {
	// propagate errors
	if (m_dev_type == DeviceType::ERROR
		|| m_self_addr == ADDR_ERROR
		|| m_self_addr == ADDR_NONE
		|| m_addr_parent == ADDR_ERROR
		|| dst_addr == m_self_addr) return ADDR_ERROR;
	// coordinators have special behavior
	if (m_dev_type == DeviceType::COORDINATOR) {
		// if there's a first router in the address, send to the first router
		if (dst_addr & 0xF000) {
			// check if the router is in our address space
			return m_check_first_router(dst_addr & 0xF000);
		}
		// else send to an end device
		return m_check_end_device(dst_addr);
	}
	// if we're a router and the destination is in our address space, send to the right child
	else if (m_dev_type == DeviceType::FIRST_ROUTER && (dst_addr & 0xF000) == m_self_addr) {
		// if there is a second router in the address, send to the second router
		if (dst_addr & 0x0F00) return m_check_second_router(dst_addr & 0xFF00);
		// else we must be sending to an end device
		return m_check_end_device(dst_addr);
	}
	else if (m_dev_type == DeviceType::SECOND_ROUTER && (dst_addr & 0xFF00) == m_self_addr)
		return m_check_end_device(dst_addr);
	// else route upwards, since either we're an end device or we don't have the destination in our children
	else return m_addr_parent;
}

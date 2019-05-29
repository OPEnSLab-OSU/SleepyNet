#pragma once
#include <cstdint>
#include "LoomNetworkUtility.h"

/**
 * Class containing information about a node's position in a Loom Network
 * This class is meant to be generated once.
 */

namespace LoomNet {
	class Router {
	public:
		Router(const DeviceType dev_type, const uint16_t self_addr, const uint16_t addr_parent)
			: m_dev_type(dev_type)
			, m_self_addr(self_addr)
			, m_addr_parent(addr_parent) {}

		DeviceType get_device_type() const { return m_dev_type; }
		uint16_t get_self_addr() const { return m_self_addr; }
		uint16_t get_addr_parent() const { return m_addr_parent; }

		// NOTE: it is presumed this function will never route to itself
		uint16_t route(const uint16_t dst_addr) {
			// if there's no routing table, this is a broken packet
			if (!(dst_addr & 0xFF00)) return ADDR_NONE;
			// coordinators have special behavior
			if (m_dev_type == DeviceType::COORDINATOR) {
				// if there's a first router in the address, send to the first router
				if (dst_addr & 0xF000) return dst_addr & 0xF000;
				// else send to an end device
				return dst_addr;
			}
			// if we're a router and the destination is in our address space, send to the right child
			// we check address space with an XOR against the first 8 bits of the address
			else if (m_dev_type == DeviceType::FIRST_ROUTER && ((m_self_addr ^ dst_addr) & 0xF000) == 0) {
				// if there is a second router in the address, send to the second router
				if (dst_addr & 0x0F00) return dst_addr & 0xFF00;
				// else we must be sending to an end device
				return dst_addr;
			}
			else if (m_dev_type == DeviceType::SECOND_ROUTER && ((m_self_addr ^ dst_addr) & 0xFF00) == 0)
				return dst_addr;
			// else route upwards, since either we're an end device or we don't have the destination in our children
			else return m_addr_parent;
		}
	private:

		bool m_is_child(const uint16_t addr) const {
			const uint16_t self_route = m_self_addr >> 8;
			const uint16_t addr_route = addr >> 8;
			const uint16_t is_eq = self_route ^ addr_route;
			// check if the first router matches and the second in our own address is zero
			return (is_eq == 0) || (is_eq == (addr_route & 0x0F));
		}
		// store our device type
		const DeviceType m_dev_type;
		// store our own address
		const uint16_t m_self_addr;
		// store the address of our parent
		const uint16_t m_addr_parent;
	};
}

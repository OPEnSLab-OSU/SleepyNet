#pragma once
#include <stdint.h>
#include "LoomNetworkUtility.h"

/**
 * Class containing information about a node's position in a Loom Network
 * This class is meant to be generated once.
 */

namespace LoomNet {
	class Router {
	public:
		Router(const DeviceType dev_type, const uint16_t self_addr, const uint16_t addr_parent, const uint8_t router_count, const uint8_t node_count);

		DeviceType get_device_type() const { return m_dev_type; }
		uint16_t get_self_addr() const { return m_self_addr; }
		uint16_t get_addr_parent() const { return m_addr_parent; }
		uint8_t get_router_count() const { return m_router_child_count; }
		uint8_t get_node_count() const { return m_node_child_count; }

		bool operator==(const Router& rhs) const {
			return (rhs.get_addr_parent() == get_addr_parent())
				&& (rhs.get_device_type() == get_device_type())
				&& (rhs.get_self_addr() == get_self_addr())
				&& (rhs.get_router_count() == get_router_count())
				&& (rhs.get_node_count() == get_node_count());
		}

		// NOTE: it is presumed this function will never route to itself, and does not check if the address returned
		// it actually present in the network
		uint16_t route(const uint16_t dst_addr) const;
	private:
		/** Verify that an address is actually corresponding to a device */
		uint16_t m_check_first_router(const uint16_t addr) const {
			return (addr >> 12) <= m_router_child_count ? addr : ADDR_ERROR;
		}

		uint16_t m_check_second_router(const uint16_t addr) const {
			return ((addr & 0x0F00) >> 8) <= m_router_child_count ? addr : ADDR_ERROR;
		}

		uint16_t m_check_end_device(const uint16_t addr) const {
			return (addr & 0x00FF) <= m_node_child_count ? addr : ADDR_ERROR;
		}

		// store our device type
		const DeviceType m_dev_type;
		// store our own address
		const uint16_t m_self_addr;
		// store the address of our parent
		const uint16_t m_addr_parent;
		// store the number of router children and node children, so we can validate an address
		const uint8_t m_node_child_count;
		const uint8_t m_router_child_count;
	};

	const Router ROUTER_ERROR = Router(DeviceType::ERROR, ADDR_ERROR, ADDR_ERROR, 0, 0);
}

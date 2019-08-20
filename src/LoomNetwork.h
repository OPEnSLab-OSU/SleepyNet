#pragma once

#include "CircularBuffer.h"
#include "LoomNetworkFragment.h"
#include "LoomNetworkUtility.h"
#include "LoomMAC.h"
#include "LoomNetworkInfo.h"

/**
 * Loom Network Layer
 * Operates asynchronously
 */

namespace LoomNet {
	template<size_t send_buffer = 16U, size_t recv_buffer = 16U>
	class Network {
	public:
		enum Status : uint8_t {
			NET_SLEEP_RDY = 1,
			NET_CLOSED = (1 << 1),
			NET_RECV_RDY = (1 << 4),
			NET_SEND_RDY = (1 << 5)
		};

		enum class Error {
			NET_OK,
			RECV_BUF_FULL,
			SEND_BUF_FULL,
			MAC_FAIL,
			INVAL_MAC_STATE,
			ROUTE_FAIL,
		};

		Network(const NetworkInfo& config, const NetworkSim & network)
			: m_mac(	config.route_info.get_self_addr(), 
						config.route_info.get_device_type(),
						config.slot_info, 
						network)
			, m_router(config.route_info)
			, m_addr(config.route_info.get_self_addr())
			, m_buffer_send()
			, m_buffer_recv()
			, m_last_error(Error::NET_OK)
			, m_status(Status::NET_SEND_RDY) {}

		bool operator==(const Network& rhs) const {
			return (rhs.m_mac == m_mac)
				&& (rhs.m_router == m_router)
				&& (rhs.m_addr == m_addr);
		}

		TimeInterval net_sleep_next_wake_time() const { return m_mac.sleep_next_wake_time(); }
		
		void net_sleep_wake_ack() {
			// if there's an error in the machine, we have to wait for it to be cleared
			if (m_last_error != Error::NET_OK) return;
			// wake the MAC layer up, and get its state
			m_mac.sleep_wake_ack();
			MAC::State mac_status = m_mac.get_status();
			// set the sleep and wake bit
			m_update_state(mac_status);
		}

		uint8_t net_update() {
			// if there's an error in the machine, we have to wait for it to be cleared
			if (m_last_error != Error::NET_OK) return m_status;
			const MAC::State mac_status = m_mac.get_status();
			// loop until the MAC layer is ready to sleep, or we're wating on the network
			// update our status with the status from the MAC layer
			// if the mac is ready for data, check our circular buffers!
			if (mac_status == MAC::State::MAC_DATA_WAIT) m_mac.check_for_data();
			// if we're waiting for a refresh, update the MAC layer
			else if (mac_status == MAC::State::MAC_REFRESH_WAIT) m_mac.check_for_refresh();
			// else we have to do something to update the layer
			else if (mac_status == MAC::State::MAC_DATA_SEND_RDY) {
				if (m_buffer_send.size() > 0) {
					// send the first packet corresponding to the address indicated by the MAC layer
					uint16_t addr = m_mac.get_cur_send_address();
					auto iter = m_buffer_send.crange().begin();
					auto end = m_buffer_send.crange().end();
					for (; iter != end; ++iter) {
						if ((*iter).get_next_hop() == addr) break;
					}
					// if there isn't any, send none and move on
					if (!(iter != end)) m_mac.send_pass();
					else {
						// send, and if send succeded, destroy the item
						if (m_mac.send_fragment(*iter)) {
							m_buffer_send.remove(iter);
							// hey there's a new spot!
							m_status |= Status::NET_SEND_RDY;
						}
						// else we will try again later
						// the MAC layer will automatically trigger a refresh if 
						// sending fails consecutivly, so we don't need to do that here
						// TODO: Refresh after consecutive failures
					}
				}
				// else tell the mac layer we got nothing
				else m_mac.send_pass();
			}
			// if the MAC layer failed to send, add the packet back to the buffer for later
			else if (mac_status == MAC::State::MAC_DATA_SEND_FAIL) {
				// TODO: figure out some better way of handling errors
				if (!m_buffer_send.emplace_back(m_mac.get_staged_packet()))
					return m_halt_error(Error::SEND_BUF_FULL);
			}
			// if the mac has data ready to be copied, do that
			else if (mac_status == MAC::State::MAC_DATA_RECV_RDY) {
				// create a lookahead copy of the fragment
				DataFragment recv_frag = m_mac.get_staged_packet();
				// if we have a fragment that is addressed to us, add it to the recv buffer
				if (recv_frag.get_dst() == m_addr) {
					if (!m_buffer_recv.emplace_back(recv_frag))
						return m_halt_error(Error::RECV_BUF_FULL);
					// flip the recv ready bit
					m_status |= Status::NET_RECV_RDY;
				}
				// else the packet needs to be routed
				else {
					const uint16_t nexthop = m_router.route(recv_frag.get_dst());
					if (nexthop == ADDR_ERROR || nexthop == ADDR_NONE)
						return m_halt_error(Error::ROUTE_FAIL);
					// set the next hop address and src
					recv_frag.set_src(m_router.get_self_addr());
					recv_frag.set_next_hop(nexthop);
					// push the packet to the send buffer, tagging it with the next hop address
					if (!m_buffer_send.emplace_back(recv_frag))
						return m_halt_error(Error::SEND_BUF_FULL);
				}
			}
			// throw an error if the MAC state is out of bounds
			else if (mac_status != MAC::State::MAC_SLEEP_RDY) 
				return m_halt_error(Error::INVAL_MAC_STATE);
			// update and return status
			m_update_state(mac_status);
			return m_status;
		}

		void app_send(const DataFragment& send) {
			// push the send fragment into the buffer
			m_buffer_send.emplace_back(send);
			// if the send buffer is full, disallow further sending
			if (m_buffer_send.size() - 1 == m_buffer_send.allocated()) m_status &= ~Status::NET_SEND_RDY;
		}

		void app_send(const uint16_t dst_addr, const uint8_t seq, const uint8_t* raw_payload, const uint8_t length) {
			// push the send fragment into the buffer
			m_buffer_send.emplace_back(
				dst_addr,
				m_addr,
				m_addr,
				seq,
				raw_payload,
				length,
				m_router.route(dst_addr)
			);
			// if the send buffer is full, disallow further sending
			if (m_buffer_send.full()) m_status &= ~Status::NET_SEND_RDY;
		}

		DataFragment app_recv() {
			// create a copy of the last recieved object
			const DataFragment frag(m_buffer_recv.front());
			// destroy the stored object
			m_buffer_recv.destroy_front();
			// if the buffer is emptey, tell the user that there's no more data
			if (m_buffer_recv.empty()) m_status &= ~Status::NET_RECV_RDY;
			// return the copy
			return frag;
		}

		void reset() {
			// reset MAC layer
			m_mac.reset();
			// clear buffers
			m_buffer_recv.reset();
			m_buffer_send.reset();
			// reset state and error
			m_last_error = Error::NET_OK;
			m_status = Status::NET_SEND_RDY;
		}

		Error get_last_error() const { return m_last_error; }
		uint8_t get_status() const { return m_status; }
		const Router& get_router() const { return m_router; }
		const MAC& get_mac() const { return m_mac; }

	private:
		uint8_t m_halt_error(Error error) {
			m_last_error = error;
			// teardown here?
			return m_status = Status::NET_CLOSED;
		}

		void m_update_state(const MAC::State mac_status) {
			// update the state and set the sleep and wake bit
			if (mac_status == MAC::State::MAC_SLEEP_RDY)
				m_status |= Status::NET_SLEEP_RDY;
			else 
				m_status &= ~Status::NET_SLEEP_RDY;
		}

		MAC m_mac;
		Router m_router;

		const uint16_t m_addr;
		// TODO: Implement in terms of a binary tree
		CircularBuffer<DataFragment, send_buffer> m_buffer_send;
		CircularBuffer<DataFragment, recv_buffer> m_buffer_recv;

		Error m_last_error;
		uint8_t m_status;
	};
}


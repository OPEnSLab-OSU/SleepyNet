#pragma once

#include "ArduinoJson.h"
#include "CircularBuffer.h"
#include "LoomNetworkFragment.h"
#include "LoomNetworkUtility.h"
#include "LoomMAC.h"
#include "LoomRouter.h"

/**
 * Loom Network Layer
 * Operates asynchronously
 */

namespace LoomNet {
	template<size_t send_buffer = 16, size_t recv_buffer = 16>
	class Network {
	public:
		enum Status : uint8_t {
			NET_SLEEP_RDY = 1,
			NET_WAIT_REFRESH = (1 << 1),
			NET_CLOSED = (1 << 2),
			NET_RECV_RDY = (1 << 5),
			NET_SEND_RDY = (1 << 6),
		};

		enum class Error {
			NET_OK,
			RECV_BUF_FULL,
			SEND_BUF_FULL,
			MAC_FAIL,
			INVAL_MAC_STATE,
			ROUTE_FAIL,
		};

		Network(const Router& route_info);

		TimeInverval net_sleep_next_wake_time() const { return m_mac.sleep_next_wake_time(); }
		void net_sleep_wake_ack();
		void net_wait_ack();
		void app_send(const Fragment& send);
		void app_send(const uint16_t dst_addr, const uint8_t seq, const uint8_t* raw_payload, const uint8_t length);
		Fragment app_recv();

		void reset();
		Error get_last_error() const { return m_last_error; }
	private:
		void m_halt_error(Error error) {
			m_last_error = error;
			// teardown here?
			m_status = Status::NET_CLOSED;
		}

		MAC m_mac;
		Router m_router;

		const uint16_t m_addr;
		//TODO: Implement in terms of a binary tree
		CircularBuffer<Fragment, send_buffer> m_buffer_send;
		CircularBuffer<Fragment, recv_buffer> m_buffer_recv;

		Error m_last_error;
		Status m_status;
	};
}


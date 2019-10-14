#pragma once
#include <stdint.h>
#include <limits.h>
#include "LoomNetworkPacket.h"
#include "LoomNetworkUtility.h"
#include "LoomSlotter.h"
#include "LoomRadio.h"
#include "LoomNetworkInfo.h"
#include "LoomNetworkTime.h"

/** 
 * Loom Medium Access Control 
 * Operates synchronously
 */

/** This class will serve entirely for simulation for now */
namespace LoomNet {
	class MAC {
	public:

		enum class State {
			MAC_SLEEP_RDY,
			MAC_DATA_SEND_RDY,
			MAC_DATA_WAIT,
			MAC_DATA_RECV_RDY,
			MAC_DATA_SEND_FAIL,
			MAC_REFRESH_WAIT,
			MAC_CLOSED
		};

		enum class SendType {
			MAC_ACK_WITH_DATA,
			MAC_ACK_NO_DATA,
			MAC_DATA,
			NONE
		};

		enum class Error {
			MAC_OK,
			SEND_FAILED,
			WRONG_PACKET_TYPE,
			REFRESH_TIMEOUT,
			REFRESH_PACKET_ERR,
			INVALID_CONFIG,
		};

		MAC(	const uint16_t self_addr, 
				const DeviceType self_type, 
				const Slotter& slot,
				const Drift& timing,
				Radio& radio);

		bool operator==(const MAC& rhs) const {
			return (rhs.m_slot == m_slot)
				&& (rhs.m_self_addr == m_self_addr)
				&& (rhs.m_self_type == m_self_type);
		}

		State get_status() const { return m_state; }
		Error get_last_error() const { return m_last_error; }
		const Slotter& get_slotter() const { return m_slot; }
		const Drift& get_drift() const { return m_timings; }
		uint16_t get_cur_send_address() const { return m_cur_send_addr; }

		void reset();
		void sleep_wake_ack();
		TimeInterval sleep_next_wake_time() const;
		// make sure the address is correct!
		bool send_fragment(const Packet& frag);
		Packet get_staged_packet();
		void send_pass();
		State check_for_data();
		void data_pass();
		void check_for_refresh();
	private:

		void m_send_ack() {
			// send a regular ACK packet
			m_radio.send(ACKPacket::Factory(m_self_addr));
		}

		void m_halt_error(const Error error);

		Slotter m_slot;
		State m_state;
		SendType m_send_type;
		Error m_last_error;
		uint16_t m_cur_send_addr;
		TimeInterval m_time_wake_start;
		Packet m_staging;
		bool m_staged;
		TimeInterval m_next_refresh;
		TimeInterval m_next_data;
		uint8_t m_fail_count;
		Radio& m_radio;
		const uint16_t m_self_addr;
		const DeviceType m_self_type;
		const Drift m_timings;
		// TODO: remove
		int m_check_count = 0;
	};
}

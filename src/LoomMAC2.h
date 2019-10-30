#pragma once
#include <stdint.h>
#include "LoomSlotter.h"
#include "LoomNetworkConfig.h"
#include "LoomRadio.h"

/** 
 * Loom Medium Access Control V2
 * An attempt to abstract the state machine component from the actual functionality of LoomMAC
 * to allow for simpler testing
 */

namespace LoomNet {

	class MACState {
	public:
		enum class State {
			MAC_SEND_REFRESH,
			MAC_WAIT_REFRESH,
			MAC_SLEEP_RDY,
			MAC_SEND_RDY,
			MAC_RECV_RDY,
			MAC_CLOSED
		};

		enum class SendStatus {
			MAC_SEND_SUCCESSFUL,
			MAC_SEND_NO_ACK,
			MAC_SEND_WRONG_RESPONSE,
			NONE,
		};

		enum class Error {
			MAC_INVALID_SLOTTER_STATE,
			MAC_INVALID_SEND_PACKET,
		};

		MACState(const NetworkConfig& config);
		
		State get_state() const { return m_state; }
		SendStatus get_last_sent_status() const { return m_last_send_status; }
		PacketCtrl get_cur_packet_type() const { return m_cur_packet_type; }
		uint16_t get_cur_addr() const;
		TimeInterval wake_next_time() const;
		TimeInterval get_refresh_period() const { return m_slot_length * m_slot.get_slots_per_refresh() - m_max_drift; }
		TimeInterval get_data_period() const { return m_slot_length * REFRESH_CYCLE_SLOTS - m_max_drift; }
		
		void send_event(const PacketCtrl type);
		void recv_event(const PacketCtrl type);
		void refresh_event(const TimeInterval& recv_time, const TimeInterval& data, const TimeInterval& refresh);
		void wake_event();

		void begin();

		void reset();

	private:

		void m_halt_error(const State expected_state);
		void m_halt_error(const Error error);

		State m_state;
		SendStatus m_last_send_status;
		PacketCtrl m_cur_packet_type;

		Slotter m_slot;
		TimeInterval m_data_wait;
		TimeInterval m_refresh_wait;
		uint16_t m_slots_since_refresh;

		const DeviceType m_self_type;
		const uint16_t m_self_addr;
		const uint8_t m_child_router_count;
		const TimeInterval m_slot_length;
		const TimeInterval m_min_drift;
		const TimeInterval m_max_drift;
	};

}
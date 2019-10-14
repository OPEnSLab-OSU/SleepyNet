#pragma once
#include <stdint.h>
#include "LoomNetworkUtility.h"

/**
 * Class containg some useful information and calculations for time slot information
 * meant to be generated once
 */

namespace LoomNet {
	
	class Slotter {
	public:
		enum class State {
			SLOT_SEND,
			SLOT_SEND_W_SYNC,
			SLOT_RECV,
			SLOT_RECV_W_SYNC,
			SLOT_WAIT_REFRESH,
			SLOT_ERROR
		};

		Slotter(	const uint8_t		send_slot, 
					const uint8_t		total_slots,
					const uint8_t		cycles_per_refresh,
					const uint8_t		cycle_gap,
					const uint8_t		batch_gap,
					const uint8_t		send_count,
					const uint8_t		recv_slot, 
					const uint8_t		recv_count);
		
		Slotter(const uint8_t send_slot, const uint8_t total_slots, const uint8_t cycles_per_refresh, const uint8_t cycle_gap, const uint8_t batch_gap)
			: Slotter(send_slot, total_slots, cycles_per_refresh, cycle_gap, batch_gap, 1, SLOT_NONE, 0) {}

		bool operator==(const Slotter& rhs) const {
			return (rhs.m_send_slot == m_send_slot)
				&& (rhs.m_send_count == m_send_count)
				&& (rhs.m_recv_slot == m_recv_slot)
				&& (rhs.m_recv_count == m_recv_count)
				&& (rhs.m_total_slots == m_total_slots)
				&& (rhs.m_cycle_gap == m_cycle_gap)
				&& (rhs.m_batch_gap == m_batch_gap)
				&& (rhs.m_cycles_per_refresh == m_cycles_per_refresh);
		}

		State get_state() const { return m_state; }
		State next_state();
		uint8_t get_slot_wait() const;
		uint16_t get_slots_per_refresh() const;
		void reset();

		uint8_t get_send_slot() const { return m_send_slot; }
		uint8_t get_recv_slot() const { return m_recv_slot; }
		uint8_t get_cur_data_cycle() const { return m_cur_cycle; }
		uint8_t get_total_slots() const { return m_state == State::SLOT_ERROR ? SLOT_ERROR : m_total_slots; }

	private:

		const uint8_t m_send_slot;
		const uint8_t m_send_count;
		const uint8_t m_recv_slot;
		const uint8_t m_recv_count;
		const uint8_t m_total_slots;
		const uint8_t m_cycles_per_refresh;
		const uint8_t m_cycle_gap;
		const uint8_t m_batch_gap;
		State m_state;
		uint8_t m_cur_cycle;
		uint8_t m_cur_device;
	};

	const Slotter SLOTTER_ERROR = Slotter(SLOT_ERROR, 0, 0, 0, 0, 0, SLOT_ERROR, 0);
}
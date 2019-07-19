#pragma once
#include <cstdint>
#include "LoomNetworkUtility.h"

/**
 * Class containg some useful information and calculations for time slot information
 * meant to be generated once
 */

namespace LoomNet {
	
	class Slotter {
	public:
		enum class State {
			SLOT_WAIT_PARENT,
			SLOT_WAIT_CHILD,
			SLOT_WAIT_REFRESH,
			SLOT_WAIT_NEXT_DATA,
			SLOT_ERROR
		};

		Slotter(	const uint8_t send_slot, 
					const uint8_t cycles_per_refresh, 
					const uint8_t recv_slot, 
					const uint8_t recv_count)
			: m_send_slot(send_slot)
			, m_recv_slot(recv_slot)
			, m_recv_count(recv_count)
			, m_cycles_per_refresh(cycles_per_refresh)
			, m_state(send_slot != SLOT_ERROR && recv_slot != SLOT_ERROR ? State::SLOT_WAIT_REFRESH : State::SLOT_ERROR)
			, m_cur_cycle(0) {}
		
		Slotter(const uint8_t send_slot, const uint8_t cycles_per_refresh)
			: Slotter(send_slot, cycles_per_refresh, SLOT_NONE, 0) {}

		bool operator==(const Slotter& rhs) const {
			return (rhs.m_send_slot == m_send_slot)
				&& (rhs.m_recv_slot == m_recv_slot);
		}

		uint8_t get_send_slot() const { return m_send_slot; }
		uint8_t get_recv_slot() const { return m_recv_slot; }

		State get_state() const { return m_state; }
		State next_state() {
			// if we were waiting for a refresh, we wait for children next
			// if we don't have any children then parent
			if (m_state == State::SLOT_ERROR) return m_state;
			if (m_state == State::SLOT_WAIT_REFRESH) {
				if (m_recv_slot == 0) return m_state = State::SLOT_WAIT_PARENT;
				else return m_state = State::SLOT_WAIT_CHILD;
			}
			// else if we were waiting for a parent, start waiting for the child
			else if (m_state == State::SLOT_WAIT_PARENT) return m_state = State::SLOT_WAIT_CHILD;
			// else we were waiting on a child, so on to the next cycle!
			else {
				if (++m_cur_cycle == m_cycles_per_refresh) {
					m_cur_cycle = 0;
					return m_state = State::SLOT_WAIT_REFRESH;
				}
				else return m_state = State::SLOT_WAIT_NEXT_DATA;
			}
		}

		uint8_t get_slot_wait() const {
			if (m_state == State::SLOT_WAIT_PARENT) return get_send_slot();
			if (m_state == State::SLOT_WAIT_CHILD) return get_recv_slot();
			// else we're waiting for a time interval, so we have no slot data
			else return 0;
		}

		void reset() {
			m_state = State::SLOT_WAIT_REFRESH;
			m_cur_cycle = 0;
		}

	private:
		const uint8_t m_send_slot;
		const uint8_t m_recv_slot;
		const uint8_t m_recv_count;
		const uint8_t m_cycles_per_refresh;
		State m_state;
		uint8_t m_cur_cycle;
	};

	const Slotter SLOTTER_ERROR = Slotter(SLOT_ERROR, 0, SLOT_ERROR, 0);
}
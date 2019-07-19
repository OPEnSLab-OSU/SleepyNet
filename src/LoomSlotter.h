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
			WAIT_PARENT,
			WAIT_CHILD,
			WAIT_REFRESH,
			WAIT_NEXT_DATA,
			ERROR
		};

		Slotter(	const uint8_t send_slot, 
					const uint8_t cycles_per_refresh, 
					const uint8_t recv_slot, 
					const uint8_t recv_count)
			: m_send_slot(send_slot)
			, m_recv_slot(recv_slot)
			, m_recv_count(recv_count)
			, m_cycles_per_refresh(cycles_per_refresh)
			, m_state(send_slot != SLOT_ERROR && recv_slot != SLOT_ERROR ? State::WAIT_REFRESH : State::ERROR)
			, m_cur_cycle(0) {}
		
		Slotter(const uint8_t send_slot, const uint8_t cycles_per_refresh)
			: Slotter(send_slot, cycles_per_refresh, SLOT_NONE, 0) {}

		bool operator==(const Slotter& rhs) const {
			return (rhs.m_send_slot == m_send_slot)
				&& (rhs.m_recv_slot == m_recv_slot)
				&& (rhs.m_cycles_per_refresh == m_cycles_per_refresh);
		}

		uint8_t get_send_slot() const { return m_send_slot; }
		uint8_t get_recv_slot() const { return m_recv_slot; }

		State get_state() const { return m_state; }
		State next_state() {
			// if we were waiting for a refresh, we wait for children next
			// if we don't have any children then parent
			if (m_state == State::ERROR) return m_state;
			if (m_state == State::WAIT_REFRESH) {
				if (m_recv_slot == 0) return m_state = State::WAIT_PARENT;
				else return m_state = State::WAIT_CHILD;
			}
			// else if we were waiting for a parent, start waiting for the child
			else if (m_state == State::WAIT_PARENT) return m_state = State::WAIT_CHILD;
			// else we were waiting on a child, so on to the next cycle!
			else {
				if (++m_cur_cycle == m_cycles_per_refresh) {
					m_cur_cycle = 0;
					return m_state = State::WAIT_REFRESH;
				}
				else return m_state = State::WAIT_NEXT_DATA;
			}
		}

		uint8_t get_slot_wait() const {
			if (m_state == State::WAIT_PARENT) return get_send_slot();
			if (m_state == State::WAIT_CHILD) return get_recv_slot();
			// else we're waiting for a time interval, so we have no slot data
			else return 0;
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
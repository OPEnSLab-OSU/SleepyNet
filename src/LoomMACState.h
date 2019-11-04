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

	template<class SlotType = LoomNet::Slotter>
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
		uint16_t get_cur_send_addr() const { return m_cur_send_addr; }
		TimeInterval get_refresh_period() const { return m_slot_length * m_slot.get_slots_per_refresh(); }
		TimeInterval get_data_period() const { return m_slot_length * REFRESH_CYCLE_SLOTS; }
		TimeInterval get_recv_timeout() const { return m_min_drift; }
		TimeInterval wake_next_time() const;

		DeviceType get_device_type() const { return m_self_type; }
		SlotType& get_slotter() { return m_slot; }
		const SlotType& get_slotter() const { return m_slot; }

		void begin();
		void send_event(const PacketCtrl type);
		void recv_event(const PacketCtrl type, const uint16_t addr);
		void recv_fail();
		void refresh_event(const TimeInterval& recv_time, const TimeInterval& data, const TimeInterval& refresh);
		void wake_event();

	private:

		void m_halt_error(const State expected_state);
		void m_halt_error(const Error error);

		State m_state;
		SendStatus m_last_send_status;
		PacketCtrl m_cur_packet_type;
		uint16_t m_cur_send_addr;

		SlotType m_slot;
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

	template <class T>
	MACState<T>::MACState(const NetworkConfig& config)
		: m_state(State::MAC_CLOSED)
		, m_last_send_status(SendStatus::NONE)
		, m_cur_packet_type(PacketCtrl::NONE)
		, m_cur_send_addr(ADDR_NONE)
		, m_slot(config)
		, m_data_wait(TIME_NONE)
		, m_refresh_wait(TIME_NONE)
		, m_slots_since_refresh(0)
		, m_self_type(get_type(config.self_addr))
		, m_self_addr(config.self_addr)
		, m_child_router_count(config.child_router_count)
		, m_slot_length(config.slot_length)
		, m_min_drift(config.min_drift)
		, m_max_drift(config.max_drift) {}

	template <class T>
	TimeInterval MACState<T>::wake_next_time() const {
		const Slotter::State state = m_slot.get_state();
		// we correct for drift by moving the recieving device backwards
		// and leaving the send device in the normal time
		// the maximum drift is used to correct the refresh cycle,
		// and the minimum drift is used to correct each indivdual slot
		// waiting for a refresh
		if (state == Slotter::State::SLOT_WAIT_REFRESH) {
			if (m_self_type == DeviceType::COORDINATOR)
				return m_refresh_wait;
			else
				return m_refresh_wait - m_max_drift;
		}
		// else sending/reciving
		TimeInterval next_wake(TIME_NONE);
		if (state == Slotter::State::SLOT_RECV_W_SYNC || state == Slotter::State::SLOT_SEND_W_SYNC)
			next_wake = m_data_wait;
		else
			next_wake = m_slot_length * (m_slots_since_refresh + m_slot.get_slot_wait());
		// only correct for drift if recieving
		if (state == Slotter::State::SLOT_RECV || state == Slotter::State::SLOT_RECV_W_SYNC)
			return next_wake - m_min_drift;
		else
			return next_wake;
	}

	template <class T>
	void MACState<T>::begin() {
		m_last_send_status = SendStatus::NONE;
		m_slot.reset();
		m_slots_since_refresh = 0;
		m_cur_packet_type = PacketCtrl::REFRESH_INITIAL;
		m_cur_send_addr = ADDR_COORD;

		if (m_self_type == DeviceType::COORDINATOR) {
			// tell the network to send a refresh packet
			m_state = State::MAC_SEND_REFRESH;
			m_cur_send_addr = ADDR_NONE;
		}
		else
			m_state = State::MAC_WAIT_REFRESH;
	}

	// an end device or router has recieved a refresh packet, or a coordinator has sent the refresh packet, so the network starts now!
	template <class T>
	void MACState<T>::refresh_event(const TimeInterval& timestamp, const TimeInterval& data, const TimeInterval& refresh) {
		if (m_state != State::MAC_WAIT_REFRESH && m_state != State::MAC_SEND_REFRESH)
			m_halt_error(State::MAC_WAIT_REFRESH);
		// set the network
		m_data_wait = timestamp + data;
		m_refresh_wait = timestamp + refresh;
		m_slots_since_refresh = 0;
		// advance the slotter
		m_slot.next_state();
		// go to sleep
		m_state = State::MAC_SLEEP_RDY;
	}

	template <class T>
	void MACState<T>::wake_event() {
		using SlotState = Slotter::State;
		const SlotState cur_state = m_slot.get_state();
		// increment the slot wait counter, if the wait was not synchronized
		if (cur_state == SlotState::SLOT_SEND || cur_state == SlotState::SLOT_RECV)
			m_slots_since_refresh += m_slot.get_slot_wait();
		// if refresh, begin the MAC anew
		if (cur_state == SlotState::SLOT_WAIT_REFRESH)
			begin();
		// else we must be transmitting or recieving data
		else {
			if (cur_state == SlotState::SLOT_SEND || cur_state == SlotState::SLOT_SEND_W_SYNC) {
				m_state = State::MAC_SEND_RDY;
				m_cur_send_addr = get_parent(m_self_addr, m_self_type);
			}
			else if (cur_state == SlotState::SLOT_RECV || cur_state == SlotState::SLOT_RECV_W_SYNC)
				m_state = State::MAC_RECV_RDY;
			// we don't know who we're going to recieve from until we recieve the packet,
			// so we don't have an address here
			else m_halt_error(Error::MAC_INVALID_SLOTTER_STATE);
			// data time!
			m_cur_packet_type = PacketCtrl::DATA_TRANS;
		}
		// move the slotter forward
		m_slot.next_state();
	}

	template <class T>
	void MACState<T>::send_event(const PacketCtrl type) {
		if (m_state != State::MAC_SEND_RDY)
			return m_halt_error(State::MAC_SEND_RDY);
		// based on the current packet type, wait for a new packet type or sleep
		if (type == PacketCtrl::DATA_TRANS || type == PacketCtrl::DATA_ACK_W_DATA) {
			if (m_cur_packet_type != PacketCtrl::DATA_ACK || m_cur_packet_type != PacketCtrl::DATA_ACK_W_DATA)
				m_halt_error(Error::MAC_INVALID_SEND_PACKET);
			// ready to recieve an acknowledgement!
			m_state = State::MAC_RECV_RDY;
			if (type == PacketCtrl::DATA_TRANS)
				m_cur_packet_type = PacketCtrl::DATA_ACK_W_DATA;
			else
				m_cur_packet_type = PacketCtrl::DATA_ACK;
		}
		// else the packet must be an ACK, so go to sleep
		else {
			if (m_cur_packet_type != PacketCtrl::DATA_ACK_W_DATA && m_cur_packet_type != PacketCtrl::DATA_ACK)
				m_halt_error(Error::MAC_INVALID_SEND_PACKET);
			m_state = State::MAC_SLEEP_RDY;
		}
	}

	template <class T>
	void MACState<T>::recv_event(const PacketCtrl type, const uint16_t addr) {
		if (m_state != State::MAC_RECV_RDY)
			return m_halt_error(State::MAC_RECV_RDY);
		// check for an invalid packet type
		// if the packet type is not what we expected, BUT we allow an ACK when we expect a DATA_ACK
		if (type != m_cur_packet_type
			&& (m_cur_packet_type != PacketCtrl::DATA_ACK_W_DATA && type != PacketCtrl::DATA_ACK)) {
			// we were waiting for something but got something else
			m_last_send_status = SendStatus::MAC_SEND_WRONG_RESPONSE;
			m_state = State::MAC_SLEEP_RDY;
		}
		// packet type is valid!
		// based on the packet type recieved, send a new packet type or sleep
		else if (type == PacketCtrl::DATA_TRANS || type == PacketCtrl::DATA_ACK_W_DATA) {
			// send an ACK packet, but choose type based on the type
			if (type == PacketCtrl::DATA_TRANS)
				m_cur_packet_type = PacketCtrl::DATA_ACK_W_DATA;
			else {
				// we got an ACK as well, so send was good!
				m_last_send_status = SendStatus::MAC_SEND_SUCCESSFUL;
				m_cur_packet_type = PacketCtrl::DATA_ACK;
			}
			m_cur_send_addr = addr;
			m_state = State::MAC_SEND_RDY;
		}
		// else it must be an ACK packet, so the send was good and go to sleep
		else {
			m_last_send_status = SendStatus::MAC_SEND_SUCCESSFUL;
			m_state = State::MAC_SLEEP_RDY;
		}
	}

	template <class T>
	void MACState<T>::recv_fail() {
		// we were waiting for something but got something else
		m_last_send_status = SendStatus::MAC_SEND_NO_ACK;
		m_state = State::MAC_SLEEP_RDY;
	}
}

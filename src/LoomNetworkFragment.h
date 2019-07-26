#pragma once

#include <cstdint>
#include "LoomNetworkUtility.h"
/**
 * Data types for Loom Network Packets
 */
namespace LoomNet {
	PacketCtrl get_packet_control(const std::array<uint8_t, 255> & packet) {
		return static_cast<PacketCtrl>(packet[0]);
	}

	uint16_t calc_framecheck_global(const std::array<uint8_t, 255> & packet, const uint8_t framecheck_index) {
		return 0xBEEF;
	}

	uint16_t calc_framecheck_global(const uint8_t* packet, const uint8_t framecheck_index) {
		return 0xBEEF;
	}

	bool check_packet(const std::array<uint8_t, 255> & packet, const uint16_t expected_src) {
		const PacketCtrl ctrl = get_packet_control(packet);
		// if it's a small ACK, just check the src field
		if (ctrl == PacketCtrl::DATA_ACK)
			return (static_cast<uint16_t>(packet[1]) | static_cast<uint16_t>(packet[2]) << 8) == expected_src;
		// else if it's a refresh check the framecheck with a fixed offset
		else if (ctrl == PacketCtrl::REFRESH_INITIAL) {
			return calc_framecheck_global(packet, 9) 
				== (static_cast<uint16_t>(packet[9]) | static_cast<uint16_t>(packet[10]) << 8);
		}
		// else check the framecheck based off of an encoded offset
		else if (ctrl == PacketCtrl::DATA_ACK_W_DATA
			|| ctrl == PacketCtrl::DATA_TRANS
			|| ctrl == PacketCtrl::REFRESH_ADDITONAL) {
			const uint8_t endpos = packet[3] + 3;
			return calc_framecheck_global(packet, endpos)
				== (static_cast<uint16_t>(packet[endpos]) | static_cast<uint16_t>(packet[endpos + 1]) << 8);
		}
		// else the packet control is invalid, oops
		return false;
	}

	class Fragment {
	public:
		explicit Fragment(const PacketCtrl control, const uint16_t src_addr)
			: m_payload{} {
			// set the control
			m_payload[0] = control;
			// set the source address
			m_payload[1] = static_cast<uint8_t>(src_addr & 0xff);
			m_payload[2] = static_cast<uint8_t>(src_addr >> 8);
		}

		explicit Fragment(const uint8_t* raw_packet, const uint8_t max_length)
			: m_payload{} {
			// check lengths are correct
			if (max_length > sizeof(m_payload)) set_error();
			// copy the bytes over
			else {
				for (auto i = 0; i < max_length; i++) m_payload[i] = raw_packet[i];
			}
		}

		// get the buffer
		uint8_t operator[](const uint8_t index) const { m_payload[index]; }
		const uint8_t* get_raw() const { return m_payload; }
		// get the section that we're allowed to write to
		uint8_t* get_write_start() { return &(m_payload[3]); }
		const uint8_t* get_write_start() const { return &(m_payload[3]); }
		// get the length we're allowed to write
		constexpr uint8_t get_write_count() { return 250; }

		// get the control
		PacketCtrl get_control() const { return static_cast<PacketCtrl>(m_payload[0]); }
		void set_control(const PacketCtrl ctrl) { m_payload[0] = ctrl; }
		// set the control to an error if something invalid happens down the chain
		void set_error() { m_payload[0] = PacketCtrl::ERROR; }
		void set_src(const uint16_t src_addr) { 
			m_payload[1] = static_cast<uint8_t>(src_addr & 0xff);
			m_payload[2] = static_cast<uint8_t>(src_addr >> 8);
		}
		// calculate frame control sequence
		// run this function right before the packet is sent, to ensure it is correct
		void calc_framecheck(const uint8_t endpos) { 
			const uint16_t check = calc_framecheck_global(m_payload, endpos); 
			m_payload[endpos] = check & 0xff;
			m_payload[endpos + 1] = check >> 8;
		}
		// run this function right after the packet has been recieved to verify the packet
		bool check_framecheck(const uint8_t endpos) { 
			return calc_framecheck_global(m_payload, endpos) 
				== (static_cast<uint16_t>(m_payload[endpos]) | static_cast<uint16_t>(m_payload[endpos + 1]) << 8);
		}

	private:
		uint8_t m_payload[255];
	};

	class DataFragment : public Fragment {
	public:
		DataFragment(const uint16_t dst_addr, const uint16_t src_addr, const uint16_t orig_src_addr, const uint8_t seq, const uint8_t* raw_payload, const uint8_t length, const uint16_t next_addr)
			: Fragment(PacketCtrl::DATA_TRANS, src_addr)
			, m_next_hop_addr(next_addr) {
			auto packet = get_write_start();
			auto count = get_write_count();
			const auto end_pos = length + 6;
			// overflow check
			if (end_pos > count) set_error();
			else {
				// copy our data into the payload
				packet[0] = end_pos;
				packet[1] = static_cast<uint8_t>(dst_addr & 0xff);
				packet[2] = static_cast<uint8_t>(dst_addr >> 8);
				packet[3] = static_cast<uint8_t>(orig_src_addr & 0xff);
				packet[4] = static_cast<uint8_t>(orig_src_addr >> 8);
				for (auto i = 0; i < length; i++) packet[i + 5] = raw_payload[i];
				// calculate the FCS
				calc_framecheck(end_pos);
			}
		}

		DataFragment(const uint8_t* raw_packet, const uint8_t max_length)
			: Fragment(raw_packet, max_length)
			, m_next_hop_addr(ADDR_NONE) {}

		uint16_t get_dst() const { return static_cast<uint16_t>(get_write_start()[1]) | static_cast<uint16_t>(get_write_start()[2]) << 8; }
		uint16_t get_orig_src() const { return static_cast<uint16_t>(get_write_start()[3]) | static_cast<uint16_t>(get_write_start()[4]) << 8; }
		uint8_t* get_payload() { return &(get_write_start()[5]); }
		const uint8_t* get_payload() const { return &(get_write_start()[5]); }
		uint8_t get_payload_length() const { const uint8_t num = get_write_start()[5]; return num >= 6 ? num - 6 : 0; }
		uint8_t get_fragment_length() const { return get_payload_length() + 5; }
		void set_next_hop(const uint8_t next_hop) { m_next_hop_addr = next_hop; }
		uint16_t get_next_hop() const { return m_next_hop_addr; }
		void set_is_ack(bool is_ack) { is_ack ? set_control(PacketCtrl::DATA_ACK_W_DATA) : set_control(PacketCtrl::DATA_TRANS); }

	private:
		uint16_t m_next_hop_addr;
	};

	class RefreshFragment : public Fragment {
	public:
		explicit RefreshFragment(const uint16_t src_addr, const TimeInterval data_interval, const TimeInterval refresh_interval, const uint8_t count)
			: Fragment(PacketCtrl::REFRESH_INITIAL, src_addr) {
			auto packet = get_write_start();
			auto pkt_count = get_write_count();
			// overflow check
			if (6 > pkt_count) set_error();
			else {
				// copy our data into the payload
				packet[0] = (data_interval.unit & 0x07) | ((refresh_interval.unit & 0x07) << 3);
				packet[1] = static_cast<uint8_t>(data_interval.time & 0xFF);
				packet[2] = static_cast<uint8_t>(refresh_interval.time & 0xFF);
				packet[3] = static_cast<uint8_t>(refresh_interval.time >> 8);
				packet[4] = 0;
				packet[5] = count;
			}
		}

		TimeInterval get_data_interval() const { return TimeInterval(get_write_start()[0] & static_cast<uint8_t>(0x07), get_write_start()[1]); }
		TimeInterval get_refresh_interval() const { 
			return TimeInterval(
				(get_write_start()[0] >> 3) & static_cast<uint8_t>(0x07),
				static_cast<uint16_t>(get_write_start()[2]) | (static_cast<uint16_t>(get_write_start()[3]) << static_cast<uint16_t>(8))
			);
		}
		uint8_t get_count() const { return get_write_start()[5]; }
	};

	class ACKFragment : public Fragment {
	public:
		explicit ACKFragment(const uint16_t src_addr)
			: Fragment(PacketCtrl::DATA_ACK, src_addr) {}
	};
}

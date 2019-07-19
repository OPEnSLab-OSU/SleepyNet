#pragma once

#include <cstdint>
#include "LoomNetworkUtility.h"
/**
 * Data types for Loom Network Packets
 */
namespace LoomNet {
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
		// set the control to an error if something invalid happens down the chain
		void set_error() { m_payload[0] = PacketCtrl::ERROR; }
		void set_src(const uint16_t src_addr) { 
			m_payload[1] = static_cast<uint8_t>(src_addr & 0xff);
			m_payload[2] = static_cast<uint8_t>(src_addr >> 8);
		}
		// calculate frame control sequence
		// run this function right before the packet is sent, to ensure it is correct
		void calc_framecheck(const uint8_t endpos) { /* TODO: FRAMECHECK */ m_payload[endpos] = 0xBE; m_payload[endpos + 1] = 0xEF; }
		// run this function right after the packet has been recieved to verify the packet
		bool check_framecheck(const uint8_t endpos) { return m_payload[endpos] == 0xBE && m_payload[endpos + 1] == 0xEF; }

	private:
		uint8_t m_payload[255];
	};

	class DataFragment : public Fragment {
	public:
		explicit DataFragment(const uint16_t dst_addr, const uint16_t src_addr, const uint16_t orig_src_addr, const uint8_t seq, const uint8_t* raw_payload, const uint8_t length, const uint16_t next_addr)
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

		uint16_t get_dst() const { return static_cast<uint16_t>(get_write_start()[1]) | static_cast<uint16_t>(get_write_start()[2]) << 8; }
		uint16_t get_orig_src() const { return static_cast<uint16_t>(get_write_start()[3]) | static_cast<uint16_t>(get_write_start()[4]) << 8; }
		uint8_t* get_payload() { return &(get_write_start()[5]); }
		const uint8_t* get_payload() const { return &(get_write_start()[5]); }
		uint8_t get_payload_length() const { const uint8_t num = get_write_start()[5]; return num >= 6 ? num - 6 : 0; }
		uint8_t get_fragment_length() const { return get_payload_length() + 5; }
		void set_next_hop(const uint8_t next_hop) { m_next_hop_addr = next_hop; }
		uint16_t get_next_hop() const { return m_next_hop_addr; }

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
}

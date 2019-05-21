#pragma once

#include <cstdint>
/**
 * Data types for Loom Network Packets
 */
class LoomNetworkFragment {
public:
	static const uint16_t ADDR_NONE = 0;
	
	
	explicit LoomNetworkFragment(const uint16_t dst_addr, const uint16_t src_addr, const uint8_t seq, const uint8_t* raw_payload, const uint8_t length, const uint16_t next_addr)
		: m_dst_addr(dst_addr)
		, m_src_addr(src_addr)
		, m_seq(seq)
		, m_payload{}
		, m_payload_len(length)
		, m_next_hop_addr(next_addr) {
		// copy the payload
		for (auto i = 0; i < length; i++) m_payload[i] = raw_payload[i];
	}

	explicit LoomNetworkFragment(const uint8_t* raw_packet, const uint16_t max_length)
		: LoomNetworkFragment(static_cast<uint16_t>(raw_packet[2] << 8) | raw_packet[1],
			static_cast<uint16_t>(raw_packet[4] << 8) | raw_packet[3],
			raw_packet[5],
			&raw_packet[7],
			raw_packet[0] - 6,
			ADDR_NONE) {}

	explicit LoomNetworkFragment(const LoomNetworkFragment& rp, const uint16_t nexthop)
		: LoomNetworkFragment(rp.m_dst_addr, rp.m_src_addr, rp.m_seq, rp.m_payload, rp.m_payload_len, nexthop) {}

	uint16_t to_raw(uint8_t* buf, const uint16_t max_length) {
		const uint8_t frame_length = m_payload_len + 6;
		if (max_length < frame_length || m_payload_len > 249) return 0;

		buf[0] = frame_length;
		buf[1] = static_cast<uint8_t>(m_dst_addr & 0xff);
		buf[2] = static_cast<uint8_t>(m_dst_addr >> 8);
		buf[3] = static_cast<uint8_t>(m_src_addr & 0xff);
		buf[4] = static_cast<uint8_t>(m_src_addr >> 8);
		for (auto i = 0; i < m_payload_len; i++) buf[i + 5] = m_payload[i];

		return frame_length;
	}

	uint16_t get_dst() const { return m_dst_addr; }
	uint16_t get_src() const { return m_src_addr; }
	uint8_t* get_payload() { return m_payload; }
	const uint8_t* get_payload() const { return m_payload; }
	uint8_t get_payload_length() const { return m_payload_len; }
	uint16_t get_next_hop() const { return m_next_hop_addr; }

private:
	const uint16_t m_dst_addr;
	const uint16_t m_src_addr;
	const uint8_t m_seq;
	uint8_t m_payload[149];
	const uint8_t m_payload_len;
	const uint16_t m_next_hop_addr;
};
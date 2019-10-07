#include "LoomNetworkPacket.h"

using namespace LoomNet;

Packet::Packet(const PacketCtrl control, const uint16_t src_addr)
	: m_payload{} {
	// set the control
	m_payload[Structure::CONTROL] = control;
	// set the source address
	m_payload[Structure::SRC_ADDR] = static_cast<uint8_t>(src_addr & 0xff);
	m_payload[Structure::SRC_ADDR + 1] = static_cast<uint8_t>(src_addr >> 8);
}

Packet::Packet(const uint8_t* raw_packet, const uint8_t max_length)
	: m_payload{} {
	// check lengths are correct
	if (max_length > sizeof(m_payload)) set_error();
	// copy the bytes over
	else {
		for (auto i = 0; i < max_length; i++) m_payload[i] = raw_packet[i];
	}
}

uint16_t Packet::calc_framecheck() const {
	const uint8_t endpos = m_get_fragment_end();
	if (endpos == 0 || get_control() == PacketCtrl::DATA_ACK) return 0;
	return m_framecheck_impl(endpos);
}

void Packet::set_framecheck() {
	if (get_control() == PacketCtrl::DATA_ACK) set_error();
	else {
		const uint8_t endpos = m_get_fragment_end();
		if (endpos == 0) set_error();
		else {
			const uint16_t check = m_framecheck_impl(endpos);
			m_payload[endpos] = static_cast<uint8_t>(check & 0xff);
			m_payload[endpos + 1] = static_cast<uint8_t>(check >> 8);
		}
	}
}

bool Packet::check_packet(const uint16_t expected_src) const {
	if (get_control() != PacketCtrl::DATA_ACK) {
		const uint8_t endpos = m_get_fragment_end();
		if (endpos == 0) return false;
		const uint16_t stored_check = static_cast<uint16_t>(m_payload[endpos]) | static_cast<uint16_t>(m_payload[endpos + 1]) << 8;
		const uint16_t calc_check = m_framecheck_impl(endpos);
		return calc_check != 0 && calc_check == stored_check;
	}
	// only applies to ACK packets
	return get_src() == expected_src;
}

Packet LoomNet::DataPacket::Factory(const uint16_t dst_addr, const uint16_t src_addr, const uint16_t orig_src_addr, const uint8_t id, const uint8_t seq, const uint8_t* raw_payload, const uint8_t length) {

	Packet ret(PacketCtrl::DATA_TRANS, src_addr);
	uint8_t* pkt = &(ret.get_raw()[Packet::PAYLOAD]);

	auto count = ret.get_write_count();
	const auto frag_len = length + Structure::PAYLOAD;
	// overflow check
	if (frag_len > count) ret.set_error();
	else {
		// copy our data into the payload
		pkt[Structure::FRAME_LEN] = frag_len;
		pkt[Structure::DST_ADDR] = static_cast<uint8_t>(dst_addr & 0xff);
		pkt[Structure::DST_ADDR + 1] = static_cast<uint8_t>(dst_addr >> 8);
		pkt[Structure::ORIG_SRC_ADDR] = static_cast<uint8_t>(orig_src_addr & 0xff);
		pkt[Structure::ORIG_SRC_ADDR + 1] = static_cast<uint8_t>(orig_src_addr >> 8);
		pkt[Structure::ROLL_ID] = id;
		pkt[Structure::SEQUENCE] = seq;
		for (auto i = 0; i < length; i++) pkt[i + DataPacket::Structure::PAYLOAD] = raw_payload[i];
	}
	return ret;
}

Packet RefreshPacket::Factory(const uint16_t src_addr,
	TimeInterval data_interval, // must be 8bits long
	TimeInterval refresh_interval, // must be 16bits long
	const uint8_t count) {

	Packet ret(PacketCtrl::REFRESH_INITIAL, src_addr);
	uint8_t* pkt = &(ret.get_raw()[Packet::PAYLOAD]);
	auto pkt_count = ret.get_write_count();
	// overflow check
	if (6 > pkt_count) ret.set_error();
	else {
		// copy our data into the payload
		pkt[Structure::INTERVAL_CTRL] = (data_interval.get_unit() & 0x07) | ((refresh_interval.get_unit() & 0x07) << 3);
		pkt[Structure::DATA_OFF] = static_cast<uint8_t>(data_interval.get_time());
		pkt[Structure::REFRESH_OFF] = static_cast<uint8_t>(refresh_interval.get_time() & 0xFF);
		pkt[Structure::REFRESH_OFF + 1] = static_cast<uint8_t>(refresh_interval.get_time() >> 8);
		pkt[Structure::COUNT] = count;
	}
	return ret;
}

uint8_t Packet::m_get_fragment_end() const {
	const PacketCtrl ctrl = get_control();

	if (ctrl == PacketCtrl::DATA_ACK_W_DATA
		|| ctrl == PacketCtrl::DATA_TRANS) return Structure::PAYLOAD + as<DataPacket>().get_fragment_length();
	if (ctrl == PacketCtrl::REFRESH_INITIAL) return Structure::PAYLOAD + as<RefreshPacket>().get_fragment_length();
	if (ctrl == PacketCtrl::REFRESH_ADDITONAL) /* TODO */ return 0;
	// ACK doesn't have a framecheck
	if (ctrl == PacketCtrl::DATA_ACK) return Structure::PAYLOAD;
	return 0;
}
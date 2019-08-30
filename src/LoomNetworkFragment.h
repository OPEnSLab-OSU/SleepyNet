#pragma once

#include <cstdint>
#include "../simulate/LoomNetworkSimulate/LoomNetworkSimulate/FastCRC.h"
#include "LoomNetworkUtility.h"
/**
 * Data types for Loom Network Packets
 */
namespace LoomNet {
	class Packet {
	public:
		enum Structure {
			CONTROL = 0,
			SRC_ADDR = 1,
			PAYLOAD = 3
		};

		explicit Packet(const PacketCtrl control, const uint16_t src_addr)
			: m_payload{} {
			// set the control
			m_payload[Structure::CONTROL] = control;
			// set the source address
			m_payload[Structure::SRC_ADDR] = static_cast<uint8_t>(src_addr & 0xff);
			m_payload[Structure::SRC_ADDR + 1] = static_cast<uint8_t>(src_addr >> 8);
		}

		explicit Packet(const uint8_t* raw_packet, const uint8_t max_length)
			: m_payload{} {
			// check lengths are correct
			if (max_length > sizeof(m_payload)) set_error();
			// copy the bytes over
			else {
				for (auto i = 0; i < max_length; i++) m_payload[i] = raw_packet[i];
			}
		}

		// get the control
		PacketCtrl get_control() const { return static_cast<PacketCtrl>(m_payload[Structure::CONTROL]); }
		void set_control(const PacketCtrl ctrl) { m_payload[Structure::CONTROL] = ctrl; }
		// set the control to an error if something invalid happens down the chain
		void set_error() { m_payload[0] = PacketCtrl::ERROR; }
		uint16_t get_src() const { return static_cast<uint16_t>(m_payload[Structure::SRC_ADDR]) | static_cast<uint16_t>(m_payload[Structure::SRC_ADDR + 1]) << 8; }
		void set_src(const uint16_t src_addr) {
			m_payload[Structure::SRC_ADDR] = static_cast<uint8_t>(src_addr & 0xff);
			m_payload[Structure::SRC_ADDR + 1] = static_cast<uint8_t>(src_addr >> 8);
		}

		// upcast! Convert this packet to a reference of a specific packet type
		// so we can safely access the data
		template<class PacketType> 
		const typename std::enable_if<std::is_base_of<Packet, PacketType>::value, PacketType>::type& as() const { return *reinterpret_cast<const PacketType*>(this); }
		template<class PacketType>
		typename std::enable_if<std::is_base_of<Packet, PacketType>::value, PacketType>::type& as() { return *reinterpret_cast<PacketType*>(this); }

		// calculate frame control sequence
		// run this function right before the packet is sent, to ensure it is correct
		// calculate frame control sequence
		// run this function right before the packet is sent, to ensure it is correct
		uint16_t calc_framecheck() const {
			const uint8_t endpos = m_get_fragment_end();
			if (endpos == 0) return 0;
			return m_framecheck_impl(endpos);
		}

		void set_framecheck() {
			const uint8_t endpos = m_get_fragment_end();
			if (endpos != 0) {
				const uint16_t check = m_framecheck_impl(endpos);
				m_payload[endpos] = static_cast<uint8_t>(check & 0xff);
				m_payload[endpos + 1] = static_cast<uint8_t>(check >> 8);
			}
			else set_error();
		}
		// run this function right after the packet has been recieved to verify the packet
		// run this function right after the packet has been recieved to verify the packet
		bool check_packet(const uint16_t expected_src = ADDR_NONE) const {
			const uint8_t endpos = m_get_fragment_end();
			if (endpos != 0) {
				const uint16_t stored_check = static_cast<uint16_t>(m_payload[endpos]) | static_cast<uint16_t>(m_payload[endpos + 1]) << 8;
				return m_framecheck_impl(endpos) == stored_check;
			}
			// only applies to ACK packets
			return get_src() == expected_src;
		}
		const uint8_t* get_raw() const { return m_payload; }
		uint8_t get_raw_length() const { return 255; }
		uint8_t get_packet_length() const { return m_get_fragment_end() + 2; }

		// get the buffer
		const uint8_t* payload() const { return &(m_payload[Structure::PAYLOAD]); }
		uint8_t* payload() { return &(m_payload[Structure::PAYLOAD]); }
		// get the length we're allowed to write
		uint8_t get_write_count() const { return 255 - (Structure::PAYLOAD + 2); }

	private:
		uint16_t m_framecheck_impl(const uint8_t frag_end) const { return FastCRC16().xmodem(m_payload, frag_end); }
		uint8_t m_get_fragment_end() const;

		uint8_t m_payload[255];
	};

	class DerivedPacket : public Packet {
		DerivedPacket() = delete;
		DerivedPacket(const DerivedPacket&) = delete;
		DerivedPacket& operator=(const DerivedPacket&) = delete;
	};

	class DataPacket : public DerivedPacket {
	public:
		enum Structure {
			FRAME_LEN = 0,
			DST_ADDR = 1,
			ORIG_SRC_ADDR = 3,
			ROLL_ID = 5,
			SEQUENCE = 6,
			PAYLOAD = 8
		};

		uint16_t get_dst() const { return static_cast<uint16_t>(payload()[Structure::DST_ADDR]) | static_cast<uint16_t>(payload()[Structure::DST_ADDR + 1]) << 8; }
		uint16_t get_orig_src() const { return static_cast<uint16_t>(payload()[Structure::ORIG_SRC_ADDR]) | static_cast<uint16_t>(payload()[Structure::ORIG_SRC_ADDR + 1]) << 8; }
		uint8_t get_rolling_id() const { return payload()[Structure::ROLL_ID]; }
		uint8_t get_seq() const { return payload()[Structure::SEQUENCE]; }
		uint8_t* get_payload() { return &(payload()[Structure::PAYLOAD]); }
		const uint8_t* get_payload() const { return &(payload()[Structure::PAYLOAD]); }
		uint8_t get_payload_length() const { const uint8_t num = payload()[Structure::FRAME_LEN]; return num >= Structure::PAYLOAD ? num - Structure::PAYLOAD : 0; }
		uint8_t get_fragment_length() const { return get_payload_length() + Structure::PAYLOAD; }
		uint8_t get_packet_length() const { return get_fragment_length() + Packet::Structure::PAYLOAD + 2; }

		static Packet Factory(const uint16_t dst_addr,
			const uint16_t src_addr,
			const uint16_t orig_src_addr,
			const uint8_t id,
			const uint8_t seq,
			const uint8_t* raw_payload,
			const uint8_t length) {

			Packet ret(PacketCtrl::DATA_TRANS, src_addr);
			uint8_t* pkt = ret.payload();

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
	};

	class RefreshPacket : public DerivedPacket {
	public:
		enum Structure {
			INTERVAL_CTRL = 0,
			DATA_OFF = 1,
			REFRESH_OFF = 2,
			COUNT = 5
		};

		TimeInterval get_data_interval() const { return TimeInterval(payload()[Structure::INTERVAL_CTRL] & static_cast<uint8_t>(0x07), payload()[Structure::DATA_OFF]); }
		TimeInterval get_refresh_interval() const {
			return TimeInterval(
				(payload()[Structure::INTERVAL_CTRL] >> 3) & static_cast<uint8_t>(0x07),
				static_cast<uint16_t>(payload()[Structure::REFRESH_OFF]) | (static_cast<uint16_t>(payload()[Structure::REFRESH_OFF + 1]) << static_cast<uint16_t>(8))
			);
		}
		uint8_t get_count() const { return payload()[Structure::COUNT]; }
		uint8_t get_fragment_length() const { return Structure::COUNT + 1; }
		uint8_t get_packet_length() const { return get_fragment_length() + Packet::Structure::PAYLOAD + 2; }

		static Packet Factory(const uint16_t src_addr,
			TimeInterval data_interval, // must be 8bits long
			TimeInterval refresh_interval, // must be 16bits long
			const uint8_t count) {

			Packet ret(PacketCtrl::REFRESH_INITIAL, src_addr);
			uint8_t* pkt = ret.payload();
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
	};

	class ACKPacket : public DerivedPacket {
		// wait but that's just a-
	public:
		static Packet Factory(const uint16_t src_addr) {
			return Packet(PacketCtrl::DATA_ACK, src_addr);
		}
	};

	uint8_t Packet::m_get_fragment_end() const {
		const PacketCtrl ctrl = get_control();

		if (ctrl == PacketCtrl::DATA_ACK_W_DATA
			|| ctrl == PacketCtrl::DATA_TRANS) return Structure::PAYLOAD + as<DataPacket>().get_fragment_length();
		else if (ctrl == PacketCtrl::REFRESH_INITIAL) return Structure::PAYLOAD + as<RefreshPacket>().get_fragment_length();
		else if (ctrl == PacketCtrl::REFRESH_ADDITONAL) /* TODO */ return 0;
		// ACK doesn't have a framecheck
		else if (ctrl == PacketCtrl::DATA_ACK) return 0;
		else return 0;
	}

	struct PacketFingerprint {
		PacketFingerprint(const uint16_t addr, const uint8_t rolling_id)
			: src_addr(addr)
			, rolling_id(rolling_id) {}

		PacketFingerprint(const DataPacket& packet)
			: PacketFingerprint(packet.get_orig_src(), packet.get_rolling_id()) {}

		const uint16_t src_addr;
		const uint8_t rolling_id;
	};
}

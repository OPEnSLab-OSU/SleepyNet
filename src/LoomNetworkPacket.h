#pragma once

#include <stdint.h>
#include "FastCRC.h"
#include "LoomNetworkUtility.h"
#include "LoomNetworkTime.h"
/**
 * Data types for Loom Network Packets
 */
namespace LoomNet {
	class Packet {
	public:
		enum Structure: uint8_t {
			CONTROL = 0,
			SRC_ADDR = 1,
			PAYLOAD = 3
		};

		explicit Packet(const PacketCtrl control, const uint16_t src_addr);
		explicit Packet(const uint8_t* raw_packet, const uint8_t max_length);

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
		const PacketType& as() const { return *reinterpret_cast<const PacketType*>(this); }
		template<class PacketType>
		PacketType& as() { return *reinterpret_cast<PacketType*>(this); }

		// calculate frame control sequence
		// run this function right before the packet is sent, to ensure it is correct
		uint16_t calc_framecheck() const;

		void set_framecheck();
		// run this function right after the packet has been recieved to verify the packet
		bool check_packet(const uint16_t expected_src = ADDR_NONE) const;
		const uint8_t* get_raw() const { return m_payload; }
		uint8_t* get_raw() { return m_payload; }
		uint8_t get_raw_length() const { return LoomNet::PACKET_MAX; }
		uint8_t get_packet_length() const { return get_control() == PacketCtrl::DATA_ACK ? m_get_fragment_end() : m_get_fragment_end() + 2; }

		// get the length we're allowed to write
		uint8_t get_write_count() const { return LoomNet::PACKET_MAX - (Structure::PAYLOAD + 2); }

	protected:
		// get the buffer
		const uint8_t* payload() const { return &(m_payload[Structure::PAYLOAD]); }
		uint8_t* payload() { return &(m_payload[Structure::PAYLOAD]); }

	private:
		uint16_t m_framecheck_impl(const uint8_t frag_end) const { return FastCRC16().xmodem(m_payload, frag_end); }
		uint8_t m_get_fragment_end() const;

		uint8_t m_payload[LoomNet::PACKET_MAX];
	};

	class DerivedPacket : public Packet {
		DerivedPacket() = delete;
		DerivedPacket(const DerivedPacket&) = delete;
		DerivedPacket& operator=(const DerivedPacket&) = delete;
	};

	class DataPacket : public Packet {
	public:
		enum Structure : uint8_t {
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
			const uint8_t length);
	};

	class RefreshPacket : public DerivedPacket {
	public:
		enum Structure : uint8_t {
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
			const uint8_t count);
	};

	class ACKPacket : public DerivedPacket {
		// wait but that's just a-
	public:
		static Packet Factory(const uint16_t src_addr) {
			return Packet(PacketCtrl::DATA_ACK, src_addr);
		}
	};

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

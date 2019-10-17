#pragma once

#include <stdint.h>

/**
 * Some utility structures and constants to be used by Loom Network
 */

namespace LoomNet {
	constexpr uint16_t ADDR_NONE = 0;
	constexpr uint16_t ADDR_COORD = 0xF000;
	constexpr uint16_t ADDR_ERROR = 0xFFFF;
	constexpr uint8_t SLOT_ERROR = 255;
	constexpr uint8_t SLOT_NONE = 254;
	constexpr auto STRING_MAX = 32;
	constexpr auto PROTOCOL_VER = 0;
	constexpr auto MAX_DEVICES = 255;
	constexpr uint8_t PACKET_MAX = 32;
	constexpr uint8_t REFRESH_CYCLE_SLOTS = 4;

	enum PacketCtrl : uint8_t {
		REFRESH_INITIAL = 0b001 | (PROTOCOL_VER << 2),
		REFRESH_ADDITONAL = 0b110 | (PROTOCOL_VER << 2),
		ERROR = 0b100 | (PROTOCOL_VER << 2),
		DATA_TRANS = 0b101 | (PROTOCOL_VER << 2),
		DATA_ACK = 0b011 | (PROTOCOL_VER << 2),
		DATA_ACK_W_DATA = 0b111 | (PROTOCOL_VER << 2),
		NONE = 0
	};

	enum class DeviceType {
		END_DEVICE,
		COORDINATOR,
		FIRST_ROUTER,
		SECOND_ROUTER,
		ERROR
	};

	DeviceType get_type(const uint16_t addr);
	uint16_t get_parent(const uint16_t addr, const DeviceType type);

	// debug stuff for simulation
	// TODO: replace this stuff with real numbers
	// TODO: CYCLES_PER_BATCH cannot be less than two
	// constexpr uint8_t CYCLES_PER_BATCH = 2;
	// constexpr uint8_t CYCLE_GAP = 1;
	// const TimeInterval SLOT_LENGTH(TimeInterval::Unit::SECOND, 5);
	// const TimeInterval RECV_TIMEOUT(TimeInterval::Unit::SECOND, 3);
	// const TimeInterval SLOT_LENGTH(TimeInterval::Unit::MILLISECOND, 1000);
	// const TimeInterval RECV_TIMEOUT(TimeInterval::Unit::MILLISECOND, 600);
	// constexpr uint8_t BATCH_GAP = 1;
	constexpr uint8_t FAIL_MAX = 6;
}
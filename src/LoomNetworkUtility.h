#pragma once

#include <cstdint>

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

	enum PacketCtrl : uint8_t {
		REFRESH_INITIAL		= 0 | (PROTOCOL_VER << 2),
		REFRESH_ADDITONAL	= 1 | (PROTOCOL_VER << 2),
		ERROR				= 3 | (PROTOCOL_VER << 2),
		DATA_TRANS			= 4 | (PROTOCOL_VER << 2),
		DATA_ACK			= 5 | (PROTOCOL_VER << 2),
		DATA_ACK_W_DATA		= 6 | (PROTOCOL_VER << 2)
	};

	enum class DeviceType {
		END_DEVICE,
		COORDINATOR,
		FIRST_ROUTER,
		SECOND_ROUTER,
		ERROR
	};

	struct TimeInterval {
		enum Unit : uint8_t {
			MICROSECOND = 0,
			MILLISECOND = 1,
			SECOND = 2,
			MINUTE = 3,
			HOUR = 4,
			DAY = 5
		};

		TimeInterval(Unit _unit, uint16_t _time)
			: unit(_unit)
			, time(_time) {}

		TimeInterval(uint8_t _unit, uint16_t _time)
			: TimeInterval(static_cast<Unit>(_unit), _time) {}

		const Unit unit;
		const uint16_t time;
	};
}
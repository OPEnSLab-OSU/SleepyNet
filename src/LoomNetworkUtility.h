#pragma once

#include <cstdint>

/**
 * Some utility structures and constants to be used by Loom Network
 */

namespace LoomNet {
	constexpr uint16_t ADDR_NONE = 0;
	constexpr uint16_t ADDR_COORD = 0xF000;
	constexpr uint16_t ADDR_ERROR = 0xFFFF;
	constexpr auto STRING_MAX = 32;

	enum class DeviceType {
		COORDINATOR,
		END_DEVICE,
		FIRST_ROUTER,
		SECOND_ROUTER,
		ERROR,
	};

	struct TimeInverval {
		enum class Unit : uint8_t {
			MICROSECOND = 0,
			MILLISECOND = 1,
			SECOND = 2,
			MINUTE = 3,
			HOUR = 4,
			DAY = 5,
		};

		TimeInverval(Unit _unit, uint16_t _time)
			: unit(_unit)
			, time(_time) {}

		TimeInverval(uint8_t _unit, uint16_t _time)
			: TimeInverval(static_cast<Unit>(_unit), _time) {}

		const Unit unit;
		const uint16_t time;
	};
}
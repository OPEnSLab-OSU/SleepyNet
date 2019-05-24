#pragma once

#include <cstdint>

/**
 * Loom Structure for storing a time interval
 */

struct LoomTimeInverval {
	enum class Unit : uint8_t {
		MICROSECOND = 0,
		MILLISECOND = 1,
		SECOND = 2,
		MINUTE = 3,
		HOUR = 4,
		DAY = 5,
	};

	LoomTimeInverval(Unit _unit, uint16_t _time)
		: unit(_unit)
		, time(_time) {}

	LoomTimeInverval(uint8_t _unit, uint16_t _time)
		: LoomTimeInverval(static_cast<Unit>(_unit), _time) {}

	const Unit unit;
	const uint16_t time;
};
#include "LoomNetworkTime.h"

using namespace LoomNet;

const TimeInterval TimeInterval::operator+(const TimeInterval& rhs) const {
	const TwoTimes& times = m_match_time(rhs);
	if (times.first().get_unit() == Unit::NONE || times.second().get_unit() == Unit::NONE) 
		return TimeInterval{ Unit::NONE, 0 };
	const uint64_t res = static_cast<uint64_t>(times.first().get_time()) + static_cast<uint64_t>(times.second().get_time());
	if (res > UINT32_MAX) 
		return TimeInterval{ Unit::NONE, 0 };
	return { times.first().get_unit(), times.first().get_time() + times.second().get_time() };
}

const TimeInterval TimeInterval::operator-(const TimeInterval& rhs) const {
	const TwoTimes& times = m_match_time(rhs);
	return  times.first().get_time() > times.second().get_time()
		? TimeInterval{ times.first().get_unit(), times.first().get_time() - times.second().get_time() }
	: TimeInterval{ Unit::NONE, 0 };
}

bool TimeInterval::operator==(const TimeInterval& rhs) const {
	const TwoTimes& times = m_match_time(rhs);
	if (times.first().get_unit() == Unit::NONE || times.second().get_unit() == Unit::NONE) {
		if (times.first().get_unit() == Unit::NONE && times.second().get_unit() == Unit::NONE)
			return true;
		else
			return false;
	}
	return times.first().get_time() == times.second().get_time();
}

void TimeInterval::downcast(const uint32_t type_max) {
	if (is_none()) 
		return;

	while (get_time() > type_max) {
		switch (m_unit) {
			// microsecond -> millisecond
			// millisecond -> second
		case Unit::MICROSECOND:
		case Unit::MILLISECOND:
			m_time /= 1000; break;
			// second -> minute, minute -> hour
		case Unit::SECOND:
		case Unit::MINUTE:
			m_time /= 60; break;
			// hour -> day
		case Unit::HOUR:
			m_time /= 24; break;
		default:
			m_time = 0;
			m_unit = Unit::NONE;
			return;
		}
		m_unit = static_cast<Unit>(static_cast<size_t>(m_unit) + 1);
	}
}

const TimeInterval TimeInterval::m_multiply(const uint32_t num) const {
	const uint64_t res = static_cast<uint64_t>(m_time)* num;
	if (res > UINT32_MAX) 
		return TimeInterval{ Unit::NONE, 0 };
	return { m_unit, m_time * num };
}

TimeInterval::TwoTimes TimeInterval::m_match_time(const TimeInterval& rhs) const {
	// error check
	if (get_unit() == Unit::NONE || rhs.get_unit() == Unit::NONE)
		return { TimeInterval(Unit::NONE, 0), TimeInterval(Unit::NONE, 0) };
	if (m_unit > rhs.m_unit) 
		return { m_match_unit(rhs, *this),  rhs };
	else if (m_unit < rhs.m_unit) 
		return { *this,  m_match_unit(*this, rhs) };
	else return { *this, rhs };
}

TimeInterval TimeInterval::m_match_unit(const TimeInterval& smaller_unit, const TimeInterval& larger_unit) const {
	uint8_t unit_index = larger_unit.m_unit;
	uint64_t time_index = larger_unit.m_time;
	while (unit_index != smaller_unit.m_unit) {
		switch (unit_index) {
			// day -> hour
		case Unit::DAY:
			time_index *= 24; break;
			// hour -> minute
			// minute -> second
		case Unit::HOUR:
		case Unit::MINUTE:
			time_index *= 60; break;
			// second -> milisecond
			// millisecond -> microsecond
		case Unit::SECOND:
		case Unit::MILLISECOND:
			time_index *= 1000; break;
		}
		unit_index--;
		if (time_index > UINT32_MAX) 
			return { Unit::NONE, 0 };
	}
	return { unit_index, static_cast<uint32_t>(time_index) };
}
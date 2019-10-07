#pragma once
#include <stdint.h>
#include <stdlib.h>

namespace LoomNet {
	template<class T>
	class TwoThings {
	public:
		TwoThings(const T& one, const T& two)
			: m_things{ one, two } {}

		const T& operator[](const size_t index) const { return m_things[index]; }
		T& operator[](const size_t index) { return m_things[index]; }

	private:
		T m_things[2];
	};

	class TimeInterval {
	public:
		enum Unit : uint8_t {
			MICROSECOND = 0,
			MILLISECOND = 1,
			SECOND = 2,
			MINUTE = 3,
			HOUR = 4,
			DAY = 5,
			NONE = 6,
		};

		using TwoTimes = TwoThings<TimeInterval>;

		TimeInterval(const Unit unit, const uint32_t time)
			: m_unit(unit)
			, m_time(time) {}

		TimeInterval(const uint8_t unit, const uint32_t time)
			: TimeInterval(static_cast<Unit>(unit), time) {}

		TimeInterval(const TimeInterval&) = default;
		TimeInterval(TimeInterval&&) = default;
		TimeInterval& operator=(TimeInterval&&) = default;
		TimeInterval& operator=(const TimeInterval&) = default;

		const TimeInterval operator+(const TimeInterval& rhs) const;

		const TimeInterval operator-(const TimeInterval& rhs) const;

		const TimeInterval operator*(const uint32_t num) const { return m_multiply(num); }
		const TimeInterval operator*(const uint16_t num) const { return m_multiply(num); }
		const TimeInterval operator*(const uint8_t num) const { return m_multiply(num); }
		const TimeInterval operator*(const int num) const { return num < 0 ? TimeInterval{ Unit::NONE, 0 } : m_multiply(num); }

		bool operator==(const TimeInterval& rhs) const;
		bool operator!=(const TimeInterval& rhs) const { return !(*this == rhs); }

		bool operator<(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE || times[1].get_unit() == Unit::NONE) return false;
			return times[0].get_time() < times[1].get_time();
		}

		bool operator>(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE || times[1].get_unit() == Unit::NONE) return false;
			return times[0].get_time() > times[1].get_time();
		}

		bool operator<=(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE || times[1].get_unit() == Unit::NONE) return false;
			return times[0].get_time() <= times[1].get_time();
		}

		bool operator>=(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE || times[1].get_unit() == Unit::NONE) return false;
			return times[0].get_time() >= times[1].get_time();
		}

		Unit get_unit() const { return m_unit; }
		uint32_t get_time() const { return m_time; }
		bool is_none() const { return m_unit == Unit::NONE; }

		void downcast(const uint32_t type_max);

	private:
		const TimeInterval m_multiply(const uint32_t num) const;
		TwoTimes m_match_time(const TimeInterval& rhs) const;
		TimeInterval m_match_unit(const TimeInterval& smaller_unit, const TimeInterval& larger_unit) const;

		Unit m_unit;
		uint32_t m_time;
	};

	const TimeInterval TIME_NONE(TimeInterval::NONE, 0);
}
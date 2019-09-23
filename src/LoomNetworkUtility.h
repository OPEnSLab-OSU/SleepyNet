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
	constexpr uint8_t REFRESH_CYCLE_SLOTS = 5;

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

		const TimeInterval operator+(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			return { times[0].get_unit(), times[0].get_time() + times[1].get_time() };
		}

		const TimeInterval operator-(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			return  times[0].get_time() > times[1].get_time() 
				? TimeInterval{ times[0].get_unit(), times[0].get_time() - times[1].get_time() } 
				: TimeInterval{ Unit::NONE, 0 };
		}

		const TimeInterval operator*(const uint32_t num) const { return { m_unit, m_time * num }; }
		const TimeInterval operator*(const uint16_t num) const { return { m_unit, m_time * num }; }
		const TimeInterval operator*(const uint8_t num) const { return { m_unit, m_time * num }; }
		const TimeInterval operator*(const int num) const { return { m_unit, m_time * num }; }

		bool operator==(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE) return false;
			return times[0].get_time() == times[1].get_time();
		}

		bool operator!=(const TimeInterval& rhs) const { return !(*this == rhs); }

		bool operator<(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE) return false;
			return times[0].get_time() < times[1].get_time();
		}

		bool operator>(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE) return false;
			return times[0].get_time() > times[1].get_time();
		}

		bool operator<=(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE) return false;
			return times[0].get_time() <= times[1].get_time();
		}

		bool operator>=(const TimeInterval& rhs) const {
			const TwoTimes& times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE) return false;
			return times[0].get_time() >= times[1].get_time();
		}

		Unit get_unit() const { return m_unit; }
		uint32_t get_time() const { return m_time; }
		bool is_none() const { return m_unit == Unit::NONE; }

		void downcast(const uint32_t type_max) {
			if (is_none()) return;

			while (get_time() > type_max) {
				switch (m_unit) {
				// millisecond -> second
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

	private:
		TwoTimes m_match_time(const TimeInterval & rhs) const {
			// error check
			if (get_unit() == Unit::NONE || rhs.get_unit() == Unit::NONE)
				return { TimeInterval(Unit::NONE, 0), TimeInterval(Unit::NONE, 0) };
			if (m_unit > rhs.m_unit) return { m_match_unit(rhs, *this),  rhs };
			else if (m_unit < rhs.m_unit) return { *this,  m_match_unit(*this, rhs) };
			else return { *this, rhs };
		}

		TimeInterval m_match_unit(const TimeInterval& smaller_unit, const TimeInterval& larger_unit) const {
			uint8_t unit_index = larger_unit.m_unit;
			uint32_t time_index = larger_unit.m_time;
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
			}
			return { unit_index, time_index };
		}

		Unit m_unit;
		uint32_t m_time;
	};

	const TimeInterval TIME_NONE(TimeInterval::NONE, 0);

	DeviceType get_type(const uint16_t addr) {
		if (addr == ADDR_NONE || addr == ADDR_ERROR) return DeviceType::ERROR;
		// figure out device type and parent address from there
		// if theres any node address, it's an end device
		if (addr & 0x00FF) return DeviceType::END_DEVICE;
		// else it's a router
		// if theres a first router in the address, check if there's a second
		if (addr & 0x0F00) return DeviceType::SECOND_ROUTER;
		return DeviceType::FIRST_ROUTER;
	}

	uint16_t get_parent(const uint16_t addr, const DeviceType type) {
		if (addr == ADDR_NONE || addr == ADDR_ERROR || type == DeviceType::ERROR)
			return ADDR_ERROR;
		// remove node address from end device
		if (type == DeviceType::END_DEVICE) {
			const uint16_t router_parent = addr & 0xFF00;
			if (router_parent) return router_parent;
			return ADDR_COORD;
		}
		// remove second router address from secound router
		if (type == DeviceType::SECOND_ROUTER) return addr & 0xF000;
		// parent of first router is always coordinator
		if (type == DeviceType::FIRST_ROUTER) return ADDR_COORD;
		// huh
		return ADDR_ERROR;
	}

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
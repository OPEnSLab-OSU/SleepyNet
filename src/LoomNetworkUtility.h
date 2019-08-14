#pragma once

#include <cstdint>
#include <functional>

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
	constexpr uint8_t REFRESH_CYCLE_SLOTS = 5;
	const TimeInterval TIME_NONE(TimeInterval::NONE, 0);

	enum PacketCtrl : uint8_t {
		REFRESH_INITIAL = 0b100 | (PROTOCOL_VER << 2),
		REFRESH_ADDITONAL = 0b011 | (PROTOCOL_VER << 2),
		ERROR = 0b001 | (PROTOCOL_VER << 2),
		DATA_TRANS = 0b101 | (PROTOCOL_VER << 2),
		DATA_ACK = 0b110 | (PROTOCOL_VER << 2),
		DATA_ACK_W_DATA = 0b111 | (PROTOCOL_VER << 2),
	};

	enum class DeviceType {
		END_DEVICE,
		COORDINATOR,
		FIRST_ROUTER,
		SECOND_ROUTER,
		ERROR
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

		TimeInterval(const Unit unit, const uint32_t time)
			: m_unit(unit)
			, m_time(time) {}

		TimeInterval(const uint8_t unit, const uint32_t time)
			: TimeInterval(static_cast<Unit>(unit), time) {}

		const TimeInterval operator+(const TimeInterval& rhs) const {
			if (m_unit > rhs.m_unit) return { rhs.m_unit, m_match_unit(rhs, *this) + rhs.m_time };
			else if (m_unit < rhs.m_unit) return { m_unit, m_match_unit(*this, rhs) + m_time };
			else return { m_unit, rhs.m_time + m_time };
		}

		const TimeInterval operator*(const uint32_t num) const { return { m_unit, m_time * num }; }
		const TimeInterval operator*(const uint16_t num) const { return { m_unit, m_time * num }; }
		const TimeInterval operator*(const uint8_t num) const { return { m_unit, m_time * num }; }
		const TimeInterval operator*(const int32_t num) const { return { m_unit, m_time * num }; }

		bool operator==(const TimeInterval& rhs) const {
			if (m_unit > rhs.m_unit) return m_match_unit(rhs, *this) == rhs.m_time;
			else if (m_unit < rhs.m_unit) return m_match_unit(*this, rhs) == m_time;
			else return rhs.m_time == m_time;
		}

		bool operator!=(const TimeInterval& rhs) const { return !(*this == rhs); }

		const Unit get_unit() const { return m_unit; }
		const uint32_t get_time() const { return m_time; }

	private:
		uint32_t m_match_unit(const TimeInterval& smaller_unit, const TimeInterval& larger_unit) const {
			// error check
			if (smaller_unit.get_unit() == Unit::NONE || larger_unit.get_unit() == Unit::NONE) 
				return larger_unit.get_time();
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
					// second -> microsecond
					case Unit::MILLISECOND:
						time_index *= 1000; break;
				}
				unit_index--;
			}
			return time_index;
		}

		const Unit m_unit;
		const uint32_t m_time;
	};

	class NetworkSim {
	public:
		NetworkSim()
			: m_net_write([](const std::array<uint8_t, 255> & packet) {})
			, m_net_read([]() { return std::array<uint8_t, 255>(); }) {}

		void set_net_write(const std::function<void(std::array<uint8_t, 255>)>& func) { m_net_write = func; }
		void set_net_read(const std::function<std::array<uint8_t, 255>(void)> & func) { m_net_read = func; }

		void net_write(const std::array<uint8_t, 255> & packet) const { m_net_write(packet); }
		std::array<uint8_t, 255> net_read() const { return m_net_read(); }

	private:
		std::function<void(std::array<uint8_t, 255>)> m_net_write;
		std::function<std::array<uint8_t, 255>(void)> m_net_read;
	};

	// debug stuff for simulation
	// TODO: replace this stuff with real numbers
	constexpr uint8_t CYCLES_PER_REFRESH = 5;
	constexpr auto LOOPS_PER_SLOT = 5;
	constexpr uint8_t CYCLE_GAP = 2;
	const TimeInterval SLOT_LENGTH(TimeInterval::Unit::SECOND, 1);
	const uint8_t BATCH_GAP = 5;
}
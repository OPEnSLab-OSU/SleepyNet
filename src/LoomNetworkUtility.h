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

		TimeInterval(const TimeInterval&) = default;
		TimeInterval(TimeInterval&&) = default;
		TimeInterval& operator=(TimeInterval&&) = default;
		TimeInterval& operator=(const TimeInterval&) = default;

		const TimeInterval operator+(const TimeInterval& rhs) const {
			const std::array<TimeInterval, 2> & times = m_match_time(rhs);
			return { times[0].get_unit(), times[0].get_time() + times[1].get_time() };
		}

		const TimeInterval operator-(const TimeInterval& rhs) const {
			const std::array<TimeInterval, 2> & times = m_match_time(rhs);
			return { times[0].get_unit(), times[0].get_time() - times[1].get_time() };
		}

		const TimeInterval operator*(const uint32_t num) const { return { m_unit, m_time * num }; }
		const TimeInterval operator*(const uint16_t num) const { return { m_unit, m_time * num }; }
		const TimeInterval operator*(const uint8_t num) const { return { m_unit, m_time * num }; }
		const TimeInterval operator*(const int32_t num) const { return { m_unit, m_time * num }; }

		bool operator==(const TimeInterval& rhs) const {
			const std::array<TimeInterval, 2> & times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE) return false;
			return times[0].get_time() == times[1].get_time();
		}

		bool operator!=(const TimeInterval& rhs) const { return !(*this == rhs); }

		bool operator<(const TimeInterval& rhs) const {
			const std::array<TimeInterval, 2> & times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE) return false;
			return times[0].get_time() < times[1].get_time();
		}

		bool operator>(const TimeInterval& rhs) const {
			const std::array<TimeInterval, 2> & times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE) return false;
			return times[0].get_time() > times[1].get_time();
		}

		bool operator<=(const TimeInterval& rhs) const {
			const std::array<TimeInterval, 2> & times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE) return false;
			return times[0].get_time() <= times[1].get_time();
		}

		bool operator>=(const TimeInterval& rhs) const {
			const std::array<TimeInterval, 2> & times = m_match_time(rhs);
			if (times[0].get_unit() == Unit::NONE) return false;
			return times[0].get_time() >= times[1].get_time();
		}

		const Unit get_unit() const { return m_unit; }
		const uint32_t get_time() const { return m_time; }

	private:
		std::array<TimeInterval, 2> m_match_time(const TimeInterval & rhs) const {
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
					// second -> microsecond
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
		if (type == DeviceType::END_DEVICE) return addr & 0xFF00;
		// remove second router address from secound router
		if (type == DeviceType::SECOND_ROUTER) return addr & 0xF000;
		// parent of first router is always coordinator
		if (type == DeviceType::FIRST_ROUTER) return ADDR_COORD;
		// huh
		return ADDR_ERROR;
	}

	class NetworkSim {
	public:
		NetworkSim()
			: m_net_write([](const std::array<uint8_t, 255> & packet) {})
			, m_net_read([](const bool clear) { return std::array<uint8_t, 255>(); })
			, m_get_time([]() { return 0; }) {}

		void set_net_write(const std::function<void(std::array<uint8_t, 255>)>& func) { m_net_write = func; }
		void set_net_read(const std::function<std::array<uint8_t, 255>(const bool)>& func) { m_net_read = func; }
		void set_get_time(const std::function<uint32_t(void)>& func) { m_get_time = func; }

		void net_write(const std::array<uint8_t, 255> & packet) const { m_net_write(packet); }
		std::array<uint8_t, 255> net_read(const bool clear) const { return m_net_read(clear); }
		uint32_t get_time() const { return m_get_time(); }

	private:
		std::function<void(std::array<uint8_t, 255>)> m_net_write;
		std::function<std::array<uint8_t, 255>(const bool)> m_net_read;
		std::function<uint32_t(void)> m_get_time;
	};

	// debug stuff for simulation
	// TODO: replace this stuff with real numbers
	constexpr uint8_t CYCLES_PER_BATCH = 5;
	constexpr uint8_t LOOPS_PER_SLOT = 5;
	constexpr uint8_t CYCLE_GAP = 2;
	const TimeInterval SLOT_LENGTH(TimeInterval::Unit::SECOND, 1);
	const uint8_t BATCH_GAP = 5;
}
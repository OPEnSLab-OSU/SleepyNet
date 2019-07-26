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
	constexpr auto CYCLES_PER_REFRESH = 5;
	
	// debug stuff for simulation
	// TODO: replace this stuff with real numbers
	constexpr auto LOOPS_PER_SLOT = 10;

	enum PacketCtrl : uint8_t {
		REFRESH_INITIAL		= 0b100 | (PROTOCOL_VER << 2),
		REFRESH_ADDITONAL	= 0b011 | (PROTOCOL_VER << 2),
		ERROR				= 0b001 | (PROTOCOL_VER << 2),
		DATA_TRANS			= 0b101 | (PROTOCOL_VER << 2),
		DATA_ACK			= 0b110 | (PROTOCOL_VER << 2),
		DATA_ACK_W_DATA		= 0b111 | (PROTOCOL_VER << 2),
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
}
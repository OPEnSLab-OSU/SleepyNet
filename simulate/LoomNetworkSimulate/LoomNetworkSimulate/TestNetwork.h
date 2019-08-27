#pragma once

#include "../../../src/LoomNetwork.h"
#include "../../../src/CircularBuffer.h"
#include "../../../src/LoomRouter.h"
#include "../../../src/LoomNetworkConfig.h"
#include "../../../src/LoomNetworkInfo.h"
#include <iostream>
#include <vector>
#include <bitset>
#include <iomanip>
#include <string>
#include <sstream>
#include <limits>
#include <array>
#include <utility>
#include <random>

class NulStreambuf : public std::streambuf
{
	char                dummyBuffer[64];
protected:
	virtual int         overflow(int c)
	{
		setp(dummyBuffer, dummyBuffer + sizeof(dummyBuffer));
		return (c == traits_type::eof()) ? '\0' : c;
	}
};

class TestRadio : public LoomNet::Radio {
public:
	TestRadio(std::array<uint8_t, 255> & airwaves, const std::function<LoomNet::TimeInterval(void)> get_time, const std::function<bool(void)> is_drop)
		: m_airwaves(airwaves)
		, m_get_time(get_time)
		, m_is_drop(is_drop)
		, m_state(State::DISABLED) {}

	LoomNet::TimeInterval get_time() override { return m_get_time(); }
	LoomNet::Radio::State get_state() const override { return m_state; }
	void enable() override {
		if (m_state != State::DISABLED) std::cout << "Invalid radio state movement in enable()" << std::endl;
		m_state = State::SLEEP;
	}
	void disable() override {
		if (m_state != State::SLEEP) std::cout << "Invalid radio state movement in disable()" << std::endl;
		m_state = State::DISABLED;
	}
	void sleep() override {
		if (m_state != State::IDLE) std::cout << "Invalid radio state movement in sleep()" << std::endl;
		m_state = State::SLEEP;
	}
	void wake() override {
		if (m_state != State::SLEEP) std::cout << "Invalid radio state movement in wake()" << std::endl;
	}
	LoomNet::Packet recv() override {
		return LoomNet::Packet{ m_airwaves.data(), static_cast<uint8_t>(m_airwaves.size()) };
	}
	void send(const LoomNet::Packet& send) override {
		for (auto i = 0; i < send.get_raw_length(); i++) m_airwaves[i] = send.get_raw()[i];
 	}

private:
	std::array<uint8_t, 255>& m_airwaves;
	const std::function<LoomNet::TimeInterval(void)> m_get_time;
	const std::function<bool(void)> m_is_drop;
	State m_state;
};

class TestNetwork {
public:
	

	enum class Verbosity : uint8_t {
		VERBOSE = 3,
		INFO = 2,
		ERROR = 1,
		NONE = 0,
	};

	enum class Error {
		OK,
		OUT_OF_SYNC,
		UNKNOWN_PACKET,
		DEVICE_CLOSED,
	};

	using NetType = LoomNet::Network<TestRadio, 16, 16, 64>;
	using NetStatus = NetType::Status;
	using NetTrack = std::tuple<uint16_t, std::string, size_t>;

	TestNetwork(const JsonObjectConst& obj, const Verbosity verbose = Verbosity::ERROR)
		: airwaves{ 0 }
		, cur_slot(0)
		, cur_loop(0)
		, drop_rate(0)
		, radio(airwaves, [this]() -> LoomNet::TimeInterval { return LoomNet::TimeInterval(
					LoomNet::TimeInterval::SECOND, 
					cur_slot * LoomNet::LOOPS_PER_SLOT + cur_loop 
				); 
			}, [this]() -> bool {
				return drop_rate > 0
					&& std::uniform_int_distribution<int>(0, 99)(rand_engine) > drop_rate;
			})
		, devices{ {
			{ LoomNet::read_network_topology(obj, "Router 3 Router 1 End Device 1"), radio },
			{ LoomNet::read_network_topology(obj, "Router 3 Router 1"), radio },
			{ LoomNet::read_network_topology(obj, "Router 3"), radio },
			{ LoomNet::read_network_topology(obj, "Router 2 End Device 1"), radio },
			{ LoomNet::read_network_topology(obj, "Router 2"), radio },
			{ LoomNet::read_network_topology(obj, "Router 1 End Device 3"), radio },
			{ LoomNet::read_network_topology(obj, "Router 1 Router 2"), radio },
			{ LoomNet::read_network_topology(obj, "Router 1 Router 2 End Device 1"), radio },
			{ LoomNet::read_network_topology(obj, "Router 1 Router 2 End Device 2"), radio },
			{ LoomNet::read_network_topology(obj, "Router 1 Router 1"), radio },
			{ LoomNet::read_network_topology(obj, "Router 1 Router 1 End Device 1"), radio },
			{ LoomNet::read_network_topology(obj, "Router 1 End Device 2"), radio },
			{ LoomNet::read_network_topology(obj, "Router 1 End Device 1"), radio },
			{ LoomNet::read_network_topology(obj, "Router 1"), radio },
			{ LoomNet::read_network_topology(obj, "End Device 1"), radio },
			{ LoomNet::read_network_topology(obj, "BillyTheCoord"), radio }
		} }
		, next_wake_times{ 0 }
		, send_track()
		, rand_engine(std::random_device()())
		, how_much(verbose)
		, null_buf()
		, null_stream(&null_buf)
		, last_error(Error::OK) {}

	void next_slot() {
		// clear the network
		airwaves.fill(0);
		cur_loop = 0;
		// increment all the slot time trackers in the send tracking hash table
		for (auto& elem : send_track)
			std::get<2>(elem.second)++;
		m_print(Verbosity::VERBOSE) << "Slot: " << std::dec << cur_slot << std::endl;
		// wake all the devices that scheduled a wakeup now
		m_print(Verbosity::VERBOSE) << "	Woke: ";
		auto woke_count = 0;
		for (uint8_t o = 0; o < devices.size(); o++) {
			if (devices[o].get_status() & NetStatus::NET_SLEEP_RDY) {
				if (cur_slot * LoomNet::LOOPS_PER_SLOT >= next_wake_times[o]) {
					devices[o].net_sleep_wake_ack();
					woke_count++;
					m_print(Verbosity::VERBOSE) << "0x" << std::hex << std::setfill('0') << std::setw(4) << devices[o].get_router().get_self_addr() << ", ";
				}
			}
		}
		m_print(Verbosity::VERBOSE) << std::endl;
		if (woke_count != 1 && woke_count != 2 && woke_count != 16 && woke_count != 0 && woke_count != 15) last_error = Error::OUT_OF_SYNC;
		// iterate through each element until all of them are asleep, then move to the next slot
		bool all_sleep;
		for (; cur_loop < LoomNet::LOOPS_PER_SLOT; cur_loop++) {
			all_sleep = true;
			m_print(Verbosity::VERBOSE) << "	Iteration " << cur_loop << ":" << std::endl;
			for (uint8_t i = 0; i < devices.size(); i++) {
				// if the device is awake
				const uint8_t status = devices[i].get_status();
				if (status == NetStatus::NET_CLOSED) {
					m_print(Verbosity::ERROR) << std::hex << devices[i].get_router().get_self_addr() << " closed!" << std::endl;
					last_error = Error::DEVICE_CLOSED;
				}
				// sleep check
				else if (!(status & NetStatus::NET_SLEEP_RDY)) {
					// Send/recieve data!
					if (status & NetStatus::NET_RECV_RDY) {
						m_print(Verbosity::VERBOSE) << "	0x" << std::hex << std::setfill('0') << std::setw(4) << devices[i].get_router().get_self_addr();
						m_print(Verbosity::VERBOSE) << " recieved: " << std::endl;
						do {
							const LoomNet::Packet& buf = devices[i].app_recv();
							const LoomNet::DataPacket& frag = buf.as<LoomNet::DataPacket>();
							const std::string payload(reinterpret_cast<const char*>(frag.get_payload()), frag.get_payload_length());
							m_print(Verbosity::VERBOSE) << "		" << payload << std::endl;
							// find the packet in the tracking table, and remove it
							// verifying it was recieved
							// get all the packets that are outbound for this device
							auto find_iter = send_track.equal_range(devices[i].get_router().get_self_addr());
							auto& e = find_iter.first;
							size_t num_slots = 0;
							// find the one that matchs this packet
							for (; e != find_iter.second; ++e) {
								if (std::get<0>(e->second) == frag.get_orig_src()
									&& std::get<1>(e->second).compare(payload) == 0) {
									num_slots = std::get<2>(e->second);
									break;
								}
							}
							// if we didn't find one, print and error out
							if (e == find_iter.second) {
								m_print(Verbosity::ERROR) << "	Invalid or duplicated packet!" << std::endl;
								last_error = Error::UNKNOWN_PACKET;
							}
							// else erase the one we found
							else send_track.erase(e);
							// else print some useful data
							m_print(Verbosity::VERBOSE) << "		From: 0x" << std::hex << std::setfill('0') << std::setw(4) << frag.get_orig_src() << std::endl;
							m_print(Verbosity::VERBOSE) << "		Took " << std::dec << num_slots << " slots" << std::endl;
						} while (devices[i].get_status() & NetStatus::NET_RECV_RDY);
					}
					// run the state machine
					const uint8_t new_status = devices[i].net_update();
					m_print(Verbosity::VERBOSE) << "		Status of 0x" << std::hex << std::setfill('0') << std::setw(4) << devices[i].get_router().get_self_addr() << ": " << std::bitset<8>(devices[i].get_status()) << std::endl;
					// if it wants to go to sleep now, add the time to the wake times
					if (new_status & NetStatus::NET_SLEEP_RDY)
						next_wake_times[i] = devices[i].net_sleep_next_wake_time().get_time() + cur_slot * LoomNet::LOOPS_PER_SLOT + cur_loop;
					else all_sleep = false;
				}
			}
			if (all_sleep) break;
		}
		m_print(Verbosity::VERBOSE) << "	Took " << cur_loop << " iterations." << std::endl;
		// next slot!
		cur_slot++;
	}

	uint8_t next_cycle() {
		// use the coordinator to judge the current state of the network
		const LoomNet::Slotter& slot = devices[15].get_mac().get_slotter();
		uint8_t last_cycle;
		do {
			last_cycle = slot.get_cur_data_cycle();
			next_slot();
		} while (last_cycle == slot.get_cur_data_cycle());
		return slot.get_cur_data_cycle();
	}

	void next_batch() {
		while (next_cycle() != 0);
	}

	bool send_data_and_verify(const uint16_t addr_src, const uint16_t addr_dst, const std::string& payload) {
		// find the device with the right address
		size_t index = std::numeric_limits<size_t>::max();
		for (size_t i = 0; i < devices.size(); i++) {
			if (devices[i].get_router().get_self_addr() == addr_src) {
				index = i;
				break;
			}
		}
		if (index == std::numeric_limits<size_t>::max())
			m_print(Verbosity::ERROR) << "Could not find specified device: 0x" << std::hex << std::setfill('0') << std::setw(4) << addr_src << std::endl;
		else if ((devices[index].get_status() & NetStatus::NET_SEND_RDY) == 0)
			m_print(Verbosity::ERROR) << "Device is unable to send!" << std::endl;
		// tell the device to send the data
		else {
			// send the packet!
			devices[index].app_send(addr_dst,
				0,
				reinterpret_cast<const uint8_t * const>(payload.c_str()),
				static_cast<uint8_t>(payload.length()));
			// add the packet to the internal tracking system, so we can verify it was recieved
			send_track.emplace(addr_dst, NetTrack{ addr_src, payload, 0 });
			return true;
		}
		return false;
	}

	size_t pending_packet_count() const { return send_track.size(); }

	void set_drop_rate(const int new_drop_rate) { drop_rate = new_drop_rate; }

	std::ostream& m_print(const Verbosity v) {
		// emptey our string stream
		if (static_cast<size_t>(v) <= static_cast<size_t>(how_much)) return std::cout;
		else return null_stream;
	}

	std::array<uint8_t, 255> airwaves;
	size_t cur_slot;
	size_t cur_loop;
	int drop_rate;
	TestRadio radio;
	std::array<NetType, 16> devices;
	std::array<unsigned, 16> next_wake_times;
	std::multimap<uint16_t, NetTrack> send_track;
	std::default_random_engine rand_engine;
	const Verbosity how_much;
	NulStreambuf null_buf;
	std::ostream null_stream;
	Error last_error;
};

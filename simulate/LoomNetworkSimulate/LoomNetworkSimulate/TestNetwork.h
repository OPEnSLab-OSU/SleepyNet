#pragma once

#include "../../../src/LoomNetwork.h"
#include "../../../src/CircularBuffer.h"
#include "../../../src/LoomRouter.h"
#include "../../../src/LoomNetworkConfig.h"
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
#include <map>

/*
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

constexpr static LoomNet::TimeInterval::Unit slot_unit = LoomNet::TimeInterval::Unit::SECOND;
constexpr static uint32_t slot_length = 10;

class TestRadio : public LoomNet::Radio {
public:
	TestRadio(std::array<uint8_t, LoomNet::PACKET_MAX> & airwaves, const size_t& cur_slot, const size_t& cur_loop, std::default_random_engine& rand, const int& drop_rate)
		: m_airwaves(airwaves)
		, m_cur_slot(cur_slot)
		, m_cur_loop(cur_loop)
		, m_rand(rand)
		, m_drop_rate(drop_rate)
		, m_state(State::DISABLED) {}

	LoomNet::TimeInterval get_time() const override { return { slot_unit, m_cur_slot * slot_length + m_cur_loop }; }
	LoomNet::Radio::State get_state() const override { return m_state; }
	void enable() override {
		if (m_state != State::DISABLED) 
			std::cout << "Invalid radio state movement in enable()" << std::endl;
		m_state = State::SLEEP;
	}
	void disable() override {
		if (m_state != State::SLEEP) 
			std::cout << "Invalid radio state movement in disable()" << std::endl;
		m_state = State::DISABLED;
	}
	void sleep() override {
		if (m_state != State::IDLE) 
			std::cout << "Invalid radio state movement in sleep()" << std::endl;
		m_state = State::SLEEP;
	}
	void wake() override {
		if (m_state != State::SLEEP) 
			std::cout << "Invalid radio state movement in wake()" << std::endl;
		m_state = State::IDLE;
	}
	LoomNet::Packet recv(LoomNet::TimeInterval& recv_stamp) override {
		if (m_state != State::IDLE) 
			std::cout << "Invalid radio state to recv" << std::endl;
		recv_stamp = get_time();
		return LoomNet::Packet{ m_airwaves.data(), static_cast<uint8_t>(m_airwaves.size()) };
	}
	void send(const LoomNet::Packet& send) override {
		if (m_state != State::IDLE) 
			std::cout << "Invalid radio state to recv" << std::endl;
		if (m_drop_rate == 0
			|| std::uniform_int_distribution<int>(0, 99)(m_rand) > m_drop_rate)
			for (auto i = 0; i < send.get_packet_length(); i++) m_airwaves[i] = send.get_raw()[i];
		else {
			// std::cout << "Droppped packet!" << std::endl;
			m_airwaves.fill(0);
		}
 	}

private:
	std::array<uint8_t, LoomNet::PACKET_MAX>& m_airwaves;
	const size_t& m_cur_slot;
	const size_t& m_cur_loop;
	std::default_random_engine& m_rand;
	const int& m_drop_rate;
	State m_state;
};

using NetType = LoomNet::Network<TestRadio, 16, 16, 128>;

static void recurse_all_devices(const JsonArrayConst& children_array, const JsonObjectConst& root, std::vector<NetType>& devices, const TestRadio& radio) {
	for (const JsonObjectConst obj : children_array) {
		if (!obj.isNull()) {
			devices.emplace_back(LoomNet::read_network_topology(root, obj["name"]), radio);
			const JsonArrayConst childs = obj["children"];
			if (!childs.isNull()) 
				recurse_all_devices(childs, root, devices, radio);
		}
	}
}

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

	using NetStatus = NetType::Status;
	using NetTrack = std::tuple<uint16_t, std::string, size_t>;

	TestNetwork(const JsonObjectConst& obj, const Verbosity verbose = Verbosity::ERROR)
		: airwaves{ 0 }
		, cur_slot(0)
		, cur_loop(0)
		, drop_rate(0)
		, rand_engine(std::random_device()())
		, devices{}
		, next_wake_times{}
		, send_track()
		, dupe_track()
		, how_much(verbose)
		, null_buf()
		, null_stream(&null_buf)
		, last_error(Error::OK) {
		const TestRadio radio(airwaves, cur_slot, cur_loop, rand_engine, drop_rate);
		// create the devices array from the json!
		const JsonObjectConst root = obj["root"];
		// add the coordinator!
		devices.emplace_back(LoomNet::read_network_topology(obj, root["name"]), radio);
		const JsonArrayConst children = root["children"];
		// add everything else through recursion!
		if (!children.isNull()) recurse_all_devices(children, obj, devices, radio);
		// initialize next_wake_times
		next_wake_times.resize(devices.size(), 0);
		// and the array of all addresses
		all_addrs.resize(devices.size(), 0);
		for (size_t i = 0; i < devices.size(); i++) all_addrs[i] = devices[i].get_router().get_self_addr();
	}

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
		unsigned int woke_count = 0;
		for (uint8_t o = 0; o < devices.size(); o++) {
			if (devices[o].get_status() & NetStatus::NET_SLEEP_RDY) {
				if (cur_slot * slot_length >= next_wake_times[o]) {
					devices[o].net_sleep_wake_ack();
					woke_count++;
					m_print(Verbosity::VERBOSE) << "0x" << std::hex << std::setfill('0') << std::setw(4) << devices[o].get_router().get_self_addr() << ", ";
				}
			}
		}
		m_print(Verbosity::VERBOSE) << std::endl;
		if (woke_count >= 3 && woke_count <= devices.size() - 2 && woke_count != 1) {
			m_print(Verbosity::ERROR) << "Devcies out of sync!" << std::endl;
			last_error = Error::OUT_OF_SYNC;
		}
		// iterate through each element until all of them are asleep, then move to the next slot
		bool all_sleep;
		for (; cur_loop < slot_length; cur_loop++) {
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
								// check our dupe list to see if this packet is invalid or a duplicate
								auto find_iter_dupe = dupe_track.equal_range(devices[i].get_router().get_self_addr());
								auto& f = find_iter_dupe.first;
								// find the one that matchs this packet
								for (; f != find_iter_dupe.second; ++f)
									if (std::get<0>(f->second) == frag.get_orig_src()
										&& std::get<1>(f->second).compare(payload) == 0)
										break;
								if (f == find_iter_dupe.second) 
									m_print(Verbosity::ERROR) << "Invalid packet!" << std::endl;
								else 
									m_print(Verbosity::ERROR) << "Duplicated packet!" << std::endl;
								last_error = Error::UNKNOWN_PACKET;
							}
							// else add the one we found to the duplicate tracker so we can find it later
							else {
								dupe_track.insert(*e);
								send_track.erase(e);
							}
							// else print some useful data
							m_print(Verbosity::VERBOSE) << "		From: 0x" << std::hex << std::setfill('0') << std::setw(4) << frag.get_orig_src() << std::endl;
							m_print(Verbosity::VERBOSE) << "		Took " << std::dec << num_slots << " slots" << std::endl;
						} while (devices[i].get_status() & NetStatus::NET_RECV_RDY);
					}
					// run the state machine
					const uint8_t new_status = devices[i].net_update();
					m_print(Verbosity::VERBOSE) << "		Status of 0x" << std::hex << std::setfill('0') << std::setw(4) << devices[i].get_router().get_self_addr() << ": " << std::bitset<8>(devices[i].get_status()) << std::endl;
					// if it wants to go to sleep now, add the time to the wake times
					if (new_status & NetStatus::NET_SLEEP_RDY) {
						next_wake_times[i] = devices[i].net_sleep_next_wake_time().get_time();
					}
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
		const LoomNet::Slotter& slot = devices[0].get_mac().get_slotter();
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

	void clear_dupes() { dupe_track.clear(); }

	std::ostream& m_print(const Verbosity v) {
		// emptey our string stream
		if (static_cast<size_t>(v) <= static_cast<size_t>(how_much)) return std::cout;
		else return null_stream;
	}

	std::array<uint8_t, LoomNet::PACKET_MAX> airwaves;
	size_t cur_slot;
	size_t cur_loop;
	int drop_rate;
	std::default_random_engine rand_engine;
	std::vector<NetType> devices;
	std::vector<uint32_t> next_wake_times;
	std::vector<uint16_t> all_addrs;
	std::multimap<uint16_t, NetTrack> send_track;
	std::multimap<uint16_t, NetTrack> dupe_track;
	const Verbosity how_much;
	NulStreambuf null_buf;
	std::ostream null_stream;
	Error last_error;
};
*/
// LoomNetworkSimulate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

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

class Int {
public:
	Int(int i) : m_i(i) {}
	~Int() { std::cout << m_i << " Dtor called!" << std::endl; }

	int m_i;
};

void test_address(const LoomNet::NetworkInfo r1, const LoomNet::Router r2, const LoomNet::Slotter r3, const std::string error) {
	if (!(r1.route_info == r2)) {
		std::cout << "Route init failed: " << error << std::endl;
	}
	else if (!(r1.slot_info == r3)) {
		std::cout << "Slotter init failed: " << error << std::endl;
	}
}

void test_route(const LoomNet::Router r1, const uint16_t dst, const uint16_t check, const std::string error) {
	const uint16_t routed = r1.route(dst);
	if (routed != check) {
		std::cout << "Routing failed: " << error << std::endl;
	}
}

class TestNetwork {
public:
	using NetStatus = LoomNet::Network<16, 16>::Status;
	using NetTrack = std::tuple<uint16_t, std::string, size_t>;

	TestNetwork(const JsonObjectConst& obj)
		: airwaves{ 0 }
		, devices{ {
			{ LoomNet::read_network_topology(obj, "Router 3 Router 1 End Device 1"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 3 Router 1"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 3"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 2 End Device 1"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 2"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 1 End Device 3"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 1 Router 2"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 1 Router 2 End Device 1"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 1 Router 2 End Device 2"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 1 Router 1"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 1 Router 1 End Device 1"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 1 End Device 2"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 1 End Device 1"), sim_net },
			{ LoomNet::read_network_topology(obj, "Router 1"), sim_net },
			{ LoomNet::read_network_topology(obj, "End Device 1"), sim_net },
			{ LoomNet::read_network_topology(obj, "BillyTheCoord"), sim_net }
		} }
		, busy(false)
		, next_wake_times{ 0 }
		, sim_net()
		, cur_slot(0)
		, send_track()
		, rand_engine(std::random_device()())
	{
		reset();
	}

	bool next_slot(bool quiet = false) {
		// clear the network
		if (busy) {
			std::cout << "Unread network packet!";
			return false;
		}
		airwaves.fill(0);
		// increment all the slot time trackers in the send tracking hash table
		for (auto& elem : send_track)
			std::get<2>(elem.second)++;
		if (!quiet) std::cout << "Slot: " << std::dec << cur_slot << std::endl;
		// wake all the devices that scheduled a wakeup now
		if (!quiet) std::cout << "	Woke: ";
		auto woke_count = 0;
		for (uint8_t o = 0; o < devices.size(); o++) {
			if (cur_slot > next_wake_times[o]) {
				next_wake_times[o]++;
				devices[o].net_sleep_wake_ack();
				woke_count++;
				if (!quiet) std::cout << "0x" << std::hex << std::setfill('0') << std::setw(4) << devices[o].get_router().get_self_addr() << ", ";
			}
		}
		if (!quiet) std::cout << std::endl;
		if (woke_count != 2 && woke_count != 16 && woke_count != 0) {
			std::cout << "Devices out of sync!" << std::endl;
			return false;
		}
		// iterate through each element until all of them are asleep, then move to the next slot
		bool all_sleep;
		auto iterations = 0;
		for (; iterations < LoomNet::LOOPS_PER_SLOT; iterations++) {
			all_sleep = true;
			if (!quiet) std::cout << "	Iteration " << iterations << ":" << std::endl;
			for (uint8_t i = 0; i < devices.size(); i++) {
				// if the device is awake
				const uint8_t status = devices[i].get_status();
				if (status == NetStatus::NET_CLOSED) {
					std::cout << std::hex << devices[i].get_router().get_self_addr() << " closed!" << std::endl;
					return false;
				}
				// sleep check
				else if (!(status & NetStatus::NET_SLEEP_RDY)) {
					// Send/recieve data!
					if (status & NetStatus::NET_RECV_RDY) {
						if (!quiet) {
							std::cout << "	0x" << std::hex << std::setfill('0') << std::setw(4) << devices[i].get_router().get_self_addr();
							std::cout << " recieved: " << std::endl;
						}
						do {
							const LoomNet::Packet& buf = devices[i].app_recv();
							const LoomNet::DataPacket& frag = buf.as<LoomNet::DataPacket>();
							const std::string payload(reinterpret_cast<const char*>(frag.get_payload()), frag.get_payload_length());
							if (!quiet) std::cout << "		" << payload << std::endl;
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
								std::cout << "	Invalid packet!" << std::endl;
								return false;
							}
							// else erase the one we found
							else send_track.erase(e);
							// else print some useful data
							if (!quiet) std::cout << "		From: 0x" << std::hex << std::setfill('0') << std::setw(4) << frag.get_orig_src() << std::endl;
							if (!quiet) std::cout << "		Took " << std::dec << num_slots << " slots" << std::endl;
						} while (devices[i].get_status() & NetStatus::NET_RECV_RDY);
					}
					// run the state machine
					const uint8_t new_status = devices[i].net_update();
					if (!quiet) std::cout << "		Status of 0x" << std::hex << std::setfill('0') << std::setw(4) << devices[i].get_router().get_self_addr() << ": " << std::bitset<8>(devices[i].get_status()) << std::endl;
					// if it wants to go to sleep now, add the time to the wake times
					if (new_status & NetStatus::NET_SLEEP_RDY)
						next_wake_times[i] += devices[i].net_sleep_next_wake_time().get_time();
					else all_sleep = false;
				}
			}
			if (all_sleep) break;
		}
		if (!quiet) std::cout << "	Took " << iterations << " iterations." << std::endl;
		// next slot!
		cur_slot++;
		return true;
	}

	uint8_t next_cycle(bool quiet = true) {
		// use the coordinator to judge the current state of the network
		const LoomNet::Slotter& slot = devices[15].get_mac().get_slotter();
		uint8_t last_cycle;
		do {
			last_cycle = slot.get_cur_data_cycle();
			if (!next_slot(quiet)) {
				std::cout << " Failed during next cycle" << std::endl;
				return 0;
			}

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
			std::cout << "Could not find specified device: 0x" << std::hex << std::setfill('0') << std::setw(4) << addr_src << std::endl;
		else if ((devices[index].get_status() & NetStatus::NET_SEND_RDY) == 0)
			std::cout << "Device is unable to send!" << std::endl;
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

	void reset() {
		airwaves.fill(0);
		for (auto& d : devices) d.reset();
		busy = false;
		next_wake_times.fill(0);
		cur_slot = 0;
		send_track.clear();
		// every time the device writes to the network
		sim_net.set_net_write([this](std::array<uint8_t, 255> packet) {
			const uint16_t src_addr = static_cast<uint16_t>(packet[1]) | (static_cast<uint16_t>(packet[2]) << 8);
			// std::cout << "		Transmission from: 0x" << std::hex << std::setfill('0') << std::setw(4) << src_addr << std::endl;
			if (busy) {
				std::cout << "		Collision!" << std::endl;
			}
			// set the airwaves
			airwaves = packet;
			busy = true;
			});
		// every time any device reads from the network
		sim_net.set_net_read([this](const bool clear) {
			// not busy anymore! we read already
			busy = false;
			// clear if needed
			const std::array<uint8_t, 255> copy = airwaves;
			if (clear) airwaves.fill(0);
			return copy;
			});
		// get the current time as the current slot
		sim_net.set_get_time([this]() { return cur_slot; });
	}

	void enable_lossy_network(int prob) {
		// every time the device writes to the network
		sim_net.set_net_write([this, prob](std::array<uint8_t, 255> packet) {
			std::cout << "		Transmission from: 0x" << std::hex << std::setfill('0') << std::setw(4) << (static_cast<uint16_t>(packet[1]) | static_cast<uint16_t>(packet[2]) << 8) << std::endl;
			if (busy) {
				std::cout << "		Collision!" << std::endl;
			}
			// have a certain probability of losing the packet
			if (std::uniform_int_distribution<int>(0, 99)(rand_engine) > prob) {
				// set the airwaves
				airwaves = packet;
				busy = true;
			}
			else {
				std::cout << "								Dropped packet!" << std::endl;
			}
		});
		// every time any device reads from the network
		sim_net.set_net_read([this](const bool clear) {
			// not busy anymore! we read already
			busy = false;
			// clear if needed
			const std::array<uint8_t, 255> copy = airwaves;
			if (clear) airwaves.fill(0);
			return copy;
		});
		// get the current time as the current slot
		sim_net.set_get_time([this]() { return cur_slot; });
	}

	size_t pending_packet_count() const { return send_track.size(); }

	std::array<uint8_t, 255> airwaves;
	std::array<LoomNet::Network<16, 16>, 16> devices;
	bool busy;
	std::array<unsigned, 16> next_wake_times{ 0 };
	LoomNet::NetworkSim sim_net;
	size_t cur_slot;
	std::multimap<uint16_t, NetTrack> send_track;
	std::default_random_engine rand_engine;
};

int main()
{

	std::cout << "Hello World!\n"; 

	std::cout << "Begin testing circular buffer!\n";

	{
		std::vector<bool> outputs;
		CircularBuffer<Int, 6> buffer;

		outputs.push_back(buffer.add_front(Int(2)));

		outputs.push_back(buffer.add_back(Int(3)));

		outputs.push_back(buffer.emplace_back(5));

		outputs.push_back(buffer.add_back(Int(4)));

		outputs.push_back(buffer.emplace_front(9));

		outputs.push_back(buffer.add_front(Int(4)));

		outputs.push_back(buffer.add_back(Int(3)));
		outputs.push_back(buffer.add_front(Int(3)));

		for (auto const& i : buffer.crange()) std::cout << i.m_i << ", ";
		std::cout << std::endl;

		for (auto const& i : outputs) std::cout << i << ", ";
		std::cout << std::endl;

		buffer.destroy_back();

		buffer.destroy_front();

		for (auto const& i : buffer.crange()) std::cout << i.m_i << ", ";
		std::cout << std::endl;

		buffer.add_back(Int(30));
		buffer.add_front(Int(90));

		for (auto const& i : buffer.crange()) std::cout << i.m_i << ", ";
		std::cout << std::endl;

		auto iter = buffer.crange().begin();
		++iter;
		++iter;
		++iter;

		buffer.remove(iter);

		for (auto const& i : buffer.crange()) std::cout << i.m_i << ", ";
		std::cout << std::endl;

		for (auto iter = buffer.crange().begin(); iter != buffer.crange().end(); ++iter)  std::cout << (*iter).m_i << ", ";
		std::cout << std::endl;
	}

	std::cout << "end testing circular buffer" << std::endl;
	std::cout << "begin testing JSON network parsing" << std::endl;

	constexpr char JSONStr[] = "{\"root\":{\"name\":\"BillyTheCoord\",\"sensor\":false,\"children\":[{\"name\":\"End Device 1\",\"type\":0,\"addr\":\"0x001\"},{\"name\":\"Router 1\",\"sensor\":false,\"type\":1,\"addr\":\"0x1000\",\"children\":[{\"name\":\"Router 1 End Device 1\",\"type\":0,\"addr\":\"0x1001\"},{\"name\":\"Router 1 End Device 2\",\"type\":0,\"addr\":\"0x1002\"},{\"name\":\"Router 1 Router 1\",\"sensor\":false,\"type\":1,\"addr\":\"0x1100\",\"children\":[{\"name\":\"Router 1 Router 1 End Device 1\",\"type\":0,\"addr\":\"0x1101\"}]},{\"name\":\"Router 1 Router 2\",\"sensor\":true,\"type\":1,\"addr\":\"0x1200\",\"children\":[{\"name\":\"Router 1 Router 2 End Device 1\",\"type\":0,\"addr\":\"0x1001\"},{\"name\":\"Router 1 Router 2 End Device 2\",\"type\":0,\"addr\":\"0x1202\"}]},{\"name\":\"Router 1 End Device 3\",\"type\":0,\"addr\":\"0x1003\"}]},{\"name\":\"Router 2\",\"sensor\":true,\"type\":1,\"addr\":\"0x2000\",\"children\":[{\"name\":\"Router 2 End Device 1\",\"type\":0,\"addr\":\"0x2001\"}]},{\"name\":\"Router 3\",\"sensor\":false,\"type\":1,\"addr\":\"0x3000\",\"children\":[{\"name\":\"Router 3 Router 1\",\"sensor\":false,\"type\":1,\"addr\":\"0x3100\",\"children\":[{\"name\":\"Router 3 Router 1 End Device 1\",\"type\":0,\"addr\":\"0x3101\"}]}]}]}}";
	constexpr auto size = 2048;

	StaticJsonDocument<size> json;
	deserializeJson(json, JSONStr);
	const auto obj = json.as<JsonObjectConst>();

	const std::array<uint16_t, 16> all_addrs{ 0x3101, 0x3100, 0x3000, 0x2001, 0x2000, 0x1003, 0x1200, 0x1201, 0x1202, 0x1100, 0x1101, 0x1002, 0x1001, 0x1000, 0x0001, LoomNet::ADDR_COORD };

	{
		using namespace LoomNet;
		test_address(read_network_topology(obj, "Router 3 Router 1 End Device 1"),	Router(DeviceType::END_DEVICE, 0x3101, 0x3100, 0, 0),			Slotter(3, 24, 0), "Router 3 Router 1 End Device 1");
		test_address(read_network_topology(obj, "Router 3 Router 1"),				Router(DeviceType::SECOND_ROUTER, 0x3100, 0x3000, 0, 1),		Slotter(12, 24, 0, 1, 3, 1), "Router 3 Router 1");
		test_address(read_network_topology(obj, "Router 3"),						Router(DeviceType::FIRST_ROUTER, 0x3000, ADDR_COORD, 1, 0),		Slotter(22, 24, 0, 1, 12, 1), "Router 3");
		test_address(read_network_topology(obj, "Router 2 End Device 1"),			Router(DeviceType::END_DEVICE, 0x2001, 0x2000, 0, 0),			Slotter(11, 24, 0), "Router 2 End Device 1");
		test_address(read_network_topology(obj, "Router 2"),						Router(DeviceType::FIRST_ROUTER, 0x2000, ADDR_COORD, 0, 1),		Slotter(20, 24, 0, 2, 11, 1), "Router 2");
		test_address(read_network_topology(obj, "Router 1 End Device 3"),			Router(DeviceType::END_DEVICE, 0x1003, 0x1000, 0, 0),			Slotter(10, 24, 0), "Router 1 End Device 3");
		test_address(read_network_topology(obj, "Router 1 Router 2"),				Router(DeviceType::SECOND_ROUTER, 0x1200, 0x1000, 0, 2),		Slotter(5, 24, 0, 3, 1, 2), "Router 1 Router 2");
		test_address(read_network_topology(obj, "Router 1 Router 2 End Device 1"),	Router(DeviceType::END_DEVICE, 0x1201, 0x1200, 0, 0),			Slotter(1, 24, 0), "Router 1 Router 2 End Device 1");
		test_address(read_network_topology(obj, "Router 1 Router 2 End Device 2"),	Router(DeviceType::END_DEVICE, 0x1202, 0x1200, 0, 0),			Slotter(2, 24, 0), "Router 1 Router 2 End Device 2");
		test_address(read_network_topology(obj, "Router 1 Router 1"),				Router(DeviceType::SECOND_ROUTER, 0x1100, 0x1000, 0, 1),		Slotter(4, 24, 0, 1, 0, 1), "Router 1 Router 1");
		test_address(read_network_topology(obj, "Router 1 Router 1 End Device 1"),	Router(DeviceType::END_DEVICE, 0x1101, 0x1100, 0, 0),			Slotter(0, 24, 0), "Router 1 Router 1 End Device 1");
		test_address(read_network_topology(obj, "Router 1 End Device 2"),			Router(DeviceType::END_DEVICE, 0x1002, 0x1000, 0, 0),			Slotter(9, 24, 0), "Router 1 End Device 2");
		test_address(read_network_topology(obj, "Router 1 End Device 1"),			Router(DeviceType::END_DEVICE, 0x1001, 0x1000, 0, 0),			Slotter(8, 24, 0), "Router 1 End Device 1");
		test_address(read_network_topology(obj, "Router 1"),						Router(DeviceType::FIRST_ROUTER, 0x1000, ADDR_COORD, 2, 3),		Slotter(13, 24, 0, 7, 4, 7), "Router 1");
		test_address(read_network_topology(obj, "End Device 1"),					Router(DeviceType::END_DEVICE, 0x0001, ADDR_COORD, 0, 0),		Slotter(23, 24, 0), "End Device 1");
		test_address(read_network_topology(obj, "BillyTheCoord"),					Router(DeviceType::COORDINATOR, ADDR_COORD, ADDR_NONE, 3, 1),	Slotter(SLOT_NONE, 24, 0, 0, 13, 11), "Coordinator");

		test_route(Router(DeviceType::COORDINATOR, ADDR_COORD, ADDR_NONE, 10, 10), 0xA201, 0xA000, "Coordinator -> 0xA201");
		test_route(Router(DeviceType::COORDINATOR, ADDR_COORD, ADDR_NONE, 10, 10), 0x0005, 0x0005, "Coordinator -> 0x0005");
		test_route(Router(DeviceType::FIRST_ROUTER, 0x1000, ADDR_COORD, 10, 10), 0x1A11, 0x1A00, "Router 1 -> 0x1A11");
		test_route(Router(DeviceType::FIRST_ROUTER, 0x1000, ADDR_COORD, 10, 10), 0x2211, ADDR_COORD, "Router 1 -> 0x2211");
		test_route(Router(DeviceType::FIRST_ROUTER, 0x1000, ADDR_COORD, 10, 10), 0x1001, 0x1001, "Router 1 -> 0x1001");
		test_route(Router(DeviceType::FIRST_ROUTER, 0x1000, ADDR_COORD, 10, 10), ADDR_COORD, ADDR_COORD, "Router 1 -> Coordinator");
		test_route(Router(DeviceType::FIRST_ROUTER, 0x1000, ADDR_COORD, 10, 10), 0x0011, ADDR_COORD, "Router 1 -> 0x0011");
		test_route(Router(DeviceType::SECOND_ROUTER, 0x1200, 0x1000, 10, 10), 0x1203, 0x1203, "Router 1 Router 2 -> 0x1203");
		test_route(Router(DeviceType::SECOND_ROUTER, 0x1200, 0x1000, 10, 10), 0x1311, 0x1000, "Router 1 Router 2 -> 0x1311");
		test_route(Router(DeviceType::SECOND_ROUTER, 0x1200, 0x1000, 10, 10), 0x2211, 0x1000, "Router 1 Router 2 -> 0x2211");
		test_route(Router(DeviceType::SECOND_ROUTER, 0x1200, 0x1000, 10, 10), ADDR_COORD, 0x1000, "Router 1 Router 2 -> Coordinator");
		test_route(Router(DeviceType::SECOND_ROUTER, 0x1200, 0x1000, 10, 10), 0x0011, 0x1000, "Router 1 Router 2 -> 0x0011");

		test_route(Router(DeviceType::SECOND_ROUTER, 0x1200, 0x1000, 10, 10), 0x1211, ADDR_ERROR, "Router 1 Router 2 -> 0x1211 Out of Bounds");
		test_route(Router(DeviceType::COORDINATOR, ADDR_COORD, ADDR_NONE, 10, 10), 0x0011, ADDR_ERROR, "Coordinator -> 0x0011 Out of Bounds");
		test_route(Router(DeviceType::COORDINATOR, ADDR_COORD, ADDR_NONE, 10, 10), 0xF201, ADDR_ERROR, "Coordinator -> 0xF201 Out of Bounds");
		test_route(Router(DeviceType::FIRST_ROUTER, 0x1000, ADDR_COORD, 10, 10), 0x1F11, ADDR_ERROR, "Router 1 -> 0x1F11 Out of Bounds");
		test_route(Router(DeviceType::FIRST_ROUTER, 0x1000, ADDR_COORD, 10, 10), 0x1011, ADDR_ERROR, "Router 1 -> 0x1011 Out of Bounds");
	}

	std::cout << "end testing JSON network parsing" << std::endl;

	std::cout << "begin testing Loom Network operation" << std::endl;
	{
		using namespace LoomNet;
		// run some simulations of the network!
		TestNetwork network(obj);

		// simulation one: loop for awhile, make sure nothing weird happens
		for (auto i = 0; i < 1000; i++) {
			if (!network.next_slot(false)) {
				std::cout << "Idle test failed!" << std::endl;
				return false;
			}
		}
		std::cout << "Idle test passed!" << std::endl;
		std::cout << "Begin single send test." << std::endl;
		network.reset();
		// simuation two: single send/recieve combination to every device
		// get past the refresh cycle first
		for (auto i = 0; i < 5; i++) {
			if (!network.next_slot(true)) {
				std::cout << "Single send prep failed" << std::endl;
				return false;
			}
		}
		// for every combination of addressi
		for (const auto src : all_addrs) {
			for (const auto dst : all_addrs) {
				if (src != dst) {
					char buf[16];
					snprintf(buf, sizeof(buf), "0x%04X->0x%04X", src, dst);
					if (!network.send_data_and_verify(src, dst, std::string(buf))) {
						std::cout << "Failed to send: " << src << "->" << dst << std::endl;
						return false;
					}
					// run until success, with six iterations of all end devices sending data
					for (auto i = 0; i < 6; i++) {
						for (auto& d : network.devices) {
							if (d.get_router().get_device_type() == LoomNet::DeviceType::END_DEVICE) {
								if (!network.send_data_and_verify(d.get_router().get_self_addr(), ADDR_COORD, std::string("use LOOM!"))) {
									std::cout << "Failed to send: " << d.get_router().get_self_addr() << "->" << ADDR_COORD << std::endl;
									return false;
								}
							}
						}
						// next cycle!
						network.next_cycle(true);
					}
					// run a few more times to clean out the residual packets
					for (auto i = 0; i < 4; i++) network.next_cycle();
					// check if all the packets made it
					if (network.pending_packet_count()) {
						std::cout << "Single send failed to clear network" << std::endl;
						return false;
					}
				}
			}
		}
		std::cout << "Single send test passed!" << std::endl;
		std::cout << "Begin no coordinator test:" << std::endl;
		network.reset();
		// simuation three: no coordinator, devices eventually just error out
		auto i = 0;
		for (; i < 1000; i++) {
			bool all_closed = true;
			for (auto& d : network.devices) {
				if (d.get_router().get_device_type() != DeviceType::COORDINATOR) {
					const auto status = d.net_update();
					if (status & TestNetwork::NetStatus::NET_SLEEP_RDY) {
						std::cout << "No coordinator test failed on address: 0x" << std::hex << std::setfill('0') << std::setw(4) << d.get_router().get_self_addr() << std::endl;
						return false;
					}
					all_closed &= (status == TestNetwork::NetStatus::NET_CLOSED);
				}
			}
			if (all_closed) {
				std::cout << "No coordinator test passed in " << std::dec << i << " slots" << std::endl;
				break;
			}
		}
		if (i == 1000) std::cout << "No coordinator test failed!" << std::endl;
		std::cout << "End no coordinator test" << std::endl;

		// simulation four: same as simulation two, but packets drop randomly inside the network
		std::cout << "Begin lossy single send test." << std::endl;
		network.reset();
		// get past the refresh cycle first
		for (auto i = 0; i < 5; i++) {
			if (!network.next_slot()) {
				std::cout << "Lossy single send prep failed" << std::endl;
				return false;
			}
		}
		// start your engines!
		network.enable_lossy_network(5);
		// for every combination of addressi
		for (const auto src : all_addrs) {
			for (const auto dst : all_addrs) {
				if (src != dst) {
					char buf[16];
					snprintf(buf, sizeof(buf), "0x%04X->%04X", src, dst);
					if (!network.send_data_and_verify(src, dst, std::string(buf))) {
						std::cout << "Failed to send: " << src << "->" << dst << std::endl;
						return false;
					}
					// run until success, with six iterations of all end devices sending data
					for (auto i = 0; i < 6; i++) {
						for (auto& d : network.devices) {
							if (d.get_router().get_device_type() == LoomNet::DeviceType::END_DEVICE) {
								if (!network.send_data_and_verify(d.get_router().get_self_addr(), ADDR_COORD, std::string("use LOOM!"))) {
									std::cout << "Failed to send: " << d.get_router().get_self_addr() << "->" << ADDR_COORD << std::endl;
									return false;
								}
							}
						}
						// next cycle!
						network.next_cycle(false);
					}
					// run a few more times to clean out the residual packets
					for (auto i = 0; i < 4; i++) network.next_cycle();
					// check if all the packets made it
					if (network.pending_packet_count()) {
						std::cout << "Lossy send failed to clear network" << std::endl;
						return false;
					}
				}
			}
		}
		std::cout << "Lossy single send test passed!" << std::endl;
	}

	std::cout << "end testing Loom Network operation" << std::endl;
}
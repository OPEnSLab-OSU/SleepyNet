// LoomNetworkSimulate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "../../../src/LoomNetwork.h"
#include "../../../src/CircularBuffer.h"
#include "../../../src/LoomRouter.h"
#include "../../../src/LoomNetworkConfig.h"
#include "../../../src/LoomNetworkInfo.h"
#include "TestNetwork.h"
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

// test every device sending to every device!
bool test_network_operation(TestNetwork& network, const std::array<uint16_t, 16>& all_addrs, const int drop_rate) {
	// get past the refresh cycle first
	for (auto i = 0; i < 5; i++) network.next_slot();
	// start your engines!
	network.set_drop_rate(drop_rate);
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
				// run until success, with twelve iterations of all end devices sending data
				for (auto i = 0; i < 12; i++) {
					for (auto& d : network.devices) {
						if (d.get_router().get_device_type() == LoomNet::DeviceType::END_DEVICE) {
							if (!network.send_data_and_verify(d.get_router().get_self_addr(), LoomNet::ADDR_COORD, std::string("use LOOM!"))) {
								std::cout << "Failed to send: " << d.get_router().get_self_addr() << "->" << LoomNet::ADDR_COORD << std::endl;
								return false;
							}
						}
					}
					// next cycle!
					network.next_cycle();
				}
				// run a few more times to clean out the residual packets
				auto i = 0;
				while (network.pending_packet_count() && i++ < 10) network.next_batch();
				// check if all the packets made it
				if (network.pending_packet_count()) {
					std::cout << "Lossy send failed to clear network" << std::endl;
					std::array<size_t, 16> has_data;
					for (auto i = 0; i < 16; i++) has_data[i] = network.devices[i].m_buffer_send.size();
					return false;
				}
				else std::cout << "Cleared the network in " << i << " cycles" << std::endl;
			}
		}
	}
	return true;
}

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
		std::cout << "begin idle test" << std::endl;
		{
			// run some simulations of the network!
			TestNetwork network(obj);

			// simulation one: loop for awhile, make sure nothing weird happens
			for (auto i = 0; i < 1000; i++) {
				network.next_slot();
				if (network.last_error != TestNetwork::Error::OK) {
					std::cout << "Idle test failed!" << std::endl;
					return false;
				}
			}
		}
		std::cout << "Idle test passed!" << std::endl;
		std::cout << "Begin single send test." << std::endl;
		// simuation two: single send/recieve combination to every device
		// get past the refresh cycle first
		{
			TestNetwork network(obj);

			if (!test_network_operation(network, all_addrs, 0)) return false;
		}
		std::cout << "Single send test passed!" << std::endl;
		std::cout << "Begin no coordinator test:" << std::endl;
		{
			TestNetwork network(obj);
			// simuation three: no coordinator, devices eventually just error out
			for (; network.cur_slot < 1000; network.cur_slot++) {
				bool all_closed = true;
				for (network.cur_loop = 0; network.cur_loop < LoomNet::LOOPS_PER_SLOT; network.cur_loop++) {
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
						std::cout << "No coordinator test passed in " << std::dec << network.cur_slot << " slots" << std::endl;
						break;
					}
				}
				if (all_closed) break;
			}
			if (network.cur_slot == 1000) std::cout << "No coordinator test failed!" << std::endl;
		}
		std::cout << "End no coordinator test" << std::endl;

		// simulation four: same as simulation two, but packets drop randomly inside the network
		std::cout << "Begin lossy single send test." << std::endl;
		{
			TestNetwork network(obj);

			if (!test_network_operation(network, all_addrs, 5)) return false;
		}
		std::cout << "Lossy single send test passed!" << std::endl;

		// simulation four: same as simulation two, but packets drop randomly inside the network
		std::cout << "Begin even lossyer single send test." << std::endl;
		{
			TestNetwork network(obj);

			if (!test_network_operation(network, all_addrs, 15)) return false;
		}
		std::cout << "Lossyer single send test passed!" << std::endl;
	}

	std::cout << "end testing Loom Network operation" << std::endl;
}
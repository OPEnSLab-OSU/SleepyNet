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
		// intitialize a bunch of network objects and a map to simulate network packets
		NetworkSim sim_net;
		std::array<Network<16,16>, 16> devices{{
			{ read_network_topology(obj, "Router 3 Router 1 End Device 1"), sim_net },
			{ read_network_topology(obj, "Router 3 Router 1"), sim_net },
			{ read_network_topology(obj, "Router 3"), sim_net },
			{ read_network_topology(obj, "Router 2 End Device 1"), sim_net },
			{ read_network_topology(obj, "Router 2"), sim_net },
			{ read_network_topology(obj, "Router 1 End Device 3"), sim_net },
			{ read_network_topology(obj, "Router 1 Router 2"), sim_net },
			{ read_network_topology(obj, "Router 1 Router 2 End Device 1"), sim_net },
			{ read_network_topology(obj, "Router 1 Router 2 End Device 2"), sim_net },
			{ read_network_topology(obj, "Router 1 Router 1"), sim_net },
			{ read_network_topology(obj, "Router 1 Router 1 End Device 1"), sim_net },
			{ read_network_topology(obj, "Router 1 End Device 2"), sim_net },
			{ read_network_topology(obj, "Router 1 End Device 1"), sim_net },
			{ read_network_topology(obj, "Router 1"), sim_net },
			{ read_network_topology(obj, "End Device 1"), sim_net },
			{ read_network_topology(obj, "BillyTheCoord"), sim_net }
		}};
		// prep the network simulator
		std::array<uint8_t, 255> airwaves{ 0 };
		bool busy = false;
		std::array<unsigned, 16> next_wake_times{ 0 };
		constexpr auto NET_ITER = 1000;
		using NetStatus = Network<16, 16>::Status;
		// every time any device writes to the network
		sim_net.set_net_write([&airwaves, &busy](std::array<uint8_t, 255> packet) {
			std::cout << "		Transmission from: 0x" << std::hex << std::setfill('0') << std::setw(4) << (static_cast<uint16_t>(packet[1]) | static_cast<uint16_t>(packet[2]) << 8) << std::endl;
			if (busy) {
				std::cout << "		Collision!" << std::endl;
			}
			// set the airwaves
			airwaves = packet;
			busy = true;
		});
		// every time any device reads from the network
		sim_net.set_net_read([&airwaves, &busy]() {
			// not busy anymore! we read already
			busy = false;
			return airwaves;
		});
		// iterate!
		for (uint16_t slot = 0; slot < NET_ITER; slot++) {
			// clear the network
			if (busy)
				std::cout << "Unread network packet!";
			airwaves.fill(0);
			std::cout << "Slot: " << std::dec << slot << std::endl;
			// wake all the devices that scheduled a wakeup now
			std::cout << "	Woke: ";
			for (uint8_t o = 0; o < devices.size(); o++) {
				if (slot > next_wake_times[o]) {
					next_wake_times[o]++;
					devices[o].net_sleep_wake_ack();
					std::cout << "0x" << std::hex << std::setfill('0') << std::setw(4) << devices[o].get_router().get_self_addr() << ", ";
				}
			}
			std::cout << std::endl;
			// iterate through each element until all of them are asleep, then move to the next slot
			bool all_sleep;
			auto iterations = 0;
			do {
				all_sleep = true;
				std::cout << "	Iteration " << iterations << ":" << std::endl;
				for (uint8_t i = 0; i < devices.size(); i++) {
					// if the device is awake
					const uint8_t status = devices[i].get_status();
					if (status == NetStatus::NET_CLOSED)
						std::cout << std::hex << devices[i].get_router().get_self_addr() << " closed!" << std::endl;
					// sleep check
					else if (!(status & NetStatus::NET_SLEEP_RDY)) {
						// TODO: Send/Recvieve data!
						// run the state machine
						const uint8_t new_status = devices[i].net_update();
						std::cout << "		Status of 0x" << std::hex << std::setfill('0') << std::setw(4) << devices[i].get_router().get_self_addr() << ": " << std::bitset<8>(devices[i].get_status()) << std::endl;
						// if it wants to go to sleep now, add the time to the wake times
						if (new_status & NetStatus::NET_SLEEP_RDY)
							next_wake_times[i] += devices[i].net_sleep_next_wake_time().time;
						else all_sleep = false;
					}
				}
				iterations++;
			} while (!all_sleep);
			std::cout << "	Took " << iterations << " iterations." << std::endl;
		}
	}

	std::cout << "end testing Loom Network operation" << std::endl;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

#include "pch.h"
#include "../../../src/LoomNetworkUtility.h"

using namespace LoomNet;

TEST(Utility, GetType) {
	// test coordinator
	EXPECT_EQ(get_type(ADDR_COORD), DeviceType::COORDINATOR);
	// test none,error
	EXPECT_EQ(get_type(ADDR_NONE), DeviceType::ERROR);
	EXPECT_EQ(get_type(ADDR_ERROR), DeviceType::ERROR);
	// for every first router
	for (uint16_t first = 1; first < 15; first++) {
		// test just the first router varient of the address
		const uint16_t first_addr = first << 12;
		EXPECT_EQ(get_type(first_addr), DeviceType::FIRST_ROUTER);
		// for every end device of the first router
		for (uint16_t end = 1; end < 254; end++)
			EXPECT_EQ(get_type(first_addr | end), DeviceType::END_DEVICE);
		// for every second router
		for (uint16_t second = 1; second < 15; second++) {
			// test just the second router varient
			const uint16_t second_addr = first_addr | (second << 8);
			EXPECT_EQ(get_type(second_addr), DeviceType::SECOND_ROUTER);
			// for every end device of the second router
			for (uint16_t end = 1; end < 254; end++)
				EXPECT_EQ(get_type(second_addr | end), DeviceType::END_DEVICE);
		}
	}
}

TEST(Utility, GetParent) {
	// test coordinator
	EXPECT_EQ(get_parent(ADDR_COORD, DeviceType::COORDINATOR), ADDR_NONE);
	// test none, error
	EXPECT_EQ(get_parent(ADDR_NONE, DeviceType::ERROR), ADDR_ERROR);
	EXPECT_EQ(get_parent(ADDR_ERROR, DeviceType::ERROR), ADDR_ERROR);
	// for every first router
	for (uint16_t first = 1; first < 15; first++) {
		// test just the first router varient of the address
		const uint16_t first_addr = first << 12;
		EXPECT_EQ(get_parent(first_addr, DeviceType::FIRST_ROUTER), ADDR_COORD);
		// for every end device of the first router
		for (uint16_t end = 1; end < 254; end++)
			EXPECT_EQ(get_parent(first_addr | end, DeviceType::END_DEVICE), first_addr);
		// for every second router
		for (uint16_t second = 1; second < 15; second++) {
			// test just the second router varient
			const uint16_t second_addr = first_addr | (second << 8);
			EXPECT_EQ(get_parent(second_addr, DeviceType::SECOND_ROUTER), first_addr);
			// for every end device of the second router
			for (uint16_t end = 1; end < 254; end++)
				EXPECT_EQ(get_parent(second_addr | end, DeviceType::END_DEVICE), second_addr);
		}
	}
}
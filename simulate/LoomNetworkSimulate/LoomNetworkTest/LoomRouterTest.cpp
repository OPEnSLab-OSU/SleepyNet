#include "pch.h"
#include "../../../src/LoomRouter.h"

using namespace LoomNet;

TEST(Router, FromEndDeviceOne) {
	Router router(0x0001, 0, 0);
	ASSERT_EQ(router.route(0x0001), ADDR_ERROR);
	ASSERT_EQ(router.route(ADDR_COORD), ADDR_COORD);
	ASSERT_EQ(router.route(0x1001), ADDR_COORD);
	ASSERT_EQ(router.route(0x3101), ADDR_COORD);
}

TEST(Router, FromRouterOne) {
	Router router(0x1000, 2, 2);
	ASSERT_EQ(router.route(0x1000), ADDR_ERROR);
	ASSERT_EQ(router.route(0x1300), ADDR_ERROR);
	ASSERT_EQ(router.route(0x1003), ADDR_ERROR);
	ASSERT_EQ(router.route(ADDR_COORD), ADDR_COORD);
	ASSERT_EQ(router.route(0x1001), 0x1001);
	ASSERT_EQ(router.route(0x1002), 0x1002);
	ASSERT_EQ(router.route(0x1100), 0x1100);
	ASSERT_EQ(router.route(0x1101), 0x1100);
	ASSERT_EQ(router.route(0x1200), 0x1200);
	ASSERT_EQ(router.route(0x1201), 0x1200);
	ASSERT_EQ(router.route(0x2000), ADDR_COORD);
	ASSERT_EQ(router.route(0x2001), ADDR_COORD);
}

TEST(Router, FromRouterOneRouterOne) {
	Router router(0x1100, 0, 2);
	ASSERT_EQ(router.route(0x1100), ADDR_ERROR);
	ASSERT_EQ(router.route(0x1103), ADDR_ERROR);
	ASSERT_EQ(router.route(0x1000), 0x1000);
	ASSERT_EQ(router.route(0x1001), 0x1000);
	ASSERT_EQ(router.route(0x1002), 0x1000);
	ASSERT_EQ(router.route(0x1101), 0x1101);
	ASSERT_EQ(router.route(0x1200), 0x1000);
	ASSERT_EQ(router.route(ADDR_COORD), 0x1000);
}

TEST(Router, ErrorConfig) {
	ASSERT_EQ(ROUTER_ERROR.route(ADDR_COORD), ADDR_ERROR);
}
#include "pch.h"
#include "../../../src/LoomNetworkConfig.h"

using namespace LoomNet;

class ConfigFixtureBase : public ::testing::Test {
public:
	ConfigFixtureBase()
		: ::testing::Test()
		, m_doc(4069) {}

protected:

	virtual const char* get_json() const { return nullptr; };
	
	void SetUp() override {
		// serialize the JSON into the document
		deserializeJson(m_doc, get_json());
	}

	void test_config(const char* const name, const Router& truth_router, const Slotter& truth_slotter, const Drift& truth_drifer) const {
		const NetworkInfo cfg = read_network_topology(m_doc.as<JsonObjectConst>(), name);
		EXPECT_EQ(cfg.route_info, truth_router);
		EXPECT_EQ(cfg.slot_info, truth_slotter);
		EXPECT_EQ(cfg.drift_info, truth_drifer);
	}

	DynamicJsonDocument m_doc;
};

class SimpleConfigFixture : public ConfigFixtureBase {
protected:
	const static char config[];

	const Drift drift_truth{
		TimeInterval(TimeInterval::SECOND, 3),
		TimeInterval(TimeInterval::SECOND, 10),
		TimeInterval(TimeInterval::SECOND, 10),
	};

	const char* get_json() const override {
		return config;
	}
};

const char SimpleConfigFixture::config[] = "\
{\
\"config\":{\
	\"cycles_per_batch\":2,\
	\"cycle_gap\":1,\
	\"batch_gap\":1,\
	\"slot_length\":{\
		\"unit\":\"SECOND\",\
		\"time\":10\
	},\
	\"max_drift\":{\
		\"unit\":\"SECOND\",\
		\"time\":10\
	},\
	\"min_drift\":{\
		\"unit\":\"SECOND\",\
		\"time\":3\
	}\
},\
\"root\":{\
	\"name\":\"Coordinator\",\
	\"children\":[\
		{\
			\"name\":\"End Device 1\",\
			\"type\":0,\
			\"addr\":\"0x0001\"\
		},\
		{\
			\"name\":\"End Device 2\",\
			\"type\":0,\
			\"addr\":\"0x0002\"\
		},\
		{\
			\"name\":\"End Device 3\",\
			\"type\":0,\
			\"addr\":\"0x0003\"\
		},\
		{\
			\"name\":\"End Device 4\",\
			\"type\":0,\
			\"addr\":\"0x0004\"\
		}\
	]\
}}";

TEST_F(SimpleConfigFixture, Coordinator) {
	test_config("Coordinator",
		Router(DeviceType::COORDINATOR, ADDR_COORD, ADDR_NONE, 0, 4),
		Slotter(SLOT_NONE, 4, 2, 1, 1, 0, 0, 4),
		drift_truth);
}

TEST_F(SimpleConfigFixture, EndDevice1) {
	test_config("End Device 1",
		Router(DeviceType::END_DEVICE, 0x0001, ADDR_COORD, 0, 0),
		Slotter(0, 4, 2, 1, 1),
		drift_truth);
}

TEST_F(SimpleConfigFixture, EndDevice2) {
	test_config("End Device 2",
		Router(DeviceType::END_DEVICE, 0x0002, ADDR_COORD, 0, 0),
		Slotter(1, 4, 2, 1, 1),
		drift_truth);
}

TEST_F(SimpleConfigFixture, EndDevice3) {
	test_config("End Device 3",
		Router(DeviceType::END_DEVICE, 0x0003, ADDR_COORD, 0, 0),
		Slotter(2, 4, 2, 1, 1),
		drift_truth);
}

TEST_F(SimpleConfigFixture, EndDevice4) {
	test_config("End Device 4",
		Router(DeviceType::END_DEVICE, 0x0004, ADDR_COORD, 0, 0),
		Slotter(3, 4, 2, 1, 1),
		drift_truth);
}

TEST_F(SimpleConfigFixture, Error) {
	test_config("Error",
		ROUTER_ERROR,
		SLOTTER_ERROR,
		DRIFT_ERROR);
}

class DeepConfigFixture : public ConfigFixtureBase {
protected:
	const static char config[];

	const Drift drift_truth{
		TimeInterval(TimeInterval::SECOND, 3),
		TimeInterval(TimeInterval::SECOND, 10),
		TimeInterval(TimeInterval::SECOND, 10),
	};

	const char* get_json() const override {
		return config;
	}
};

const char DeepConfigFixture::config[] = "{\
\"config\":{\
	\"cycles_per_batch\":2,\
	\"cycle_gap\":1,\
	\"batch_gap\":1,\
	\"slot_length\":{\
		\"unit\":\"SECOND\",\
		\"time\":10\
	},\
	\"max_drift\":{\
		\"unit\":\"SECOND\",\
		\"time\":10\
	},\
	\"min_drift\":{\
		\"unit\":\"SECOND\",\
		\"time\":3\
	}\
},\
\"root\":{\
	\"name\":\"Coord\",\
	\"children\":[\
		{\
			\"name\":\"Device 1\",\
			\"sensor\":false,\
			\"type\":1,\
			\"children\":[\
				{\
					\"name\":\"Device 2\",\
					\"type\":1,\
					\"sensor\":false,\
					\"children\":[\
						{\
							\"name\":\"Device 3\",\
							\"type\":0\
						}\
					]\
				},\
				{\
					\"name\":\"Device 4\",\
					\"type\":0\
				}\
			]\
		}\
	]\
}}";

TEST_F(DeepConfigFixture, Coordinator) {
	test_config("Coord",
		Router(DeviceType::COORDINATOR, ADDR_COORD, ADDR_NONE, 1, 0),
		Slotter(SLOT_NONE, 5, 2, 1, 1, 0, 3, 2),
		drift_truth);
}

TEST_F(DeepConfigFixture, Device1) {
	test_config("Device 1",
		Router(DeviceType::FIRST_ROUTER, 0x1000, ADDR_COORD, 1, 1),
		Slotter(3, 5, 2, 1, 1, 2, 1, 2),
		drift_truth);
}

TEST_F(DeepConfigFixture, Device2) {
	test_config("Device 2",
		Router(DeviceType::SECOND_ROUTER, 0x1100, 0x1000, 0, 1),
		Slotter(1, 5, 2, 1, 1, 1, 0, 1),
		drift_truth);
}

TEST_F(DeepConfigFixture, Device3) {
	test_config("Device 3",
		Router(DeviceType::END_DEVICE, 0x1101, 0x1100, 0, 0),
		Slotter(0, 5, 2, 1, 1),
		drift_truth);
}

TEST_F(DeepConfigFixture, Device4) {
	test_config("Device 4",
		Router(DeviceType::END_DEVICE, 0x1001, 0x1000, 0, 0),
		Slotter(2, 5, 2, 1, 1),
		drift_truth);
}

class FullConfigFixture : public ConfigFixtureBase {
protected:
	const static char config[];

	const Drift drift_truth{
		TimeInterval(TimeInterval::SECOND, 3),
		TimeInterval(TimeInterval::SECOND, 10),
		TimeInterval(TimeInterval::SECOND, 10),
	};

	const char* get_json() const override {
		return config;
	}
};

const char FullConfigFixture::config[] = "\
{\
\"config\":{\
	\"cycles_per_batch\":2,\
	\"cycle_gap\":1,\
	\"batch_gap\":1,\
	\"slot_length\":{\
		\"unit\":\"SECOND\",\
		\"time\":10\
	},\
	\"max_drift\":{\
		\"unit\":\"SECOND\",\
		\"time\":10\
	},\
	\"min_drift\":{\
		\"unit\":\"SECOND\",\
		\"time\":3\
	}\
},\
\"root\":{\
	\"name\":\"Coordinator\",\
	\"sensor\":false,\
	\"children\":[\
		{\
			\"name\":\"End Device 1\",\
			\"type\":0,\
			\"addr\":\"0x001\"\
		},\
		{\
			\"name\":\"Router 1\",\
			\"sensor\":false,\
			\"type\":1,\
			\"addr\":\"0x1000\",\
			\"children\":[\
				{\
					\"name\":\"Router 1 End Device 1\",\
					\"type\":0,\
					\"addr\":\"0x1001\"\
				},\
				{\
					\"name\":\"Router 1 End Device 2\",\
					\"type\":0,\
					\"addr\":\"0x1002\"\
				},\
				{\
					\"name\":\"Router 1 Router 1\",\
					\"sensor\":false,\
					\"type\":1,\
					\"addr\":\"0x1100\",\
					\"children\":[\
						{\
							\"name\":\"Router 1 Router 1 End Device 1\",\
							\"type\":0,\
							\"addr\":\"0x1101\"\
						}\
					]\
				},\
				{\
					\"name\":\"Router 1 Router 2\",\
					\"sensor\":true,\
					\"type\":1,\
					\"addr\":\"0x1200\",\
					\"children\":[\
						{\
							\"name\":\"Router 1 Router 2 End Device 1\",\
							\"type\":0,\
							\"addr\":\"0x1001\"\
						},\
						{\
							\"name\":\"Router 1 Router 2 End Device 2\",\
							\"type\":0,\
							\"addr\":\"0x1202\"\
						}\
					]\
				},\
				{\
					\"name\":\"Router 1 End Device 3\",\
					\"type\":0,\
					\"addr\":\"0x1003\"\
				}\
			]\
		},\
		{\
			\"name\":\"Router 2\",\
			\"sensor\":true,\
			\"type\":1,\
			\"addr\":\"0x2000\",\
			\"children\":[\
				{\
					\"name\":\"Router 2 End Device 1\",\
					\"type\":0,\
					\"addr\":\"0x2001\"\
				}\
			]\
		},\
		{\
			\"name\":\"Router 3\",\
			\"sensor\":false,\
			\"type\":1,\
			\"addr\":\"0x3000\",\
			\"children\":[\
				{\
					\"name\":\"Router 3 Router 1\",\
					\"sensor\":false,\
					\"type\":1,\
					\"addr\":\"0x3100\",\
					\"children\":[\
						{\
							\"name\":\"Router 3 Router 1 End Device 1\",\
							\"type\":0,\
							\"addr\":\"0x3101\"\
						}\
					]\
				}\
			]\
		}\
	]\
}}";

TEST_F(FullConfigFixture, Router3Router1EndDevice1) {
	test_config("Router 3 Router 1 End Device 1",
		Router(DeviceType::END_DEVICE, 0x3101, 0x3100, 0, 0),
		Slotter(3, 24, 2, 1, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router3Router1) {
	test_config("Router 3 Router 1",
		Router(DeviceType::SECOND_ROUTER, 0x3100, 0x3000, 0, 1),
		Slotter(12, 24, 2, 1, 1, 1, 3, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router3) {
	test_config("Router 3",
		Router(DeviceType::FIRST_ROUTER, 0x3000, ADDR_COORD, 1, 0),
		Slotter(22, 24, 2, 1, 1, 1, 12, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router2EndDevice1) {
	test_config("Router 2 End Device 1",
		Router(DeviceType::END_DEVICE, 0x2001, 0x2000, 0, 0),
		Slotter(11, 24, 2, 1, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router2) {
	test_config("Router 2",
		Router(DeviceType::FIRST_ROUTER, 0x2000, ADDR_COORD, 0, 1),
		Slotter(20, 24, 2, 1, 1, 2, 11, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router1EndDevice3) {
	test_config("Router 1 End Device 3",
		Router(DeviceType::END_DEVICE, 0x1003, 0x1000, 0, 0),
		Slotter(10, 24, 2, 1, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router1Router2) {
	test_config("Router 1 Router 2",
		Router(DeviceType::SECOND_ROUTER, 0x1200, 0x1000, 0, 2),
		Slotter(5, 24, 2, 1, 1, 3, 1, 2),
		drift_truth);
}

TEST_F(FullConfigFixture, Router1Router2EndDevice1) {
	test_config("Router 1 Router 2 End Device 1",
		Router(DeviceType::END_DEVICE, 0x1201, 0x1200, 0, 0),
		Slotter(1, 24, 2, 1, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router1Router2EndDevice2) {
	test_config("Router 1 Router 2 End Device 2",
		Router(DeviceType::END_DEVICE, 0x1202, 0x1200, 0, 0),
		Slotter(2, 24, 2, 1, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router1Router1) {
	test_config("Router 1 Router 1",
		Router(DeviceType::SECOND_ROUTER, 0x1100, 0x1000, 0, 1),
		Slotter(4, 24, 2, 1, 1, 1, 0, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router1Router1EndDevice1) {
	test_config("Router 1 Router 1 End Device 1",
		Router(DeviceType::END_DEVICE, 0x1101, 0x1100, 0, 0),
		Slotter(0, 24, 2, 1, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router1EndDevice2) {
	test_config("Router 1 End Device 2",
		Router(DeviceType::END_DEVICE, 0x1002, 0x1000, 0, 0),
		Slotter(9, 24, 2, 1, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router1EndDevice1) {
	test_config("Router 1 End Device 1",
		Router(DeviceType::END_DEVICE, 0x1001, 0x1000, 0, 0),
		Slotter(8, 24, 2, 1, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Router1) {
	test_config("Router 1",
		Router(DeviceType::FIRST_ROUTER, 0x1000, ADDR_COORD, 2, 3),
		Slotter(13, 24, 2, 1, 1, 7, 4, 7),
		drift_truth);
}

TEST_F(FullConfigFixture, EndDevice1) {
	test_config("End Device 1",
		Router(DeviceType::END_DEVICE, 0x0001, ADDR_COORD, 0, 0),
		Slotter(23, 24, 2, 1, 1),
		drift_truth);
}

TEST_F(FullConfigFixture, Coordinator) {
	test_config("Coordinator",
		Router(DeviceType::COORDINATOR, ADDR_COORD, ADDR_NONE, 3, 1),
		Slotter(SLOT_NONE, 24, 2, 1, 1, 0, 13, 11),
		drift_truth);
}

TEST_F(FullConfigFixture, Error) {
	test_config("Error",
		ROUTER_ERROR,
		SLOTTER_ERROR,
		DRIFT_ERROR);
}
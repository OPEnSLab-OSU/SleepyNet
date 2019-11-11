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

	void test_config(const char* const name, const NetworkConfig& truth) const {
		const NetworkConfig& cfg = read_network_topology(m_doc.as<JsonObjectConst>(), name);
		EXPECT_EQ(cfg.send_slot, truth.send_slot);
		EXPECT_EQ(cfg.total_slots, truth.total_slots);
		EXPECT_EQ(cfg.cycles_per_refresh, truth.cycles_per_refresh);
		EXPECT_EQ(cfg.cycle_gap, truth.cycle_gap);
		EXPECT_EQ(cfg.batch_gap, truth.batch_gap);
		EXPECT_EQ(cfg.send_count, truth.send_count);
		EXPECT_EQ(cfg.recv_slot, truth.recv_slot);
		EXPECT_EQ(cfg.recv_count, truth.recv_count);
		EXPECT_EQ(cfg.self_addr, truth.self_addr);
		EXPECT_EQ(cfg.child_router_count, truth.child_router_count);
		EXPECT_EQ(cfg.child_node_count, truth.child_node_count);
		EXPECT_EQ(cfg.min_drift, truth.min_drift);
		EXPECT_EQ(cfg.max_drift, truth.max_drift);
		EXPECT_EQ(cfg.slot_length, truth.slot_length);
	}

	DynamicJsonDocument m_doc;
};

class SimpleConfigFixture : public ConfigFixtureBase {
protected:
	const static char config[];

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
	test_config("Coordinator", { 
		SLOT_NONE, 4, 2, 1, 1, 0, 0, 4,
		ADDR_COORD, 0, 4,
		TimeInterval(TimeInterval::SECOND, 3),
		TimeInterval(TimeInterval::SECOND, 10),
		TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(SimpleConfigFixture, EndDevice1) {
	test_config("End Device 1", {
			0, 4, 2, 1, 1, 1, SLOT_NONE, 0,
			0x0001, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(SimpleConfigFixture, EndDevice2) {
	test_config("End Device 2", {
			1, 4, 2, 1, 1, 1, SLOT_NONE, 0,
			0x0002, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(SimpleConfigFixture, EndDevice3) {
	test_config("End Device 3", {
			2, 4, 2, 1, 1, 1, SLOT_NONE, 0,
			0x0003, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(SimpleConfigFixture, EndDevice4) {
	test_config("End Device 4", {
			3, 4, 2, 1, 1, 1, SLOT_NONE, 0,
			0x0004, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(SimpleConfigFixture, Error) {
	test_config("Error", NETWORK_ERROR);
}

class DeepConfigFixture : public ConfigFixtureBase {
protected:
	const static char config[];

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
	test_config("Coord", {
			SLOT_NONE, 5, 2, 1, 1, 0, 3, 2,
			ADDR_COORD, 1, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(DeepConfigFixture, Device1) {
	test_config("Device 1", {
			3, 5, 2, 1, 1, 2, 1, 2,
			0x1000, 1, 1,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(DeepConfigFixture, Device2) {
	test_config("Device 2", {
		1, 5, 2, 1, 1, 1, 0, 1,
		0x1100, 0, 1,
		TimeInterval(TimeInterval::SECOND, 3),
		TimeInterval(TimeInterval::SECOND, 10),
		TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(DeepConfigFixture, Device3) {
	test_config("Device 3", {
			0, 5, 2, 1, 1, 1, SLOT_NONE, 0,
			0x1101, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(DeepConfigFixture, Device4) {
	test_config("Device 4", {
			2, 5, 2, 1, 1, 1, SLOT_NONE, 0,
			0x1001, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

class FullConfigFixture : public ConfigFixtureBase {
protected:
	const static char config[];

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
	test_config("Router 3 Router 1 End Device 1", {
			3, 24, 2, 1, 1, 1, SLOT_NONE, 0,
			0x3101, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router3Router1) {
	test_config("Router 3 Router 1", {
			12, 24, 2, 1, 1, 1, 3, 1,
			0x3100, 0, 1,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router3) {
	test_config("Router 3", {
			22, 24, 2, 1, 1, 1, 12, 1,
			0x3000, 1, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router2EndDevice1) {
	test_config("Router 2 End Device 1", {
			11, 24, 2, 1, 1, 1, SLOT_NONE, 0,
			0x2001, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router2) {
	test_config("Router 2", {
			20, 24, 2, 1, 1, 2, 11, 1,
			0x2000, 0, 1,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router1EndDevice3) {
	test_config("Router 1 End Device 3", {
			10, 24, 2, 1, 1, 1, SLOT_NONE, 0,
			0x1003, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router1Router2) {
	test_config("Router 1 Router 2", {
			5, 24, 2, 1, 1, 3, 1, 2,
			0x1200, 0, 2,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router1Router2EndDevice1) {
	test_config("Router 1 Router 2 End Device 1", {
			1, 24, 2, 1, 1, 1, SLOT_NONE, 0,
			0x1201, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router1Router2EndDevice2) {
	test_config("Router 1 Router 2 End Device 2", {
			2, 24, 2, 1, 1, 1, SLOT_NONE, 0,
			0x1202, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router1Router1) {
	test_config("Router 1 Router 1", {
			4, 24, 2, 1, 1, 1, 0, 1,
			0x1100, 0, 1,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router1Router1EndDevice1) {
	test_config("Router 1 Router 1 End Device 1", {
			0, 24, 2, 1, 1, 1, SLOT_NONE, 0,
			0x1101, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router1EndDevice2) {
	test_config("Router 1 End Device 2", {
			9, 24, 2, 1, 1, 1, SLOT_NONE, 0,
			0x1002, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router1EndDevice1) {
	test_config("Router 1 End Device 1", {
			8, 24, 2, 1, 1, 1, SLOT_NONE, 0,
			0x1001, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Router1) {
	test_config("Router 1", {
			13, 24, 2, 1, 1, 7, 4, 7,
			0x1000, 2, 3,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, EndDevice1) {
	test_config("End Device 1", {
			23, 24, 2, 1, 1, 1, SLOT_NONE, 0,
			0x0001, 0, 0,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Coordinator) {
	test_config("Coordinator", {
			SLOT_NONE, 24, 2, 1, 1, 0, 13, 11,
			ADDR_COORD, 3, 1,
			TimeInterval(TimeInterval::SECOND, 3),
			TimeInterval(TimeInterval::SECOND, 10),
			TimeInterval(TimeInterval::SECOND, 10) });
}

TEST_F(FullConfigFixture, Error) {
	test_config("Error", NETWORK_ERROR);
}
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

class FullConfigFixture : public ConfigFixtureBase {
protected:
	constexpr static char config[] = "\
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
	\"name\":\"BillyTheCoord\",\
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

	const Drift drift_truth{
		TimeInterval(TimeInterval::SECOND, 3),
		TimeInterval(TimeInterval::SECOND, 10),
		TimeInterval(TimeInterval::SECOND, 10),
	};

	const char* get_json() const override {
		return config;
	}
};



TEST_F(FullConfigFixture, Router3Router1EndDevice1) {
	test_config("Router 3 Router 1 End Device 1",
		Router(DeviceType::END_DEVICE, 0x3101, 0x3100, 0, 0),
		Slotter(3, 24, 2, 1, 1),
		drift_truth);
}

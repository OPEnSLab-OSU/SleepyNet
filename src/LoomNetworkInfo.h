#pragma once

#include "LoomRouter.h"
#include "LoomSlotter.h"
#include "LoomNetworkUtility.h"
/** Simple container class for information extracted from a network configuration */

namespace LoomNet {
	struct Drift {
		const TimeInterval min_drift;
		const TimeInterval max_drift;
		const TimeInterval slot_length;
	};

	const Drift DRIFT_ERROR = { TIME_NONE, TIME_NONE, TIME_NONE };

	struct NetworkInfo {
		const Router route_info;
		const Slotter slot_info;
		const Drift	drift_info;
	};

	const NetworkInfo NETWORK_ERROR = { ROUTER_ERROR, SLOTTER_ERROR, DRIFT_ERROR };
}

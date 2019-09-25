#pragma once

#include "LoomRouter.h"
#include "LoomSlotter.h"
#include "LoomNetworkUtility.h"
/** Simple container class for information extracted from a network configuration */

namespace LoomNet {
	struct Drift {
		Drift(const TimeInterval& min_drift_prop,
			const TimeInterval& max_drift_prop,
			const TimeInterval& slot_length_prop)
			: min_drift(min_drift_prop)
			, max_drift(max_drift_prop)
			, slot_length(slot_length_prop) {}

		bool operator==(const Drift& rhs) const {
			return (min_drift == rhs.min_drift)
				&& (max_drift == rhs.max_drift)
				&& (slot_length == rhs.slot_length);
		}

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

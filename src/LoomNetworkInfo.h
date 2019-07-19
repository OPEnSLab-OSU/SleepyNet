#pragma once

#include "LoomRouter.h"
#include "LoomSlotter.h"
/** Simple container class for information extracted from a network configuration */

namespace LoomNet {
	struct NetworkInfo {
		Router route_info;
		Slotter slot_info;
	};
}

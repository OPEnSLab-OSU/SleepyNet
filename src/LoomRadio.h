#pragma once

#include "LoomNetworkFragment.h"
#include "LoomNetworkUtility.h"

/** 
 * An interface specifying a generic radio driver for Loom network
 */

namespace LoomNet {

	class Radio {
	public:
		enum class State {
			DISABLED,
			SLEEP,
			IDLE,
			ERROR,
		};

		// the radio needs to keep time, as does the rest of the network
		virtual TimeInterval get_time() = 0;
		// get the radio state
		virtual State get_state() const = 0;
		// initialize and configure the radio
		// called once on initial startup
		// the radio must start in sleep mode
		virtual void enable() = 0;
		// deinitialize and disable the radio
		// called once on failure
		virtual void disable() = 0;
		// put the radio into powersaving mode, disabling any airwaves
		// called after any radio activity
		virtual void sleep() = 0;
		// wake the radio up from sleep mode
		// called before any radio activity
		virtual void wake() = 0;

		// once the radio is woke, any of the below functions can be called
		// all operations are atomic and simply delay until they are complete
		virtual Packet recv() = 0;
		virtual void send(const Packet& send) = 0;
	};

};
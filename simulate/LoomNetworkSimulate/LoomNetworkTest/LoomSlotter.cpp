#include "pch.h"

namespace LoomNetTest {

	using namespace LoomNet;
	using State = Slotter::State;
	using Expected = std::pair<State, uint8_t>;

	TEST(Slotter, SlotsPerRefresh) {
		// 2 cycles per refresh, 1 slot per gap
		Slotter slotter(9, 24, 2, 1, 1); // generic end device

		ASSERT_EQ(slotter.get_slots_per_refresh(), 55);
	}

	TEST(Slotter, EndDevice) {
		Slotter slotter(9, 24, 2, 1, 1); // generic end device

		std::array<Expected, 4> answers{ {
			{ State::SLOT_WAIT_REFRESH, 0 },
			{ State::SLOT_SEND_W_SYNC, 9 },
			{ State::SLOT_SEND, 24 },
			{ State::SLOT_WAIT_REFRESH, 0 },
		} };

		int i = 1;
		for (const Expected& ex : answers) {
			EXPECT_EQ(ex.first, slotter.get_state()) << "state did not match on iteration " << i;
			EXPECT_EQ(ex.second, slotter.get_slot_wait()) << "wait did not match on iteration " << i;
			slotter.next_state();
			i++;
		}
	}
	TEST(Slotter, Router) {
		Slotter slotter(13, 24, 2, 1, 1, 7, 4, 7); // 0x1000 in example JSON

		std::array<Expected, 30> answers{ {
			{ State::SLOT_WAIT_REFRESH, 0 },
			// recv from all children
			{ State::SLOT_RECV_W_SYNC, 4 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			// send all that data to the coordinator
			{ State::SLOT_SEND, 2 },
			{ State::SLOT_SEND, 0 },
			{ State::SLOT_SEND, 0 },
			{ State::SLOT_SEND, 0 },
			{ State::SLOT_SEND, 0 },
			{ State::SLOT_SEND, 0 },
			{ State::SLOT_SEND, 0 },

			{ State::SLOT_RECV, 9 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },

			{ State::SLOT_SEND, 2 },
			{ State::SLOT_SEND, 0 },
			{ State::SLOT_SEND, 0 },
			{ State::SLOT_SEND, 0 },
			{ State::SLOT_SEND, 0 },
			{ State::SLOT_SEND, 0 },
			{ State::SLOT_SEND, 0 },

			{ State::SLOT_WAIT_REFRESH, 0 }
		} };

		int i = 1;
		for (const Expected& ex : answers) {
			EXPECT_EQ(ex.first, slotter.get_state()) << "state did not match on iteration " << i;
			EXPECT_EQ(ex.second, slotter.get_slot_wait()) << "wait did not match on iteration " << i;
			slotter.next_state();
			i++;
		}
	}

	TEST(Slotter, Coordinator) {
		Slotter slotter(SLOT_NONE, 24, 2, 1, 1, 0, 13, 11); // 0xF000 in example JSON

		std::array<Expected, 24> answers{ {
			{ State::SLOT_WAIT_REFRESH, 0 },
			// recv from all children
			{ State::SLOT_RECV_W_SYNC, 13 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			// do it again
			{ State::SLOT_RECV, 14 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },
			{ State::SLOT_RECV, 0 },

			{ State::SLOT_WAIT_REFRESH, 0 }
		} };

		int i = 1;
		for (const Expected& ex : answers) {
			EXPECT_EQ(ex.first, slotter.get_state()) << "state did not match on iteration " << i;
			EXPECT_EQ(ex.second, slotter.get_slot_wait()) << "wait did not match on iteration " << i;
			slotter.next_state();
			i++;
		}
	}
}
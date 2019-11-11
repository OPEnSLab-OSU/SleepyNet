#pragma once
#include <stdint.h>
#include "LoomNetworkPacket.h"

/**
 * Loom network packet sorter
 * Performs a few functions:
 * * Takes a data stream and converts it into packets
 * * Takes a stream of packets and converts it into a data stream
 * * Removes duplicate packets (using fingerprints to determine duplicates)
 */

namespace LoomNet {

	class PacketSorter {

		size_t write(const uint8_t* data, const size_t len);
		bool write(const DataPacket& packet);

		size_t read(uint8_t* dest, const size_t len);
		
		bool get_packet(Packet& dest, const uint16_t send_addr);
		
		size_t data_available() const;
		size_t packets_available() const;
	};

}
#include "pch.h"
#include "../../../src/LoomNetworkPacket.h"

using namespace LoomNet;

TEST(LoomPacket, BasePacket) {
	const Packet test(PacketCtrl::NONE, 0xBEEF);

	EXPECT_EQ(test.get_control(), PacketCtrl::NONE);
	EXPECT_EQ(test.get_src(), 0xBEEF);
}

TEST(LoomPacket, BasePacketRaw) {
	constexpr uint8_t raw[] = { 0x00, 0xEF, 0xBE };
	const Packet test(raw, sizeof(raw));

	EXPECT_EQ(test.get_control(), PacketCtrl::NONE);
	EXPECT_EQ(test.get_src(), 0xBEEF);
}

TEST(LoomPacket, DataPacket) {
	constexpr uint8_t payload[] = "Hello!";
	const Packet test = DataPacket::Factory(0xDEAD, 0xBEEF, 0xFEEF, 1, 2, payload, sizeof(payload));
	const DataPacket& data = test.as<DataPacket>();

	EXPECT_EQ(data.get_control(), PacketCtrl::DATA_TRANS);
	EXPECT_EQ(data.get_dst(), 0xDEAD);
	EXPECT_EQ(data.get_src(), 0xBEEF);
	EXPECT_EQ(data.get_orig_src(), 0xFEEF);
	EXPECT_EQ(data.get_rolling_id(), 1);
	EXPECT_EQ(data.get_seq(), 2);
	EXPECT_EQ(data.get_payload_length(), 7);
	EXPECT_STREQ(reinterpret_cast<const char*>(data.get_payload()), reinterpret_cast<const char*>(payload));
}

TEST(LoomPacket, DataPacketRaw) {
	constexpr uint8_t raw[] = { 0x05, 0xEF, 0xBE, 15, 0xAD, 0xDE, 0xEF, 0xFE, 0x01, 0x02, 0x00, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x21, 0x00, 0x00, 0x00 };
	const Packet test(raw, sizeof(raw));
	const DataPacket& data = test.as<DataPacket>();

	EXPECT_EQ(data.get_control(), PacketCtrl::DATA_TRANS);
	EXPECT_EQ(data.get_dst(), 0xDEAD);
	EXPECT_EQ(data.get_src(), 0xBEEF);
	EXPECT_EQ(data.get_orig_src(), 0xFEEF);
	EXPECT_EQ(data.get_rolling_id(), 1);
	EXPECT_EQ(data.get_seq(), 2);
	EXPECT_EQ(data.get_payload_length(), 7);
	EXPECT_STREQ(reinterpret_cast<const char*>(data.get_payload()), "Hello!");
}

TEST(LoomPacket, RefreshPacket) {
	const Packet test = RefreshPacket::Factory(0xDEAD, 
		TimeInterval(TimeInterval::MILLISECOND, 5), 
		TimeInterval(TimeInterval::SECOND, 400), 
		0);
	const RefreshPacket& refresh = test.as<RefreshPacket>();

	EXPECT_EQ(refresh.get_control(), PacketCtrl::REFRESH_INITIAL);
	EXPECT_EQ(refresh.get_src(), 0xDEAD);
	EXPECT_EQ(refresh.get_data_interval(), TimeInterval(TimeInterval::MILLISECOND, 5));
	EXPECT_EQ(refresh.get_refresh_interval(), TimeInterval(TimeInterval::SECOND, 400));
	EXPECT_EQ(refresh.get_count(), 0);
	EXPECT_EQ(refresh.get_packet_length(), 11);
}

TEST(LoomPacket, RefreshPacketRaw) {
	constexpr uint8_t raw[] = { 0x01, 0xAD, 0xDE, 0b00010001, 5, 0x90, 0x01, 0x00, 0x00, 0x00, 0x00 };
	const Packet test(raw, sizeof(raw));
	const RefreshPacket& refresh = test.as<RefreshPacket>();

	EXPECT_EQ(refresh.get_control(), PacketCtrl::REFRESH_INITIAL);
	EXPECT_EQ(refresh.get_src(), 0xDEAD);
	EXPECT_EQ(refresh.get_data_interval(), TimeInterval(TimeInterval::MILLISECOND, 5));
	EXPECT_EQ(refresh.get_refresh_interval(), TimeInterval(TimeInterval::SECOND, 400));
	EXPECT_EQ(refresh.get_count(), 0);
	EXPECT_EQ(refresh.get_packet_length(), 11);
}

TEST(LoomPacket, ACKPacket) {
	const Packet test = ACKPacket::Factory(0xDEAD);
	const ACKPacket& ack = test.as<ACKPacket>();

	EXPECT_EQ(ack.get_control(), PacketCtrl::DATA_ACK);
	EXPECT_EQ(ack.get_src(), 0xDEAD);
}

TEST(LoomPacket, ACKPacketRaw) {
	constexpr uint8_t raw[] = { 0x03, 0xAD, 0xDE };
	const Packet test(raw, sizeof(raw));
	const ACKPacket& ack = test.as<ACKPacket>();

	EXPECT_EQ(ack.get_control(), PacketCtrl::DATA_ACK);
	EXPECT_EQ(ack.get_src(), 0xDEAD);
}
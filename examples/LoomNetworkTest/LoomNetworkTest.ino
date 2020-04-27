#include "ArduinoJson.h"
#include "LoomNetwork.h"
#include "Radios/WireRadio.h"

#define CONFIG_FLAT "{\"root\":{\"name\":\"Coord\",\"sensor\":false,\"children\":[{\"name\":\"Device 1\",\"type\":0},{\"name\":\"Device 2\",\"type\":0},{\"name\":\"Device 3\",\"type\":0},{\"name\":\"Device 4\",\"type\":0}]}}"
#define CONFIG_ROUTER1 "{\"root\":{\"name\":\"Coord\",\"sensor\":false,\"children\":[{\"name\":\"Device 1\",\"sensor\":false,\"type\":1,\"children\":[{\"name\":\"Device 2\",\"type\":0},{\"name\":\"Device 3\",\"type\":0}]},{\"name\":\"Device 4\",\"type\":0}]}}"
#define CONFIG_ROUTER2 "{\"root\":{\"name\":\"Coord\",\"sensor\":false,\"children\":[{\"name\":\"Device 1\",\"sensor\":false,\"type\":1,\"children\":[{\"name\":\"Device 2\",\"type\":1,\"sensor\":false,\"children\":[{\"name\":\"Device 3\",\"type\":0}]},{\"name\":\"Device 4\",\"type\":0}]}]}}"
#define CONFIG CONFIG_FLAT

const char NAME[] = "Coord";
const char payload[] = "Hello world!\0";

using NetType = LoomNet::Network<LoomNet::WireRadio, 6, 6, 12>;
using NetStatus = NetType::Status;
StaticJsonDocument<500> json;

void recurse_all_devices(const JsonObjectConst& obj, const JsonObjectConst& root, NetType& send_boi) {
  const char* name = obj["name"];
  if (name 
    && strcmp(name, NAME) != 0 
    && send_boi.get_status() & NetStatus::NET_SEND_RDY) {
      
    // send a packet to that device!
    const uint16_t dst = strcmp(name, root["root"]["name"]) == 0 
      ? LoomNet::ADDR_COORD : LoomNet::get_addr(root, name);
    char buf[16] = {};
    // snprintf(buf, sizeof(buf), "0x%04X->0x%04X", send_boi.get_router().get_self_addr(), dst);
    send_boi.app_send(dst, 0, reinterpret_cast<const uint8_t*>(buf), sizeof(buf));
    const JsonArrayConst childs = obj["children"];
    for (const JsonObjectConst child : childs)
      recurse_all_devices(child, root, send_boi);
  }
}

LoomNet::NetworkInfo make_network(const char* config, const char* name) {
  deserializeJson(json, config);
  const auto obj = json.as<JsonObjectConst>();
  return LoomNet::read_network_topology(obj, name);
}

NetType network(make_network(CONFIG, NAME), LoomNet::WireRadio(5, 6, A2, A4, A0));

void setup() {
  delay(1000);
  // put your setup code here, to run once:
  Serial.begin(9600);
  if (network.get_router().get_device_type() == LoomNet::DeviceType::ERROR) {
    Serial.println("Error device!");
    while(true);
  }
  if (network.get_router().get_device_type() == LoomNet::DeviceType::COORDINATOR) {
    delay(500);
    // while(!Serial);
    Serial.println("Begin Coord!");
  }
  else Serial.println("Begin device!");
}

uint8_t count = 8;

void loop() {
  // Serial.println("Wake!");
  
  // put your main code here, to run repeatedly:
  uint8_t status = network.get_status();
  do {
    status = network.net_update();
    // handle recieve
    if (status & NetStatus::NET_RECV_RDY) {
      const LoomNet::Packet& buf = network.app_recv();
      const LoomNet::DataPacket& frag = buf.as<LoomNet::DataPacket>();
      const uint32_t time_recv = static_cast<uint32_t>(frag.get_payload()[0]) 
        | static_cast<uint32_t>(frag.get_payload()[1]) << 8
        | static_cast<uint32_t>(frag.get_payload()[2]) << 16
        | static_cast<uint32_t>(frag.get_payload()[3]) << 24; 
      Serial.print("Recieved: ");
      Serial.println(time_recv);
      Serial.print("Expected: ");
      // Serial.println((network.get_mac().m_time_wake_start - network.get_mac().m_next_data).get_time());
      Serial.print("From: ");
      Serial.println(frag.get_orig_src(), HEX);
    }
    if (status & NetStatus::NET_CLOSED) {
      Serial.println("Closed!");
      Serial.println(static_cast<uint8_t>(network.get_last_error()));
      Serial.println(static_cast<uint8_t>(network.get_mac().get_last_error()));
      while(1) Serial.flush();
  }
  } while(!(status & NetStatus::NET_SLEEP_RDY));
  // handle send
  if (status & NetStatus::NET_SEND_RDY 
    && ++count >= 8) {

    recurse_all_devices(json.as<JsonObjectConst>()["root"], json.as<JsonObjectConst>(), network);
    count = 0;
  }
  // "sleep"!
  using Unit = LoomNet::TimeInterval::Unit;
  const LoomNet::TimeInterval& sleep(network.net_sleep_next_wake_time());
  Serial.print("Sleep: ");
  Serial.print(sleep.get_unit());
  Serial.print(", ");
  Serial.println(sleep.get_time());
  if (sleep.get_time() != 0) {
    while(network.get_radio().get_time() < sleep);
  }
  // wake!
  network.net_sleep_wake_ack();
}

#include "LoomNetwork.h"
#include "Radios/LoraRadio.h"

LoomNet::LoraRadio recv_radio(A2, A4, A0);

void setup() {
  Serial.begin(115200);
  while(!Serial);
  // put your setup code here, to run once:
  Serial.println("Begin!");
  recv_radio.enable();
  recv_radio.wake();
}

void loop() {
  // put your main code here, to run repeatedly:
  LoomNet::TimeInterval none(LoomNet::TIME_NONE);
  const LoomNet::Packet thing = recv_radio.recv(none);
  if (thing.get_control() != LoomNet::PacketCtrl::NONE) {
    for (uint8_t i = 0; i < thing.get_raw_length(); i++) {
      Serial.print("0x");
      if (thing.get_raw()[i] < 16) Serial.print('0');
      Serial.print(thing.get_raw()[i], HEX);
      Serial.print(", ");
    }
    Serial.println();
    Serial.println(reinterpret_cast<const char*>(thing.as<LoomNet::DataPacket>().get_payload()));
    delay(4000);
  }
}

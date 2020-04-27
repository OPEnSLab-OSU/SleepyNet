#include "LoomNetwork.h"
#include "Radios/LoraRadio.h"

LoomNet::LoraRadio send_radio(A2, A4, A0);
const char payload[] = "Hello world!\0";
const LoomNet::Packet send = LoomNet::DataPacket::Factory(LoomNet::ADDR_COORD, LoomNet::ADDR_COORD, LoomNet::ADDR_COORD, 0, 0, reinterpret_cast<const uint8_t*>(payload), sizeof(payload));

void setup() {
  Serial.begin(115200);
  // while(!Serial);
  // put your setup code here, to run once:
  send_radio.enable();
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(5000);
  send_radio.wake();
  send_radio.send(send);
  Serial.println("Sent!");
  send_radio.sleep();
}

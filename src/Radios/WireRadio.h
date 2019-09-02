#pragma once

#include "Arduino.h"
#include "../LoomRadio.h"

/** A simple testing radio, using a wire as airwaves */

constexpr auto SLOT_LENGTH_MILLIS = 10000;
constexpr auto SEND_DELAY_MILLIS = 500;
constexpr auto WIRE_RECV_TIMEOUT_MILLIS = 1000 + SEND_DELAY_MILLIS;
constexpr auto BIT_LENGTH = 400;

namespace LoomNet {
    class WireRadio : public Radio {
    public:

        WireRadio(uint8_t data_pin, uint8_t send_indicator_pin, uint8_t recv_indicator_pin, uint8_t pwr_indicator_pin)
            : m_data_pin(data_pin)
            , m_send_ind(send_indicator_pin)
            , m_recv_ind(recv_indicator_pin)
            , m_pwr_ind(pwr_indicator_pin) 
            , m_state(State::DISABLED)
            , m_buffer{} {
                // setup all pins to output and power low, except for data which needs to be input pullup
                pinMode(data_pin,                   INPUT);
                pinMode(send_indicator_pin,         OUTPUT);
                pinMode(recv_indicator_pin,         OUTPUT);
                pinMode(pwr_indicator_pin,          OUTPUT);
                digitalWrite(send_indicator_pin,    LOW);
                digitalWrite(recv_indicator_pin,    LOW);
                digitalWrite(pwr_indicator_pin,     LOW);
            }

        TimeInterval get_time() override { return TimeInterval(TimeInterval::MILLISECOND, millis()); }
        State get_state() const override { return m_state; }
        void enable() override {
            if (m_state != State::DISABLED) 
                Serial.println("Invalid radio state movement in enable()");
            m_state = State::SLEEP;
        }
        void disable() override {
            if (m_state != State::SLEEP) 
                Serial.println("Invalid radio state movement in disable()");
            m_state = State::DISABLED;
        }
        void sleep() override {
            if (m_state != State::IDLE) 
                Serial.println("Invalid radio state movement in sleep()");
            m_state = State::SLEEP;
            // turn power indicator off
            digitalWrite(m_pwr_ind, LOW);
        }
        void wake() override {
            if (m_state != State::SLEEP) 
                Serial.println("Invalid radio state movement in wake()");
            m_state = State::IDLE;
            // turn power indicator on
            digitalWrite(m_pwr_ind, HIGH);
        }
        LoomNet::Packet recv(TimeInterval& recv_stamp) override {
            if (m_state != State::IDLE) 
                Serial.println("Invalid radio state to recv");
            // turn recv indicator on
            digitalWrite(m_recv_ind, HIGH);
            // set the pin to have a pullup resistor
            pinMode(m_data_pin, INPUT);
            // start reading the data pin
            // get the buffer ready
            for (uint8_t i = 0; i < sizeof(m_buffer); i++) m_buffer[i] = 0;
            // check the slot for one second, since this is pretty high tolarance
            uint32_t start = millis();
            bool found = false;
            while (millis() - start < WIRE_RECV_TIMEOUT_MILLIS && !found) found = found || (digitalRead(m_data_pin) == LOW);
            // if we found something, start recieving it
            if (found) {
                Serial.println("Found!");
                // set the recv_stamp to when we first heard the signal
                recv_stamp = get_time() - TimeInterval(TimeInterval::MILLISECOND, SEND_DELAY_MILLIS);
                // delay half a bit so we can start reading the middle of the peaks
                delayMicroseconds(100);
                // read the "airwaves" 254 times!
                for (uint8_t i = 0; i < 255; i++) {
                    for (uint8_t b = 0; b < 8; b++) {
                        m_buffer[i] |= (digitalRead(m_data_pin) == LOW ? 1 : 0) << b;
                        delayMicroseconds(BIT_LENGTH);
                    }
                }
                Serial.print("Got: ");
                for (uint8_t i = 0; i < 255; i++) {
                    Serial.print("0x");
                    if (m_buffer[i] <= 0x0F) Serial.print('0');
                    Serial.print(m_buffer[i], HEX);
                    Serial.print(", ");
                }
                Serial.println();
            }
            // reset the data pin
            pinMode(m_data_pin, INPUT);
            // reset the indicator
            digitalWrite(m_recv_ind, LOW);
            // return data!
            return LoomNet::Packet{ m_buffer, static_cast<uint8_t>(sizeof(m_buffer)) };
        }
        void send(const LoomNet::Packet& send) override {
            if (m_state != State::IDLE) 
                Serial.println("Invalid radio state to recv");
            // wait a bit for the recieving device to initialize
            delayMicroseconds(SEND_DELAY_MILLIS * 1000);
            // turn on the indicator!
            digitalWrite(m_send_ind, HIGH);
            // initialize our data pin to output low, to send a leading one
            pinMode(m_data_pin, OUTPUT);
            // start writing to the "network"!
            for (uint8_t i = 0; i < send.get_raw_length(); i++) {
                for (uint8_t b = 1;;) {
                    digitalWrite(m_data_pin, (send.get_raw()[i] & b) ? LOW : HIGH);
                    delayMicroseconds(BIT_LENGTH);
                    if (b == 0x80) break;
                    b <<= 1;
                }
            }
            // reset the data pin
            digitalWrite(m_data_pin, HIGH);
            digitalWrite(m_data_pin, INPUT);
            // we're all done!
            digitalWrite(m_send_ind, LOW);
        }

    private:

        const uint8_t m_data_pin;
        const uint8_t m_send_ind;
        const uint8_t m_recv_ind;
        const uint8_t m_pwr_ind;
        State m_state;
        uint8_t m_buffer[255];
        uint32_t m_cur_time;
    };
}
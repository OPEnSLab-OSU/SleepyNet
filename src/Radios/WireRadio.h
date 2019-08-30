#pragma once

#include <array>
#include "Arduino.h"
#include "../LoomRadio.h"

/** A simple testing radio, using a wire as airwaves */

constexpr auto SLOT_LENGTH = 10000;
constexpr auto WIRE_RECV_TIMEOUT = 1000;
constexpr auto BIT_LENGTH = 4;

namespace LoomNet {
    class WireRadio : public Radio {
    public:

        WireRadio(uint8_t data_pin, uint8_t send_indicator_pin, uint8_t recv_indicator_pin, uint8_t pwr_indicator_pin)
            : m_data_pin(data_pin)
            , m_send_ind(send_indicator_pin)
            , m_recv_ind(recv_indicator_pin)
            , m_pwr_ind(pwr_indicator_pin) 
            , m_state(State::DISABLED)
            , m_buffer{ 0 } {
                // setup all pins to output and power low
                pinMode(data_pin,           OUTPUT);
                pinMode(send_indicator_pin,     OUTPUT);
                pinMode(recv_indicator_pin, OUTPUT);
                pinMode(pwr_indicator_pin,  OUTPUT);
                digitalWrite(data_pin,           LOW);
                digitalWrite(send_indicator_pin,     LOW);
                digitalWrite(recv_indicator_pin, LOW);
                digitalWrite(pwr_indicator_pin,  LOW);
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
        LoomNet::Packet recv() override {
            if (m_state != State::IDLE) 
                Serial.println("Invalid radio state to recv");
            // turn recv indicator on
            digitalWrite(m_recv_ind, HIGH);
            // start reading the data pin
            pinMode(m_data_pin, INPUT);
            // get the buffer ready
            m_buffer.fill(0);
            // check the slot for one second, since this is pretty high tolarance
            uint32_t start = millis();
            bool found = false;
            while (millis() - start < WIRE_RECV_TIMEOUT && !found) found = found || digitalRead(m_data_pin);
            // if we found something, start recieving it
            if (found) {
                // delay half a bit so we can start reading the middle of the peaks
                delay(1);
                // read the "airwaves" 254 times!
                for (uint8_t i = 0; i < 255; i++) {
                    for (uint8_t b = 0; b < 8; b++) {
                        m_buffer[i] |= (digitalRead(m_data_pin) == HIGH ? 1 : 0) << b;
                        delay(BIT_LENGTH);
                    }
                }
            }
            // reset the indicator
            digitalWrite(m_recv_ind, LOW);
            // set the pin back to output
            pinMode(m_data_pin, OUTPUT);
            // return data!
            return LoomNet::Packet{ m_buffer.data(), static_cast<uint8_t>(m_buffer.size()) };
        }
        void send(const LoomNet::Packet& send) override {
            if (m_state != State::IDLE) 
                Serial.println("Invalid radio state to recv");
            // turn on the indicator!
            digitalWrite(m_send_ind, HIGH);
            // wait a bit for the recieving device to initialize
            delay(BIT_LENGTH);
            // start writing to the "network"!
            for (uint8_t i = 0; i < send.get_packet_length(); i++) {
                for (uint8_t b = 1;;) {
                    digitalWrite(m_data_pin, (send.get_raw()[i] & b) ? HIGH : LOW);
                    delay(BIT_LENGTH);
                    if (b == 0x80) break;
                    b <<= 1;
                }
            }
            // turn the pin off
            digitalWrite(m_data_pin, LOW);
            // we're all done!
            digitalWrite(m_send_ind, LOW);
        }

    private:

        const uint8_t m_data_pin;
        const uint8_t m_send_ind;
        const uint8_t m_recv_ind;
        const uint8_t m_pwr_ind;
        State m_state;
        std::array<uint8_t, 255> m_buffer;
        uint32_t m_cur_time;
    };
}
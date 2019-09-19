#pragma once

#include "Arduino.h"
#include "../LoomRadio.h"
#include <SPI.h>
#include "RF95.h"

/** A more complicated testing radio, designed to be used with the RFM95 */

constexpr auto RFM95_CS = 8;
constexpr auto RFM95_INT = 3;
constexpr auto RFM95_RST = 4;
constexpr auto RF95_FREQ = 915.0f;
constexpr auto LED = 13;

constexpr auto SMALL_RECV_TIMEOUT = 200;

namespace LoomNet {
    class LoraRadio : public Radio {
    public:

        LoraRadio(const uint8_t send_indicator_pin, 
            const uint8_t recv_indicator_pin, 
            const uint8_t pwr_indicator_pin)
            : m_send_ind(send_indicator_pin)
            , m_recv_ind(recv_indicator_pin)
            , m_pwr_ind(pwr_indicator_pin) 
            , m_state(State::DISABLED)
            , m_rfm(RFM95_CS, RFM95_INT) {}

        TimeInterval get_time() const override { 
            // get time using the internal RTC counter!
            return TimeInterval(TimeInterval::Unit::MILLISECOND, millis());
        }
        State get_state() const override { return m_state; }
        void enable() override {
            if (m_state != State::DISABLED) 
                Serial.println("Invalid radio state movement in enable()");
            m_state = State::SLEEP;
            // setup all pins to output and power low
            pinMode(m_send_ind,         OUTPUT);
            pinMode(m_recv_ind,         OUTPUT);
            pinMode(m_pwr_ind,          OUTPUT);
            digitalWrite(m_send_ind,    LOW);
            digitalWrite(m_recv_ind,    LOW);
            digitalWrite(m_pwr_ind,     LOW);
            delay(100);
            // init!
            if (!m_rfm.init()) {
                Serial.println("No radio found!");
                while(1);
            }
            m_rfm.setTxPower(13);
            constexpr RF95::ModemConfig config = { 
                0b10010010, // explicit header on, 4/5 coding rate, 500kHz
                0b10100000, // 1024 chirps/symbol, tx continous off, CRC off, 0 symbol timeout MSBs
                0x04, // AGC on
            };
            m_rfm.setModemRegisters(config);
            // tweak the preamble so it's a little longer (default is 8)
            m_rfm.setPreambleLength(12);
            // put the modem into sleep mode until we need it later
            m_rfm.setMode(RF95::RF_MODE::SLEEP);
            /*
            // configure the internal RTC to act as our timer
            RTC->MODE2.CTRL.reg &= ~RTC_MODE0_CTRL_ENABLE; // disable RTC
            // while (RTC->MODE0.STATUS.bit.SYNCBUSY);
            RTC->MODE2.CTRL.reg |= RTC_MODE0_CTRL_SWRST; // software reset
            // while (RTC->MODE0.STATUS.bit.SYNCBUSY);
            PM->APBAMASK.reg |= PM_APBAMASK_RTC; // turn on digital interface clock
            // set gclk divider
            GCLK->GENDIV.reg = GCLK_GENDIV_ID(2)|GCLK_GENDIV_DIV(0);
            while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY);
            GCLK->GENCTRL.reg = (GCLK_GENCTRL_GENEN | GCLK_GENCTRL_SRC_OSC32K | GCLK_GENCTRL_ID(2) | GCLK_GENCTRL_DIVSEL );
            while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY);
            GCLK->CLKCTRL.reg = (uint32_t)((GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2 | (RTC_GCLK_ID << GCLK_CLKCTRL_ID_Pos)));
            while (GCLK->STATUS.bit.SYNCBUSY);
            // tell the RTC to operate as a 32 bit counter
            RTC->MODE0.CTRL.reg = RTC_MODE0_CTRL_MODE_COUNT32 | RTC_MODE0_CTRL_PRESCALER_DIV16;
            while (RTC->MODE0.STATUS.bit.SYNCBUSY);
            RTC->MODE0.CTRL.reg |= RTC_MODE0_CTRL_ENABLE; // enable RTC
            while (RTC->MODE0.STATUS.bit.SYNCBUSY);
            RTC->MODE0.CTRL.reg &= ~RTC_MODE0_CTRL_SWRST; // software reset remove
            while (RTC->MODE0.STATUS.bit.SYNCBUSY);
            */
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
            // sleep radio!
            m_rfm.setMode(RF95::RF_MODE::SLEEP);
            // turn power indicator off
            digitalWrite(m_pwr_ind, LOW);
        }
        void wake() override {
            if (m_state != State::SLEEP) 
                Serial.println("Invalid radio state movement in wake()");
            m_state = State::IDLE;
            // wake the LoRa radio into standby mode
            m_rfm.setMode(RF95::RF_MODE::IDLE);
            delay(10);
            // turn power indicator on
            digitalWrite(m_pwr_ind, HIGH);
        }
        LoomNet::Packet recv(TimeInterval& recv_stamp) override {
            uint8_t buf[PACKET_MAX] = {};
            if (m_state != State::IDLE) 
                Serial.println("Invalid radio state to recv");
            // turn recv indicator on
            digitalWrite(m_recv_ind, HIGH);
            // run a CAD check on the airwaves
            const auto cur_time = get_time();
            if(m_rfm.checkRadio()) {
                // const auto time = millis();
                // we found a packet! recieve it
                m_rfm.recvSingle(buf, PACKET_MAX);
                recv_stamp = cur_time;
            }
            // reset the indicator
            digitalWrite(m_recv_ind, LOW);
            // return data!
            return LoomNet::Packet{ buf, static_cast<uint8_t>(sizeof(buf)) };
        }
        void send(const LoomNet::Packet& send) override {
            if (m_state != State::IDLE) 
                Serial.println("Invalid radio state to recv");
            // turn on the indicator!
            digitalWrite(m_send_ind, HIGH);
            // send the packet over LoRa!
            m_rfm.sendSingle(send.get_raw(), send.get_packet_length());
            // we're all done!
            digitalWrite(m_send_ind, LOW);
        }
    
    private:
        const uint8_t m_send_ind;
        const uint8_t m_recv_ind;
        const uint8_t m_pwr_ind;
        State m_state;
        RF95 m_rfm;
    };
}
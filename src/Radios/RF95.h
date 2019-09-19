#pragma once

#include <Arduino.h>
#include <RH_RF95.h>

#define SERIAL_DEBUG

/** A simplified version of the RH_RF95 driver, designed to be more low-level than RH */

static constexpr auto INT_PIN_TIMEOUT = 200;

static const SPISettings RH_spi_props/*(1000000, BitOrder::MSBFIRST, SPI_MODE0)*/;

class RF95 {
public:
    enum class RF_MODE {
        SLEEP = RH_RF95_MODE_SLEEP,
        IDLE = RH_RF95_MODE_STDBY,
        RX_SINGLE = RH_RF95_MODE_RXSINGLE,
        RX_CONT = RH_RF95_MODE_RXCONTINUOUS,
        RX_CAD = RH_RF95_MODE_CAD,
        TX = RH_RF95_MODE_TX,
    };

    typedef struct
    {
        uint8_t    reg_1d;   ///< Value for register RH_RF95_REG_1D_MODEM_CONFIG1
        uint8_t    reg_1e;   ///< Value for register RH_RF95_REG_1E_MODEM_CONFIG2
        uint8_t    reg_26;   ///< Value for register RH_RF95_REG_26_MODEM_CONFIG3
    } ModemConfig;

    RF95(const uint8_t CS, const uint8_t INT)
        : m_CS(CS)
        , m_INT(INT) {}

    bool init() const {
        // initialize the SPI bus
        SPI.begin();
        // and the CS, INT, and RST pins
        pinMode(m_CS, OUTPUT);
        pinMode(m_INT, INPUT);
        // disble chip select
        digitalWrite(m_CS, HIGH);
        // wait for the chip to startup
        delay(100);
        // Set sleep mode, so we can also set LORA mode:
        m_spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_SLEEP | RH_RF95_LONG_RANGE_MODE);
        delay(10); // Wait for sleep mode to take over from say, CAD
        // Check we are in sleep mode, with LORA set
        if (m_spiRead(RH_RF95_REG_01_OP_MODE) != (RH_RF95_MODE_SLEEP | RH_RF95_LONG_RANGE_MODE))
        {
        #ifdef SERIAL_DEBUG
            Serial.println(F("ERROR: Failed to put device in LoRa mode."));
            Serial.print(F("RH_RF95_REG_01_OP_MODE: "));
            Serial.println(m_spiRead(RH_RF95_REG_01_OP_MODE), HEX);
        #endif
            return false; // No device present?
        }
        // Set up FIFO
        // We configure so that we can use the entire 256 byte FIFO for either receive
        // or transmit, but not both at the same time
        m_spiWrite(RH_RF95_REG_0E_FIFO_TX_BASE_ADDR, 0);
        m_spiWrite(RH_RF95_REG_0F_FIFO_RX_BASE_ADDR, 0);

        // Packet format is preamble + payload
        // Implicit Header Mode
        // payload is just message data
        // RX mode is implmented with RXSINGLE
        // max message data length is 255 octets
        // start the radio in idle mode
        setMode(RF_MODE::IDLE);
        // configure it with the default settings
        // Set up default configuration
        // No Sync Words in LORA mode.
        const ModemConfig default_config = { 0x72,   0x74,    0x04};
        setModemRegisters(default_config); // Radio default
        setPreambleLength(8); // Default is 8
        // default frequency for the RF95
        setFrequency(915.0f);
        // Lowish power
        setTxPower(13);
        // all done!
        return true;
    }

    /** Device MUST be in idle mode before this function is run! */
    bool checkRadio() const {
        setMode(RF_MODE::RX_CAD);
        m_spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x80); // Interrupt on CadDone
        // wait for the interrupt pin to go high!
        if(!m_waitIntPin()) return false;
        // read the result
        // Read the interrupt register
        const uint8_t flags = m_spiRead(RH_RF95_REG_12_IRQ_FLAGS);
        // Clear all IRQ flags
        m_spiWrite(RH_RF95_REG_12_IRQ_FLAGS, 0xff); 
        // if the device did not detect a packet, set the mode to idle and return false
        return flags & RH_RF95_CAD_DETECTED;
    }

    bool recvSingle(uint8_t* buf, const uint8_t max_len) const {
        setMode(RF_MODE::RX_SINGLE);
        m_spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x00); // Interrupt on RxDone
        // wait again for the device to finish recieving (or timeout)
        delay(10);
        if(!m_waitIntPin()) return false;
        // check the packet!
        const uint8_t flags = m_spiRead(RH_RF95_REG_12_IRQ_FLAGS);
        if (flags & RH_RF95_RX_TIMEOUT) {
            #ifdef SERIAL_DEBUG
                Serial.println("Timed out when reciving a packet!");
            #endif
            return false;
        }
        // packet is good! copy it into the result buffer   
	    const uint8_t len = min(m_spiRead(RH_RF95_REG_13_RX_NB_BYTES), max_len);
        #ifdef SERIAL_DEBUG
        if (len == max_len) Serial.println("LoRa packet clipped to max_len!");
        #endif
        // Reset the fifo read ptr to the beginning of the packet
	    m_spiWrite(RH_RF95_REG_0D_FIFO_ADDR_PTR, m_spiRead(RH_RF95_REG_10_FIFO_RX_CURRENT_ADDR));
        // read the packet!
        m_spiBurstRead(RH_RF95_REG_00_FIFO, buf, len);
        // Clear all IRQ flags
        m_spiWrite(RH_RF95_REG_12_IRQ_FLAGS, 0xff);
        // reset to idle mode!
        setMode(RF_MODE::IDLE);
        return true;
    }

    void sendSingle(const uint8_t* buf, const uint8_t len) const {
        // Position at the beginning of the FIFO
        m_spiWrite(RH_RF95_REG_0D_FIFO_ADDR_PTR, 0);
        // The message data
        m_spiBurstWrite(RH_RF95_REG_00_FIFO, buf, len);
        m_spiWrite(RH_RF95_REG_22_PAYLOAD_LENGTH, len);
        // transmit!
        setMode(RF_MODE::TX);
        m_spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x40);
        // wait until the transmission is done
        m_waitIntPin();
        // set the device back into idle mode!
        setMode(RF_MODE::IDLE);
        // Clear all IRQ flags
        m_spiWrite(RH_RF95_REG_12_IRQ_FLAGS, 0xff); 
    }

    void setTxPower(int8_t power, const bool useRFO = false) const
    {
        // Sigh, different behaviours depending on whther the module use PA_BOOST or the RFO pin
        // for the transmitter output
        if (useRFO)
        {
            if (power > 14)
                power = 14;
            if (power < -1)
                power = -1;
                m_spiWrite(RH_RF95_REG_09_PA_CONFIG, RH_RF95_MAX_POWER | (power + 1));
        }
        else
        {
            if (power > 23)
                power = 23;
            if (power < 5)
                power = 5;

            // For RH_RF95_PA_DAC_ENABLE, manual says '+20dBm on PA_BOOST when OutputPower=0xf'
            // RH_RF95_PA_DAC_ENABLE actually adds about 3dBm to all power levels. We will us it
            // for 21, 22 and 23dBm
            if (power > 20)
            {
                m_spiWrite(RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_ENABLE);
                power -= 3;
            }
            else
            {
                m_spiWrite(RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_DISABLE);
            }

            // RFM95/96/97/98 does not have RFO pins connected to anything. Only PA_BOOST
            // pin is connected, so must use PA_BOOST
            // Pout = 2 + OutputPower.
            // The documentation is pretty confusing on this topic: PaSelect says the max power is 20dBm,
            // but OutputPower claims it would be 17dBm.
            // My measurements show 20dBm is correct
            m_spiWrite(RH_RF95_REG_09_PA_CONFIG, RH_RF95_PA_SELECT | (power-5));
        }
    }

    // Sets registers from a canned modem configuration structure
    void setModemRegisters(const ModemConfig& config) const
    {
        m_spiWrite(RH_RF95_REG_1D_MODEM_CONFIG1,       config.reg_1d);
        m_spiWrite(RH_RF95_REG_1E_MODEM_CONFIG2,       config.reg_1e);
        m_spiWrite(RH_RF95_REG_26_MODEM_CONFIG3,       config.reg_26);
    }

    void setPreambleLength(const uint16_t bytes) const
    {
        m_spiWrite(RH_RF95_REG_20_PREAMBLE_MSB, bytes >> 8);
        m_spiWrite(RH_RF95_REG_21_PREAMBLE_LSB, bytes & 0xff);
    }

    void setFrequency(const float centre) const
    {
        // Frf = FRF / FSTEP
        const uint32_t frf = (centre * 1000000.0) / RH_RF95_FSTEP;
        m_spiWrite(RH_RF95_REG_06_FRF_MSB, (frf >> 16) & 0xff);
        m_spiWrite(RH_RF95_REG_07_FRF_MID, (frf >> 8) & 0xff);
        m_spiWrite(RH_RF95_REG_08_FRF_LSB, frf & 0xff);
    }

    /** we intentionally perform no mode validation, so be careful! */
    void setMode(const RF_MODE mode) const {
        m_spiWrite(RH_RF95_REG_01_OP_MODE, static_cast<const uint8_t>(mode));
    }

private:

    bool m_waitIntPin() const {
        const auto start = millis();
        while (millis() - start < INT_PIN_TIMEOUT) 
            if (digitalRead(m_INT) == HIGH) return true;
        #ifdef SERIAL_DEBUG
        Serial.println("Timed out waiting for lora interrupt pin!");
        #endif
        return false;
    }

    uint8_t m_spiRead(const uint8_t reg) const
    {
        SPI.beginTransaction(RH_spi_props); 
        digitalWrite(m_CS, LOW);
        SPI.transfer(reg & ~RH_SPI_WRITE_MASK); // Send the address with the write mask off
        const uint8_t val = SPI.transfer(0); // The written value is ignored, reg value is read
        digitalWrite(m_CS, HIGH);
        SPI.endTransaction();
        return val;
    }

    uint8_t m_spiWrite(const uint8_t reg, const uint8_t val) const
    {
        SPI.beginTransaction(RH_spi_props);
        digitalWrite(m_CS, LOW);
        const uint8_t status = SPI.transfer(reg | RH_SPI_WRITE_MASK); // Send the address with the write mask on
        SPI.transfer(val); // New value follows
        digitalWrite(m_CS, HIGH);
        SPI.endTransaction();
        return status;
    }

    uint8_t m_spiBurstRead(const uint8_t reg, uint8_t* dest, uint8_t len) const
    {
        SPI.beginTransaction(RH_spi_props);
        digitalWrite(m_CS, LOW);
        const uint8_t status = SPI.transfer(reg & ~RH_SPI_WRITE_MASK); // Send the start address with the write mask off
        while (len--)
            *dest++ = SPI.transfer(0);
        digitalWrite(m_CS, HIGH);
        SPI.endTransaction();
        return status;
    }

    uint8_t m_spiBurstWrite(const uint8_t reg, const uint8_t* src, uint8_t len) const
    {
        SPI.beginTransaction(RH_spi_props);
        digitalWrite(m_CS, LOW);
        const uint8_t status = SPI.transfer(reg | RH_SPI_WRITE_MASK); // Send the start address with the write mask on
        while (len--)
            SPI.transfer(*src++);
        digitalWrite(m_CS, HIGH);
        SPI.endTransaction();
        return status;
    }

    const uint8_t m_CS;
    const uint8_t m_INT;
};
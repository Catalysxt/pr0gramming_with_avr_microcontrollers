// 24LC256 I2C EEPROM Driver Implementation
// Protocol reference: Microchip 24LC256 datasheet
// TWI hardware: SDA=PC4, SCL=PC5 (ATmega328P hardware-fixed TWI pins)

#include "24LC256.h"
#include <util/delay.h>

// -----------------------------------------------------------------------
// Internal helper: send the device write address + 16-bit memory address.
// Every read and write operation opens with this sequence to set the
// EEPROM's internal address pointer.
// 24LC256 datasheet p. 6 — "the word address requires two 8-bit data
// transmissions to fill the 13-bit address counter"
// -----------------------------------------------------------------------
static void eeprom_send_address(uint16_t address)
{
    i2c_send(EEPROM_I2C_WRITE_ADDR);   // Control byte: device address + WRITE
    i2c_send((uint8_t)(address >> 8)); // High address byte (A15..A8)
    i2c_send((uint8_t)address);        // Low address byte  (A7..A0)
}

// -----------------------------------------------------------------------
// Random Read — 24LC256 datasheet p. 8
// A "dummy write" loads the address counter; a Repeated START then
// flips the bus direction to read without releasing the bus.
// Sequence: START → write addr → mem addr → RESTART → read addr → data → NACK → STOP
// -----------------------------------------------------------------------
uint8_t EEPROM_read_byte(uint16_t address)
{
    uint8_t data;

    i2c_start();
    eeprom_send_address(address);   // Dummy write to set internal address pointer

    i2c_start();                    // Repeated START — retains bus ownership
    i2c_send(EEPROM_I2C_READ_ADDR);

    data = i2c_read_no_ack();       // Only one byte; NACK tells the slave to stop driving SDA
    i2c_stop();

    return data;
}

// -----------------------------------------------------------------------
// Sequential Read — 24LC256 datasheet p. 8
// After the first byte the EEPROM auto-increments its address counter.
// ACK after MSB asks for one more byte; NACK after LSB ends the read.
// -----------------------------------------------------------------------
uint16_t EEPROM_read_word(uint16_t address)
{
    uint16_t word;

    i2c_start();
    eeprom_send_address(address);

    i2c_start();
    i2c_send(EEPROM_I2C_READ_ADDR);

    word  = (uint16_t)i2c_read_ack() << 8; // MSB — ACK requests the next byte
    word |= i2c_read_no_ack();              // LSB — NACK ends the sequential read
    i2c_stop();

    return word;
}

// -----------------------------------------------------------------------
// Byte Write — 24LC256 datasheet p. 6
// After STOP, the chip enters its internal write cycle (up to Twc = 5 ms).
// The bus must not be accessed until the write cycle completes.
// -----------------------------------------------------------------------
void EEPROM_write_byte(uint16_t address, uint8_t byte)
{
    i2c_start();
    eeprom_send_address(address);
    i2c_send(byte);
    i2c_stop();

    _delay_ms(EEPROM_WRITE_CYCLE_MS); // Wait for internal write cycle — datasheet p. 3, Twc
}

// -----------------------------------------------------------------------
// Two-byte write within a single page.
// Both bytes must land in the same 64-byte page; the internal address
// counter wraps at page boundaries (datasheet p. 7, note 3).
// -----------------------------------------------------------------------
void EEPROM_write_word(uint16_t address, uint16_t word)
{
    i2c_start();
    eeprom_send_address(address);
    i2c_send((uint8_t)(word >> 8)); // MSB first
    i2c_send((uint8_t)word);        // LSB
    i2c_stop();

    _delay_ms(EEPROM_WRITE_CYCLE_MS);
}

// -----------------------------------------------------------------------
// Page Write — 24LC256 datasheet p. 7
// Writes all 64 bytes in each page to 0x00, page by page.
// Total time: 512 pages × 5 ms = ~2.56 s
// -----------------------------------------------------------------------
void EEPROM_clear_all(void)
{
    uint16_t page_address = 0;
    uint8_t i;

    while (page_address <= EEPROM_BYTES_MAX)
    {
        i2c_start();
        eeprom_send_address(page_address);

        for (i = 0; i < EEPROM_BYTES_PER_PAGE; i++)
        {
            i2c_send(0x00);
        }

        i2c_stop();
        _delay_ms(EEPROM_WRITE_CYCLE_MS); // Wait for page write to complete

        page_address += EEPROM_BYTES_PER_PAGE;
    }
}

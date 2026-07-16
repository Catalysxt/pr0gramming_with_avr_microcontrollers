// I2C (TWI) driver implementation for ATmega328P
// ATmega328P datasheet section 21 — Two-Wire Serial Interface
// Registers: TWBR (bit rate), TWCR (control), TWDR (data)
// Key bits: TWEN (enable), TWSTA (start), TWSTO (stop), TWINT (interrupt flag), TWEA (ack enable)

#include "i2c.h"

void init_I2C(void)
{
    // Set SCL frequency via TWBR. Prescaler bits TWPS[1:0] in TWSR default to 00 (scalar = 1).
    // At 16 MHz, 100 kHz: (16 000 000 / 100 000 - 16) / 2 = 72
    // ATmega328P datasheet p. 242
    TWBR = (uint8_t)((F_CPU / TWI_SCL_FREQ - 16) / 2);

    TWCR |= (1 << TWEN); // TWEN — enables the TWI interface and takes control of SCL/SDA pins
}

void i2c_wait_for_complete(void)
{
    // TWINT is cleared by hardware when an operation begins and set again when it finishes.
    // ATmega328P datasheet p. 228
    loop_until_bit_is_set(TWCR, TWINT);
}

void i2c_start(void)
{
    // Writing TWINT+TWSTA+TWEN causes the hardware to transmit a (repeated) START condition.
    // Clearing TWINT by writing 1 to it starts the operation — do NOT use |= here.
    // ATmega328P datasheet p. 228, status codes table 21-3 (0x08 = START transmitted)
    TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWSTA));
    i2c_wait_for_complete();
}

void i2c_stop(void)
{
    // TWSTO transmits a STOP condition. Hardware clears TWSTO automatically when done.
    // No need to poll TWINT after STOP. ATmega328P datasheet p. 229
    TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWSTO));
}

void i2c_send(uint8_t data)
{
    TWDR = data;                        // Load byte into TWI data register
    TWCR = (_BV(TWINT) | _BV(TWEN));   // Clear TWINT to begin transmission
    i2c_wait_for_complete();
}

uint8_t i2c_read_ack(void)
{
    // TWEA=1 — after receiving the byte the master sends ACK, telling the slave to continue.
    // ATmega328P datasheet p. 244, status 0x50 (data received, ACK returned)
    TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWEA));
    i2c_wait_for_complete();
    return TWDR;
}

uint8_t i2c_read_no_ack(void)
{
    // TWEA=0 — master sends NACK, signaling the slave to release SDA after this byte.
    // ATmega328P datasheet p. 244, status 0x58 (data received, NACK returned)
    TWCR = (_BV(TWINT) | _BV(TWEN));
    i2c_wait_for_complete();
    return TWDR;
}

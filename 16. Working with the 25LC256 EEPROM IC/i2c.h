// I2C (TWI) driver for ATmega328P
// Hardware pins are fixed by the TWI peripheral: SDA=PC4, SCL=PC5
// ATmega328P datasheet section 21 — Two-Wire Serial Interface

#ifndef I2C_H_
#define I2C_H_

#include <avr/io.h>

// Target SCL clock frequency. TWBR is derived from this + F_CPU at init.
// Formula (TWPS prescaler bits = 00, scalar = 1):
//   SCL = F_CPU / (16 + 2 * TWBR)  →  TWBR = (F_CPU / SCL_FREQ - 16) / 2
// ATmega328P datasheet p. 242
#define TWI_SCL_FREQ 100000UL

// Activate TWI hardware and set bus speed to TWI_SCL_FREQ
void init_I2C(void);

// Block until TWINT is set (hardware finished the current bus operation)
void i2c_wait_for_complete(void);

// Issue a START (or repeated START) condition on the bus
void i2c_start(void);

// Issue a STOP condition and release the bus
void i2c_stop(void);

// Load data into TWDR and clock it out; block until complete
void i2c_send(uint8_t data);

// Read one byte from the slave and respond with ACK (more bytes to come)
uint8_t i2c_read_ack(void);

// Read one byte from the slave and respond with NACK (this is the last byte)
uint8_t i2c_read_no_ack(void);

#endif /* I2C_H_ */


#include <avr/io.h>
#include <util/delay.h>     // _delay_ms()
#include <avr/power.h>      // clock_prescale_set()

#include "pinDefines.h"
#include "USART.h"          // initUSART(), printString(), transmitByte(), receiveByte(), getNumber()
#include "i2c.h"            // init_I2C(), i2c_start/stop/send/read_*()
#include "24lc256.h"        // EEPROM_read/write_byte/word(), EEPROM_clear_all()
#include "ds3231.h"         // rtc_time_t, ds3231_read_time(), ds3231_set_time()

// ---- LM75 I2C addresses ------------------------------------------------
// 7-bit base = 0b1001000 (0x48); A2=A1=A0=GND. LM75 datasheet (TI SNOSC16D) p.10
#define LM75_ADDRESS_W      0x90    // 0x48 << 1 | 0 (write)
#define LM75_ADDRESS_R      0x91    // 0x48 << 1 | 1 (read)

// LM75 internal register pointer values -- datasheet p.15, Table 3
#define LM75_TEMP_REGISTER      0x00
#define LM75_CONFIG_REGISTER    0x01
#define LM75_THYST_REGISTER     0x02    // OS pin goes low when temp falls below this threshold
#define LM75_TOS_REGISTER       0x03    // OS pin goes high when temp exceeds this threshold

// ---- On-EEPROM log record -----------------------------------------------
// One record = one timestamped reading. -fpack-struct (see Makefile CFLAGS)
// guarantees this lays out as exactly 7 contiguous bytes, in field order,
// with no compiler-inserted padding -- required since we serialize it to
// EEPROM byte-by-byte via eeprom_write_record()/eeprom_read_record().
typedef struct {
    uint8_t year;    // offset from 2000 -- matches rtc_time_t
    uint8_t month;
    uint8_t date;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t temp;    // packed LM75 reading -- see print_temperature()
} log_record_t;

#define RECORD_SIZE   ((uint16_t)sizeof(log_record_t))   // 7 bytes/reading

// ---- 24LC256 address map -----------------------------------------------
// Addresses 0x0000-0x0003 hold two 16-bit config words (persistent across power cycles).
// Timestamped log records begin at MEMORY_START and grow upward, RECORD_SIZE bytes apart.
#define CURRENT_LOCATION_POINTER    0   // uint16 -- next write address in EEPROM
#define SECONDS_POINTER             2   // uint16 -- seconds between readings
#define MEMORY_START                4   // first byte used for log records

// ---- Misc ---------------------------------------------------------------
#define MENU_DELAY  5   // seconds the menu prompt is shown after power-up

// ---- Helpers ------------------------------------------------------------

// Print a uint8_t as decimal without leading zeros
static void print_uint8(uint8_t val) {
    if (val >= 100) { transmitByte('0' + val / 100); }
    if (val >= 10)  { transmitByte('0' + (val / 10) % 10); }
    transmitByte('0' + val % 10);
}

// Print a uint16_t as decimal without leading zeros
static void print_uint16(uint16_t val) {
    if (val >= 10000) { transmitByte('0' + val / 10000); }
    if (val >= 1000)  { transmitByte('0' + (val / 1000) % 10); }
    if (val >= 100)   { transmitByte('0' + (val / 100) % 10); }
    if (val >= 10)    { transmitByte('0' + (val / 10) % 10); }
    transmitByte('0' + val % 10);
}

// Print a uint8_t as exactly two digits, zero-padded (e.g. 03, 59) -- used
// for calendar/clock fields so timestamps sort and align visually.
static void print_uint8_padded(uint8_t val) {
    transmitByte('0' + (val / 10) % 10);
    transmitByte('0' + val % 10);
}

// Print "YYYY-MM-DD HH:MM:SS". Century is assumed 20xx -- the DS3231 year
// register only stores a 2-digit offset (see rtc_time_t in ds3231.h).
static void print_timestamp(uint8_t year, uint8_t month, uint8_t date,
                             uint8_t hour, uint8_t minute, uint8_t second) {
    printString("20");
    print_uint8_padded(year);
    transmitByte('-');
    print_uint8_padded(month);
    transmitByte('-');
    print_uint8_padded(date);
    transmitByte(' ');
    print_uint8_padded(hour);
    transmitByte(':');
    print_uint8_padded(minute);
    transmitByte(':');
    print_uint8_padded(second);
}

// Decode and print a compressed temperature byte over UART.
// The LM75 9-bit reading is packed as: bits[7:1] = integer C, bit[0] = 0.5 C flag.
// Packing: temp_byte = (temp_high_byte << 1) | (temp_low_byte >> 7)
static void print_temperature(uint8_t temp_reading)
{
    print_uint8(temp_reading >> 1);     // integer part: drop the 0.5 C flag bit
    if (temp_reading & 1)
    {
        printString(".5 degrees\r\n");
    }
    else
    {
        printString(".0 degrees\r\n");
    }
}

// Print one full log line: "YYYY-MM-DD HH:MM:SS, XX.X degrees"
static void print_log_record(const log_record_t *rec)
{
    print_timestamp(rec->year, rec->month, rec->date, rec->hour, rec->minute, rec->second);
    printString(", ");
    print_temperature(rec->temp);
}

// Serialize a log_record_t to EEPROM one byte at a time. RECORD_SIZE (7) is
// well under the 24LC256's 64-byte page size, but 24lc256.h only exposes
// byte/word writes, so a page-write extension isn't needed at this cadence
// (readings are tens of seconds apart at minimum).
static void eeprom_write_record(uint16_t address, const log_record_t *rec)
{
    const uint8_t *bytes = (const uint8_t *)rec;
    for (uint8_t i = 0; i < RECORD_SIZE; i++)
    {
        EEPROM_write_byte(address + i, bytes[i]);
    }
}

static void eeprom_read_record(uint16_t address, log_record_t *rec)
{
    uint8_t *bytes = (uint8_t *)rec;
    for (uint8_t i = 0; i < RECORD_SIZE; i++)
    {
        bytes[i] = EEPROM_read_byte(address + i);
    }
}

// Prompt for one numeric field on its own line and read it with getNumber()
// (blocks until Enter, accepts up to 3 ASCII digits, 0-255 range -- USART.h).
static uint8_t prompt_field(const char *label)
{
    printString(label);
    uint8_t value = getNumber();
    printString("\r\n");
    return value;
}

// Interactively set the DS3231 from serial input. Only needs to run once --
// the module's coin-cell backup keeps time through power cycles after that.
static void set_clock(void)
{
    rtc_time_t t;

    printString("Enter each field then press Enter.\r\n");
    t.year   = prompt_field("Year (last two digits, e.g. 26 for 2026): ");
    t.month  = prompt_field("Month (1-12): ");
    t.date   = prompt_field("Date (1-31): ");
    t.hour   = prompt_field("Hour (0-23): ");
    t.minute = prompt_field("Minute (0-59): ");
    t.second = prompt_field("Second (0-59): ");

    ds3231_set_time(&t);

    printString("Clock set to ");
    print_timestamp(t.year, t.month, t.date, t.hour, t.minute, t.second);
    printString("\r\n");
}

int main(void)
{
    uint16_t seconds_delay = 0;             // seconds between successive readings
    uint16_t current_memory_location = 0;  // next write address in the 24LC256

    // ---- Init -------------------------------------------------------------
    clock_prescale_set(clock_div_1);    // Prescaler = 1 (no division). With LFUSE=0xF7, CPU runs at 16 MHz.
                                        // avr/power.h -- clock_prescale_set()

    init_I2C();     // Enable TWI peripheral; SDA=PC4, SCL=PC5 -- i2c.h (shared by LM75, 24LC256, DS3231)
    initUSART();    // Configure USART0 at 9600 8N1 (BAUD set in Makefile CPPFLAGS) -- USART.h

    LED_DDR |= (1 << LED0);    // LED0 (PB0) as output -- visual heartbeat during logging

    // ---- Load persisted config from 24LC256 --------------------------------
    // On a fresh chip these return 0xFFFF. Run [e] once to initialise them.
    seconds_delay           = EEPROM_read_word(SECONDS_POINTER);
    current_memory_location = EEPROM_read_word(CURRENT_LOCATION_POINTER);

    // ---- Menu prompt -------------------------------------------------------
    printString("*** Press [m] within ");
    print_uint8(MENU_DELAY);
    printString(" seconds to enter menu. ***\r\n");

    for (uint8_t i = 0; i < MENU_DELAY; i++)
    {
        _delay_ms(1000);
    }

    // Check if the user typed 'm' during the countdown.
    // UCSR0A RXC0 is set when unread data is in UDR0. Reading UDR0 consumes it.
    // ATmega328P datasheet p.194 -- UCSR0A register
    uint8_t enter_menu = (bit_is_set(UCSR0A, RXC0) && (UDR0 == 'm')) ? 1 : 0;

    // ---- Config menu -------------------------------------------------------
    while (enter_menu)
    {
        // Refresh each iteration -- [e] may have reset current_memory_location
        current_memory_location = EEPROM_read_word(CURRENT_LOCATION_POINTER);

        printString("\r\n====[ Logging Thermometer + DS3231 ]====\r\n ");
        print_uint16((current_memory_location - MEMORY_START) / RECORD_SIZE);
        printString(" readings in EEPROM.\r\n  ");
        print_uint16(seconds_delay);
        printString(" seconds between readings.\r\n");

        printString(" [<] to shorten sample delay time\r\n");
        printString(" [>] to increase sample delay time\r\n");
        printString(" [r] to reset delay time to 60 sec\r\n");
        printString(" [t] to set the clock (do this once -- battery-backed after that)\r\n");
        printString(" [p] to print out log over serial\r\n");
        printString(" [e] to erase memory\r\n");
        printString(" [s] to start logging\r\n\r\n");

        switch (receiveByte())
        {
            case 'p':
            {
                // Sequential read from MEMORY_START to the last written address,
                // one RECORD_SIZE-byte record at a time.
                log_record_t rec;
                for (uint16_t addr = MEMORY_START; addr < current_memory_location; addr += RECORD_SIZE)
                {
                    eeprom_read_record(addr, &rec);
                    print_log_record(&rec);
                }
                break;
            }

            case '<':
                if (seconds_delay >= 10)
                {
                    seconds_delay -= 5;
                    EEPROM_write_word(SECONDS_POINTER, seconds_delay);
                }
                break;

            case '>':
                if (seconds_delay < 65000)
                {
                    seconds_delay += 5;
                    EEPROM_write_word(SECONDS_POINTER, seconds_delay);
                }
                break;

            case 'r':
                seconds_delay = 60;
                EEPROM_write_word(SECONDS_POINTER, seconds_delay);
                break;

            case 't':
                set_clock();
                break;

            case 'e':
                printString("Clearing EEPROM. Please wait (~3 s).\r\n");
                EEPROM_clear_all();
                // Re-initialise both config words with sane defaults
                EEPROM_write_word(CURRENT_LOCATION_POINTER, MEMORY_START);
                EEPROM_write_word(SECONDS_POINTER, seconds_delay);
                current_memory_location = MEMORY_START;
                break;

            case 's':
                printString("OK, commencing logging...\r\n");
                enter_menu = 0;
                break;

            default:
                printString("Invalid selection.\r\n");
        }
    }

    // ---- Logging loop ------------------------------------------------------
    while (1)
    {
        current_memory_location = EEPROM_read_word(CURRENT_LOCATION_POINTER);

        log_record_t rec;
        rtc_time_t now;
        ds3231_read_time(&now);
        rec.year   = now.year;
        rec.month  = now.month;
        rec.date   = now.date;
        rec.hour   = now.hour;
        rec.minute = now.minute;
        rec.second = now.second;

        // I2C read sequence: write pointer register, repeated START, read 2 bytes.
        // See hello_world Theory.md for the full protocol explanation.
        i2c_start();
        i2c_send(LM75_ADDRESS_W);
        i2c_send(LM75_TEMP_REGISTER);
        i2c_start();                            // repeated START -- retain bus ownership
        i2c_send(LM75_ADDRESS_R);
        uint8_t temp_high_byte = i2c_read_ack();    // MSB: signed integer C
        uint8_t temp_low_byte  = i2c_read_no_ack(); // LSB: bit 7 = 0.5 C; NACK ends read
        i2c_stop();

        // Pack the 9-bit LM75 reading into one byte: integer C in bits[7:1], 0.5 C in bit[0].
        rec.temp = (temp_high_byte << 1) | (temp_low_byte >> 7);

        print_log_record(&rec);

        // Store the record, then advance the persistent address pointer -- but
        // only if a full RECORD_SIZE-byte record still fits before the chip ends.
        // (Written as "last byte <= max" rather than "+1 <= max+1" to avoid
        // overflowing EEPROM_BYTES_MAX, which is already the top of the range.)
        if (current_memory_location + RECORD_SIZE - 1 <= EEPROM_BYTES_MAX)
        {
            eeprom_write_record(current_memory_location, &rec);
            current_memory_location += RECORD_SIZE;
            EEPROM_write_word(CURRENT_LOCATION_POINTER, current_memory_location);
        }

        // Blink LED once per second during the inter-reading delay
        for (uint16_t i = 0; i < seconds_delay; i++)
        {
            _delay_ms(1000);
            LED_PORT ^= (1 << LED0);    // toggle -- visible heartbeat
        }
    }
    return 0;
}

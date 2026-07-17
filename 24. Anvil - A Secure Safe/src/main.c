/*
 * main.c
 *
 * Purpose : Init every subsystem in the required order, then run the
 *           non-blocking superloop dispatcher.
 * Hardware: all of it, indirectly, via the modules it initialises.
 * Datasheet: n/a — see each module's own file for register-level detail.
 */

#include "sys_tick.h"
#include "lcd_driver.h"
#include "ui.h"
#include "keypad.h"
#include "leds.h"
#include "buzzer.h"
#include "storage.h"
#include "anvil.h"

#include <avr/interrupt.h>

int main(void)
{
    /* Init order matters: sys_tick before anything that calls
     * sys_millis(); Lcd_Init before ui_load_glyphs (needs CGRAM writes);
     * storage_init before anvil_enter (first draw may need the PIN);
     * sei() only after every peripheral is fully configured; buzzer needs
     * no init call — its DDR bit is set lazily inside buzzer_tone_nb(). */
    sys_tick_init();
    (void)Lcd_Init();
    ui_load_glyphs();
    keypad_init();
    leds_init();
    storage_init();
    sei();

    anvil_enter(ST_SPLASH);

    for (;;) {
        keypad_service();
        buzzer_service();
        leds_service();
        anvil_tick();
    }
}

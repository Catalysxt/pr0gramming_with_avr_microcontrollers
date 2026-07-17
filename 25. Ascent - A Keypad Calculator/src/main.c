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
#include "keypad.h"
#include "buzzer.h"
#include "sleep.h"
#include "ascent.h"

#include <avr/interrupt.h>

int main(void)
{
    /* Init order matters: sys_tick before anything that calls
     * sys_millis(); keypad_init before the superloop starts scanning;
     * buzzer_init just sets its DDR bit (no timer armed yet);
     * sleep_init only touches PCICR/PCMSK1, safe pre-sei(); sei() only
     * after every peripheral is fully configured. */
    sys_tick_init();
    (void)Lcd_Init();
    keypad_init();
    buzzer_init();
    sleep_init();
    sei();

    ascent_enter(ST_SPLASH);

    for (;;) {
        keypad_service();
        buzzer_service();
        ascent_tick();
        if (ascent_check_idle_sleep()) {
            sleep_enter_power_down();
            ascent_redraw();
        }
    }
}

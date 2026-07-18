#include "face.h"
#include "animator.h"

void face_init(max7219_chain_t chain, uint8_t intensity) {
    max7219_chain_init(chain);                  /* SPI + chip config, blanked */
    max7219_chain_set_intensity(chain, intensity);
    animator_init(chain);                       /* paints the neutral frame */
}

void face_set_expression(expression_t e) {
    animator_set_target(e);
}

void face_blink(void) {
    animator_force_blink();
}

void face_tick(void) {
    animator_tick();
}

void face_flush(void) {
    animator_flush();
}

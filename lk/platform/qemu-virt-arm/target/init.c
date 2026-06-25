/*
 * Minimal target initialization for QEMU virt
 */
#include <lk/debug.h>
#include <target.h>
#include <stdbool.h>

void target_early_init(void) {
    /* Nothing to do for QEMU virt - platform handles everything */
}

void target_init(void) {
    /* Nothing to do for QEMU virt - platform handles everything */
}

void target_set_debug_led(unsigned int led, bool on) {
    /* No debug LEDs on QEMU virt - stub function */
}

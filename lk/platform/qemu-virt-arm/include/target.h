/*
 * Minimal target header for QEMU virt
 */
#pragma once

#include <stdbool.h>

void target_early_init(void);
void target_init(void);
void target_set_debug_led(unsigned int led, bool on);

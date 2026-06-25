/*
 * Minimal platform debug header for QEMU virt
 */
#pragma once

#include <stddef.h>
#include <stdbool.h>

/* UART output function - implemented in platform/debug.c */
void platform_dputc(char c);
int platform_dgetc(char *c, bool wait);
void platform_dputs_thread(const char *str, size_t len);
void platform_dputs_irq(const char *str, size_t len);
int platform_pgetc(char *c, bool wait);
void platform_pputc(char c);  // Panic output (same as dputc for QEMU)

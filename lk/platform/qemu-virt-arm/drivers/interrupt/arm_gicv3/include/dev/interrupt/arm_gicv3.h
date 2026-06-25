/*
 * Copyright (c) 2024
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

#include <sys/types.h>

__BEGIN_CDECLS

/* Initialize GICv3 - called once on boot CPU */
void arm_gicv3_init(void);

/* Initialize GICv3 for secondary CPU - must be called on each secondary CPU */
void arm_gicv3_init_percpu(void);

/* Send Software Generated Interrupt (SGI) for inter-processor communication */
status_t arm_gic_sgi(uint irq, uint flags, uint cpu_mask);

/* SGI flags */
#define ARM_GIC_SGI_FLAG_NS (1U << 15)

__END_CDECLS

#ifndef ARCH_IPI_H
#define ARCH_IPI_H

#include <stdint.h>

#include "arch_api/arch.h"
#include "arch_api/barrier.h"

static inline void arch_ipi_send(unsigned int cpu, unsigned int intid)
{
    uint64_t mpidr = arch_linear_cpu_to_mpidr(cpu);
    uint64_t aff0 = mpidr & 0xfU;
    uint64_t aff1 = (mpidr >> 8) & 0xffU;
    uint64_t aff2 = (mpidr >> 16) & 0xffU;
    uint64_t aff3 = (mpidr >> 32) & 0xffU;
    uint64_t sgi = (1ULL << aff0) |
                   (aff1 << 16) |
                   (((uint64_t)intid & 0xfULL) << 24) |
                   (aff2 << 32) |
                   (aff3 << 48);

    arch_mb();
    __asm__ volatile("msr S3_0_C12_C11_5, %0" :: "r"(sgi) : "memory");
    arch_isb();
}

static inline void arch_ipi_send_reschedule(unsigned int cpu)
{
    arch_ipi_send(cpu, 0U);
}

#endif /* ARCH_IPI_H */

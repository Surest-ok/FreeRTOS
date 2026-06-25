#ifndef ARCH_ARCH_H
#define ARCH_ARCH_H

#ifndef TX_PORT_H
#include <stdint.h>
#endif

static inline uint64_t arch_read_mpidr(void)
{
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return mpidr;
}

static inline unsigned int arch_cpu_id_from_mpidr(uint64_t mpidr)
{
    return (unsigned int)((mpidr & 0xffULL) + (((mpidr >> 8) & 0xffULL) << 4));
}

static inline unsigned int arch_curr_cpu_id(void)
{
    return arch_cpu_id_from_mpidr(arch_read_mpidr());
}

static inline uint64_t arch_linear_cpu_to_mpidr(unsigned int cpu)
{
    return (uint64_t)(cpu & 0xfU) | (((uint64_t)cpu >> 4) << 8);
}

static inline void arch_cpu_relax(void)
{
    __asm__ volatile("yield" ::: "memory");
}

static inline void arch_cpu_wait_for_interrupt(void)
{
    __asm__ volatile("wfi" ::: "memory");
}

static inline void arch_cpu_send_event(void)
{
    __asm__ volatile("sev" ::: "memory");
}

#endif /* ARCH_ARCH_H */

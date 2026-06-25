#ifndef ARCH_BARRIER_H
#define ARCH_BARRIER_H

#define arch_barrier()       __asm__ volatile("" ::: "memory")
#define arch_isb()           __asm__ volatile("isb" ::: "memory")
#define arch_dsb_sy()        __asm__ volatile("dsb sy" ::: "memory")
#define arch_dmb_sy()        __asm__ volatile("dmb sy" ::: "memory")
#define arch_mb()            arch_dsb_sy()
#define arch_rmb()           __asm__ volatile("dsb ld" ::: "memory")
#define arch_wmb()           __asm__ volatile("dsb st" ::: "memory")
#define arch_smp_mb()        __asm__ volatile("dmb ish" ::: "memory")
#define arch_smp_rmb()       __asm__ volatile("dmb ishld" ::: "memory")
#define arch_smp_wmb()       __asm__ volatile("dmb ishst" ::: "memory")

#endif /* ARCH_BARRIER_H */

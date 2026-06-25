/*
 * ARM64 atomic operation helpers for use with -mgeneral-regs-only
 */
#include <lk/asm.h>

.text

// uint32_t __aarch64_ldadd4_acq_rel(uint32_t val, uint32_t *ptr)
// Atomic add: *ptr += val, return old value
FUNCTION(__aarch64_ldadd4_acq_rel)
1:
    ldaxr w2, [x1]           // Load exclusive with acquire
    add w3, w2, w0           // Add val to old value
    stlxr w4, w3, [x1]       // Store exclusive with release
    cbnz w4, 1b              // Retry if store failed
    mov w0, w2               // Return old value
    ret
END_FUNCTION(__aarch64_ldadd4_acq_rel)

// uint32_t __aarch64_ldset4_relax(uint32_t val, uint32_t *ptr)
// Atomic OR: *ptr |= val, return old value
FUNCTION(__aarch64_ldset4_relax)
1:
    ldxr w2, [x1]            // Load exclusive
    orr w3, w2, w0           // OR val with old value
    stxr w4, w3, [x1]        // Store exclusive
    cbnz w4, 1b              // Retry if store failed
    mov w0, w2               // Return old value
    ret
END_FUNCTION(__aarch64_ldset4_relax)

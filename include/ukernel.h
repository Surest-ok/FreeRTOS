#ifndef UKERNEL_H
#define UKERNEL_H

#include <stdint.h>

typedef struct {
    uint32_t cores;
    uint64_t timer_hz;
} UkernelBootInfo;

void ukernel_start(void);
int ukernel_app_init(const UkernelBootInfo *boot);
void ukernel_panic(const char *reason);
void ukernel_wake_secondary_cores(void);
void ukernel_note_core_alive(uint32_t core_id);
uint64_t ukernel_core_seen_mask(void);
uint64_t ukernel_core_heartbeat(uint32_t core_id);

#endif /* UKERNEL_H */

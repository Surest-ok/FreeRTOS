#ifndef ARCH_SYSCALL_H
#define ARCH_SYSCALL_H

#include "arch_api/exception.h"

typedef int (*arch_syscall_handler_t)(arch_exception_frame_t *frame);

void arch_syscall_set_handler(arch_syscall_handler_t handler);
int arch_syscall_dispatch(arch_exception_frame_t *frame);

#endif /* ARCH_SYSCALL_H */

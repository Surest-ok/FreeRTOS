/*
 * CherrySyscall - POSIX-compatible System Call Subsystem for CherryOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Implements Linux AArch64 standard system call numbers and dispatcher.
 */

#ifndef _CHERRY_SYSCALL_H_
#define _CHERRY_SYSCALL_H_

#include <stdint.h>
#include <stddef.h>
#include "cherry_vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Standard Linux AArch64 System Call Numbers (__NR_*) */
#define __NR_getcwd             17
#define __NR_dup                23
#define __NR_dup3               24
#define __NR_fcntl              25
#define __NR_ioctl              29
#define __NR_mkdirat            34
#define __NR_unlinkat           35
#define __NR_renameat           38
#define __NR_stat               40
#define __NR_chdir              49
#define __NR_openat             56
#define __NR_close              57
#define __NR_pipe2              59
#define __NR_getdents64         61
#define __NR_lseek              62
#define __NR_read               63
#define __NR_write              64
#define __NR_readv              65
#define __NR_writev             66
#define __NR_newfstatat         79
#define __NR_fstat              80
#define __NR_exit               93
#define __NR_exit_group         94
#define __NR_set_tid_address    96
#define __NR_futex              98
#define __NR_nanosleep          101
#define __NR_clock_gettime      113
#define __NR_sched_yield        124
#define __NR_kill               129
#define __NR_rt_sigaction       134
#define __NR_rt_sigprocmask     135
#define __NR_uname              160
#define __NR_gettimeofday       169
#define __NR_getpid             172
#define __NR_getppid            173
#define __NR_getuid             174
#define __NR_geteuid            175
#define __NR_getgid             176
#define __NR_getegid            177
#define __NR_brk                214
#define __NR_munmap             215
#define __NR_mmap               222
#define __NR_mprotect           226
#define __NR_wait4              260
#define __NR_prlimit64          261
#define __NR_getrandom          278
#define __NR_statx              291
#define __NR_rseq               293

/* POSIX struct iovec */
struct iovec {
    void  *iov_base;
    size_t iov_len;
};

/* POSIX struct utsname */
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

/* POSIX struct timespec */
#ifndef _TIMESPEC_DECLARED
#define _TIMESPEC_DECLARED
struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};
#endif

#define CLOCK_REALTIME          0
#define CLOCK_MONOTONIC         1

#define WNOHANG                 1
#define WUNTRACED               2

/* =========================================================================
 * Central Syscall Dispatcher
 * ========================================================================= */

/**
 * Dispatch system call by number with up to 6 arguments (Linux ARM64 ABI).
 */
long cherry_syscall(long nr, long a0, long a1, long a2, long a3, long a4, long a5);

/* =========================================================================
 * Process-aware POSIX C System Call Wrappers
 * ========================================================================= */

int     sys_open(const char *path, int flags, mode_t mode);
int     sys_openat(int dirfd, const char *path, int flags, mode_t mode);
int     sys_close(int fd);
ssize_t sys_read(int fd, void *buf, size_t count);
ssize_t sys_write(int fd, const void *buf, size_t count);
off_t   sys_lseek(int fd, off_t offset, int whence);
int     sys_stat(const char *path, struct stat *buf);
int     sys_fstat(int fd, struct stat *buf);
int     sys_ioctl(int fd, unsigned long req, void *arg);
int     sys_pipe2(int pipefd[2], int flags);
int     sys_dup(int oldfd);
int     sys_dup2(int oldfd, int newfd);
int     sys_mkdir(const char *path, mode_t mode);
int     sys_unlink(const char *path);
int     sys_getcwd(char *buf, size_t size);
int     sys_chdir(const char *path);
pid_t   sys_getpid(void);
pid_t   sys_getppid(void);
void    sys_exit(int status);
pid_t   sys_wait4(pid_t pid, int *wstatus, int options, void *rusage);
int     sys_kill(pid_t pid, int sig);
int     sys_uname(struct utsname *buf);
int     sys_nanosleep(const struct timespec *req, struct timespec *rem);
int     sys_clock_gettime(int clk_id, struct timespec *tp);
void   *sys_brk(void *addr);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_SYSCALL_H_ */

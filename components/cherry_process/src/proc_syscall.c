/*
 * CherrySyscall - System Call Dispatcher & Implementations
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_syscall.h"
#include "cherry_process.h"
#include "cherry_pipe.h"
#include "uart.h"
#include <string.h>

#define AT_FDCWD    -100

static void resolve_path(const char *in_path, char *out_path, size_t max_len)
{
    if (!in_path || in_path[0] == '\0') {
        out_path[0] = '\0';
        return;
    }

    if (in_path[0] == '/') {
        strncpy(out_path, in_path, max_len - 1);
        out_path[max_len - 1] = '\0';
        return;
    }

    struct process *p = proc_current();
    const char *cwd = p ? p->cwd : "/";

    if (strcmp(cwd, "/") == 0) {
        snprintf(out_path, max_len, "/%s", in_path);
    } else {
        snprintf(out_path, max_len, "%s/%s", cwd, in_path);
    }
}

int sys_openat(int dirfd, const char *path, int flags, mode_t mode)
{
    if (!path) return -EINVAL;

    char full_path[VFS_PATH_MAX];
    if (path[0] == '/' || dirfd != AT_FDCWD) {
        strncpy(full_path, path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    } else {
        resolve_path(path, full_path, sizeof(full_path));
    }

    int vfs_fd = open(full_path, flags, mode);
    if (vfs_fd < 0) return vfs_fd;

    struct process *p = proc_current();
    int pfd = proc_fd_install(p, vfs_fd);
    if (pfd < 0) {
        close(vfs_fd);
        return pfd;
    }
    return pfd;
}

int sys_open(const char *path, int flags, mode_t mode)
{
    return sys_openat(AT_FDCWD, path, flags, mode);
}

int sys_close(int fd)
{
    struct process *p = proc_current();
    return proc_fd_close(p, fd);
}

ssize_t sys_read(int fd, void *buf, size_t count)
{
    struct process *p = proc_current();
    int vfs_fd = proc_fd_get(p, fd);
    if (vfs_fd < 0) return -EBADF;
    return read(vfs_fd, buf, count);
}

ssize_t sys_write(int fd, const void *buf, size_t count)
{
    struct process *p = proc_current();
    int vfs_fd = proc_fd_get(p, fd);
    if (vfs_fd < 0) return -EBADF;
    return write(vfs_fd, buf, count);
}

off_t sys_lseek(int fd, off_t offset, int whence)
{
    struct process *p = proc_current();
    int vfs_fd = proc_fd_get(p, fd);
    if (vfs_fd < 0) return -EBADF;
    return lseek(vfs_fd, offset, whence);
}

int sys_stat(const char *path, struct stat *buf)
{
    if (!path || !buf) return -EINVAL;
    char full_path[VFS_PATH_MAX];
    resolve_path(path, full_path, sizeof(full_path));
    return stat(full_path, buf);
}

int sys_fstat(int fd, struct stat *buf)
{
    if (!buf) return -EINVAL;
    struct process *p = proc_current();
    int vfs_fd = proc_fd_get(p, fd);
    if (vfs_fd < 0) return -EBADF;
    return fstat(vfs_fd, buf);
}

int sys_ioctl(int fd, unsigned long req, void *arg)
{
    struct process *p = proc_current();
    int vfs_fd = proc_fd_get(p, fd);
    if (vfs_fd < 0) return -EBADF;
    return ioctl(vfs_fd, (unsigned int)req, arg);
}

int sys_pipe2(int pipefd[2], int flags)
{
    (void)flags;
    if (!pipefd) return -EINVAL;

    int r_vfs = -1, w_vfs = -1;
    int ret = cherry_pipe_create(&r_vfs, &w_vfs);
    if (ret != 0) return ret;

    struct process *p = proc_current();
    int pfd0 = proc_fd_install(p, r_vfs);
    if (pfd0 < 0) {
        close(r_vfs);
        close(w_vfs);
        return pfd0;
    }

    int pfd1 = proc_fd_install(p, w_vfs);
    if (pfd1 < 0) {
        proc_fd_close(p, pfd0);
        close(w_vfs);
        return pfd1;
    }

    pipefd[0] = pfd0;
    pipefd[1] = pfd1;
    return 0;
}

int sys_dup(int oldfd)
{
    struct process *p = proc_current();
    int vfs_fd = proc_fd_get(p, oldfd);
    if (vfs_fd < 0) return -EBADF;

    return proc_fd_install(p, vfs_fd);
}

int sys_dup2(int oldfd, int newfd)
{
    if (newfd < 0 || newfd >= PROC_MAX_FDS) return -EBADF;
    struct process *p = proc_current();
    int vfs_fd = proc_fd_get(p, oldfd);
    if (vfs_fd < 0) return -EBADF;

    if (oldfd == newfd) return newfd;

    if (p->fd_table[newfd] >= 0) {
        close(p->fd_table[newfd]);
    }
    p->fd_table[newfd] = vfs_fd;
    return newfd;
}

int sys_mkdir(const char *path, mode_t mode)
{
    if (!path) return -EINVAL;
    char full_path[VFS_PATH_MAX];
    resolve_path(path, full_path, sizeof(full_path));
    return mkdir(full_path, mode);
}

int sys_unlink(const char *path)
{
    if (!path) return -EINVAL;
    char full_path[VFS_PATH_MAX];
    resolve_path(path, full_path, sizeof(full_path));
    return unlink(full_path);
}

int sys_getcwd(char *buf, size_t size)
{
    return proc_get_cwd(buf, size);
}

int sys_chdir(const char *path)
{
    if (!path) return -EINVAL;
    char full_path[VFS_PATH_MAX];
    resolve_path(path, full_path, sizeof(full_path));
    return proc_set_cwd(full_path);
}

pid_t sys_getpid(void)
{
    return proc_getpid();
}

pid_t sys_getppid(void)
{
    return proc_getppid();
}

void sys_exit(int status)
{
    proc_exit(status);
}

pid_t sys_wait4(pid_t pid, int *wstatus, int options, void *rusage)
{
    (void)rusage;
    return proc_waitpid(pid, wstatus, options);
}

int sys_kill(pid_t pid, int sig)
{
    return proc_kill(pid, sig);
}

int sys_uname(struct utsname *buf)
{
    if (!buf) return -EINVAL;
    strncpy(buf->sysname, "CherryOS", sizeof(buf->sysname) - 1);
    strncpy(buf->nodename, "cherry-smp", sizeof(buf->nodename) - 1);
    strncpy(buf->release, "5.10.0-cherry", sizeof(buf->release) - 1);
    strncpy(buf->version, "#1 SMP FreeRTOS AArch64", sizeof(buf->version) - 1);
    strncpy(buf->machine, "aarch64", sizeof(buf->machine) - 1);
    buf->domainname[0] = '\0';
    return 0;
}

int sys_nanosleep(const struct timespec *req, struct timespec *rem)
{
    (void)rem;
    if (!req) return -EINVAL;
    uint32_t ms = (uint32_t)(req->tv_sec * 1000 + req->tv_nsec / 1000000);
    if (ms == 0 && req->tv_nsec > 0) ms = 1;
    kal_thread_sleep_ms(ms);
    return 0;
}

int sys_clock_gettime(int clk_id, struct timespec *tp)
{
    (void)clk_id;
    if (!tp) return -EINVAL;
    uint64_t ns = ktime_get_real_ns();
    tp->tv_sec = (int64_t)(ns / 1000000000ULL);
    tp->tv_nsec = (int64_t)(ns % 1000000000ULL);
    return 0;
}

void *sys_brk(void *addr)
{
    struct process *p = proc_current();
    if (!p) return NULL;

    if (!addr) {
        return (void *)p->brk;
    }
    p->brk = (uintptr_t)addr;
    return addr;
}

long cherry_syscall(long nr, long a0, long a1, long a2, long a3, long a4, long a5)
{
    (void)a4;
    (void)a5;

    switch (nr) {
    case __NR_getcwd:
        return (long)sys_getcwd((char *)a0, (size_t)a1);
    case __NR_dup:
        return (long)sys_dup((int)a0);
    case __NR_dup3:
        return (long)sys_dup2((int)a0, (int)a1);
    case __NR_ioctl:
        return (long)sys_ioctl((int)a0, (unsigned long)a1, (void *)a2);
    case __NR_mkdirat:
        return (long)sys_mkdir((const char *)a1, (mode_t)a2);
    case __NR_unlinkat:
        return (long)sys_unlink((const char *)a1);
    case __NR_stat:
        return (long)sys_stat((const char *)a0, (struct stat *)a1);
    case __NR_chdir:
        return (long)sys_chdir((const char *)a0);
    case __NR_openat:
        return (long)sys_openat((int)a0, (const char *)a1, (int)a2, (mode_t)a3);
    case __NR_close:
        return (long)sys_close((int)a0);
    case __NR_pipe2:
        return (long)sys_pipe2((int *)a0, (int)a1);
    case __NR_lseek:
        return (long)sys_lseek((int)a0, (off_t)a1, (int)a2);
    case __NR_read:
        return (long)sys_read((int)a0, (void *)a1, (size_t)a2);
    case __NR_write:
        return (long)sys_write((int)a0, (const void *)a1, (size_t)a2);
    case __NR_fstat:
        return (long)sys_fstat((int)a0, (struct stat *)a1);
    case __NR_exit:
    case __NR_exit_group:
        sys_exit((int)a0);
        return 0;
    case __NR_nanosleep:
        return (long)sys_nanosleep((const struct timespec *)a0, (struct timespec *)a1);
    case __NR_clock_gettime:
        return (long)sys_clock_gettime((int)a0, (struct timespec *)a1);
    case __NR_sched_yield:
        kal_thread_yield();
        return 0;
    case __NR_kill:
        return (long)sys_kill((pid_t)a0, (int)a1);
    case __NR_uname:
        return (long)sys_uname((struct utsname *)a0);
    case __NR_getpid:
        return (long)sys_getpid();
    case __NR_getppid:
        return (long)sys_getppid();
    case __NR_getuid:
    case __NR_geteuid:
    case __NR_getgid:
    case __NR_getegid:
        return 0;
    case __NR_brk:
        return (long)sys_brk((void *)a0);
    case __NR_wait4:
        return (long)sys_wait4((pid_t)a0, (int *)a1, (int)a2, (void *)a3);
    default:
        return -ENOSYS;
    }
}

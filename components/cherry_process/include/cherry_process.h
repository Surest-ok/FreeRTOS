/*
 * CherryProcess - Lightweight Process Subsystem for CherryOS & FreeRTOS SMP
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Provides:
 *  - Process Control Block (PCB) with PID, PPID, state, per-process FD table, CWD
 *  - Process lifecycle: spawn, exit, waitpid, kill, signal handling
 *  - Multi-core SMP process tracking and accounting
 *  - POSIX process semantics on top of FreeRTOS SMP tasks
 */

#ifndef _CHERRY_PROCESS_H_
#define _CHERRY_PROCESS_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cherry_kal.h"
#include "cherry_vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROC_MAX_COUNT          32
#define PROC_MAX_FDS            32
#define PROC_NAME_LEN           32
#define PROC_MAX_ARGS           32
#define PROC_MAX_ENVP           16
#define PROC_STACK_WORDS        2048
#define PROC_DEFAULT_PRIO       2

/* Process States */
typedef enum {
    PROC_STATE_UNUSED = 0,
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_SLEEPING,
    PROC_STATE_ZOMBIE,
    PROC_STATE_DEAD
} proc_state_t;

/* Process Entry Prototype */
typedef int (*proc_entry_fn)(int argc, char **argv);

/**
 * Process Control Block (PCB)
 */
struct process {
    pid_t               pid;                /* Process ID (>0) */
    pid_t               ppid;               /* Parent Process ID */
    proc_state_t        state;              /* Lifecycle state */
    char                name[PROC_NAME_LEN];/* Process / command name */
    int                 exit_code;          /* Termination exit code */
    uint32_t            core_id;            /* Assigned / active CPU core */
    struct task_struct *task;               /* Bound OS thread */
    
    /* Per-Process File Descriptors (index 0..31)
     * Maps process fd to global VFS file descriptor (or -1 if closed) */
    int                 fd_table[PROC_MAX_FDS];

    /* Current Working Directory */
    char                cwd[VFS_PATH_MAX];

    /* Process Memory & Heap */
    uintptr_t           brk;
    uintptr_t           heap_start;
    uintptr_t           heap_end;

    /* Arguments and Environment */
    int                 argc;
    char               *argv[PROC_MAX_ARGS];
    char               *envp[PROC_MAX_ENVP];

    /* Process Entry Point */
    proc_entry_fn       entry;

    /* Synchronization for parent waitpid() */
    struct semaphore    wait_sem;

    /* Executable and Context */
    char                exec_path[VFS_PATH_MAX];/* Executable full path */
    void               *priv;                   /* Private process context / loader data */

    /* Accounting & Timing */
    uint64_t            start_time_ms;
    uint64_t            cpu_time_ms;
};

/* =========================================================================
 * Process Lifecycle Management APIs
 * ========================================================================= */

/**
 * Initialize the process subsystem, creating the initial PID 1 system process.
 */
int cherry_process_init(void);

/**
 * Spawn a new process from an entry function with specified arguments and stdio fds.
 *
 * @param name Command / process name
 * @param entry Function pointer to execute
 * @param argc Argument count
 * @param argv Argument string array
 * @param envp Environment array (can be NULL)
 * @param stdin_vfs_fd  VFS fd for process stdin (if <0, opens /dev/console or inherits)
 * @param stdout_vfs_fd VFS fd for process stdout (if <0, opens /dev/console or inherits)
 * @param stderr_vfs_fd VFS fd for process stderr (if <0, opens /dev/console or inherits)
 * @return PID of the new process on success, or negative error code
 */
pid_t proc_spawn(const char *name, proc_entry_fn entry, int argc, char **argv, char **envp,
                 int stdin_vfs_fd, int stdout_vfs_fd, int stderr_vfs_fd);

/**
 * Spawn a new process from an entry function with private context pointer.
 */
pid_t proc_spawn_ex(const char *name, proc_entry_fn entry, int argc, char **argv, char **envp,
                    int stdin_vfs_fd, int stdout_vfs_fd, int stderr_vfs_fd, void *priv);

/**
 * Retrieve the PCB of the currently executing process.
 */
struct process *proc_current(void);

/**
 * Look up process by PID.
 */
struct process *proc_get_by_pid(pid_t pid);

/**
 * Terminate the calling process and set its exit code.
 */
void proc_exit(int exit_code);

/**
 * Wait for child process to terminate.
 *
 * @param pid Child PID to wait for (-1 for any child)
 * @param status Pointer to store exit status
 * @param options WNOHANG, etc.
 * @return Terminated child PID on success, 0 if WNOHANG and not ready, or negative error
 */
pid_t proc_waitpid(pid_t pid, int *status, int options);

/**
 * Send a signal to a process.
 */
int proc_kill(pid_t pid, int sig);

/**
 * Get current process ID.
 */
pid_t proc_getpid(void);

/**
 * Get parent process ID.
 */
pid_t proc_getppid(void);

/**
 * Get current working directory of current process.
 */
int proc_get_cwd(char *buf, size_t size);

/**
 * Set current working directory of current process.
 */
int proc_set_cwd(const char *path);

/**
 * Install a global VFS fd into process's per-process FD table.
 *
 * @param proc Target process
 * @param vfs_fd Global VFS file descriptor
 * @return Allocated process FD (0..31), or -EMFILE
 */
int proc_fd_install(struct process *proc, int vfs_fd);

/**
 * Close and free a per-process file descriptor.
 */
int proc_fd_close(struct process *proc, int pfd);

/**
 * Get the underlying global VFS fd from process fd.
 */
int proc_fd_get(struct process *proc, int pfd);

/**
 * Iterate over all active processes.
 */
int proc_for_each(int (*cb)(struct process *p, void *arg), void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_PROCESS_H_ */

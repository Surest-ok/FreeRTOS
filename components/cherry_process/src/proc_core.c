/*
 * CherryProcess - Process Subsystem Core Implementation
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_process.h"
#include "cherry_syscall.h"
#include "cherry_printk.h"
#include <string.h>

static struct process  g_process_table[PROC_MAX_COUNT];
static struct mutex    g_proc_lock;
static pid_t           g_next_pid = 1;
static bool            g_proc_initialized = false;

static void proc_exit_internal(struct process *p, int exit_code);

static int proc_task_wrapper(void *arg)
{
    struct process *p = (struct process *)arg;
    if (!p) return -1;

    p->state = PROC_STATE_RUNNING;
    p->core_id = smp_processor_id();
    p->start_time_ms = ktime_get_ms();

    int ret = 0;
    if (p->entry) {
        ret = p->entry(p->argc, p->argv);
    }

    proc_exit_internal(p, ret);
    kthread_exit(0);
    return 0;
}

int cherry_process_init(void)
{
    if (g_proc_initialized) {
        return 0;
    }

    mutex_init(&g_proc_lock);
    memset(g_process_table, 0, sizeof(g_process_table));

    /* Initialize PID 1 (Init / Kernel Process) */
    struct process *init_p = &g_process_table[0];
    init_p->pid = g_next_pid++;
    init_p->ppid = 0;
    init_p->state = PROC_STATE_RUNNING;
    strncpy(init_p->name, "init", PROC_NAME_LEN - 1);
    init_p->core_id = smp_processor_id();
    init_p->task = kthread_current();
    strncpy(init_p->cwd, "/", VFS_PATH_MAX - 1);
    sema_init(&init_p->wait_sem, 0);

    /* Open default stdio for init process */
    int console_r = open("/dev/console", O_RDONLY);
    int console_w1 = open("/dev/console", O_WRONLY);
    int console_w2 = open("/dev/console", O_WRONLY);

    init_p->fd_table[0] = (console_r >= 0) ? console_r : 0;
    init_p->fd_table[1] = (console_w1 >= 0) ? console_w1 : 1;
    init_p->fd_table[2] = (console_w2 >= 0) ? console_w2 : 2;

    for (int i = 3; i < PROC_MAX_FDS; i++) {
        init_p->fd_table[i] = -1;
    }

    g_proc_initialized = true;
    extern void proc_register_shell_commands(void);
    proc_register_shell_commands();

    pr_info("cherry_process: initialized process subsystem (pid=1 'init' running on cpu%u)\n",
            (unsigned int)init_p->core_id);
    return 0;
}

struct process *proc_current(void)
{
    if (!g_proc_initialized) {
        cherry_process_init();
    }

    struct task_struct *cur = kthread_current();

    mutex_lock(&g_proc_lock);
    for (int i = 0; i < PROC_MAX_COUNT; i++) {
        if (g_process_table[i].state != PROC_STATE_UNUSED &&
            g_process_table[i].task && cur &&
            g_process_table[i].task->handle == cur->handle) {
            mutex_unlock(&g_proc_lock);
            return &g_process_table[i];
        }
    }
    mutex_unlock(&g_proc_lock);

    /* Fallback to PID 1 */
    return &g_process_table[0];
}

struct process *proc_get_by_pid(pid_t pid)
{
    if (pid <= 0) return NULL;

    mutex_lock(&g_proc_lock);
    for (int i = 0; i < PROC_MAX_COUNT; i++) {
        if (g_process_table[i].state != PROC_STATE_UNUSED &&
            g_process_table[i].pid == pid) {
            mutex_unlock(&g_proc_lock);
            return &g_process_table[i];
        }
    }
    mutex_unlock(&g_proc_lock);
    return NULL;
}

pid_t proc_spawn_ex(const char *name, proc_entry_fn entry, int argc, char **argv, char **envp,
                    int stdin_vfs_fd, int stdout_vfs_fd, int stderr_vfs_fd, void *priv)
{
    if (!name || !entry) {
        return -EINVAL;
    }

    struct process *parent = proc_current();

    mutex_lock(&g_proc_lock);

    /* Find free slot in process table */
    struct process *child = NULL;
    for (int i = 0; i < PROC_MAX_COUNT; i++) {
        if (g_process_table[i].state == PROC_STATE_UNUSED) {
            child = &g_process_table[i];
            break;
        }
    }

    if (!child) {
        mutex_unlock(&g_proc_lock);
        return -ENOMEM;
    }

    memset(child, 0, sizeof(struct process));
    child->pid = g_next_pid++;
    child->ppid = parent ? parent->pid : 1;
    child->state = PROC_STATE_READY;
    child->entry = entry;
    child->priv = priv;
    strncpy(child->name, name, PROC_NAME_LEN - 1);
    strncpy(child->exec_path, name, VFS_PATH_MAX - 1);
    sema_init(&child->wait_sem, 0);

    /* Inherit CWD */
    if (parent) {
        strncpy(child->cwd, parent->cwd, VFS_PATH_MAX - 1);
    } else {
        strncpy(child->cwd, "/", VFS_PATH_MAX - 1);
    }

    /* Setup stdio file descriptors */
    child->fd_table[0] = (stdin_vfs_fd >= 0) ? stdin_vfs_fd : (parent ? parent->fd_table[0] : 0);
    child->fd_table[1] = (stdout_vfs_fd >= 0) ? stdout_vfs_fd : (parent ? parent->fd_table[1] : 1);
    child->fd_table[2] = (stderr_vfs_fd >= 0) ? stderr_vfs_fd : (parent ? parent->fd_table[2] : 2);

    for (int i = 3; i < PROC_MAX_FDS; i++) {
        child->fd_table[i] = -1;
    }

    /* Copy arguments */
    child->argc = (argc < PROC_MAX_ARGS) ? argc : PROC_MAX_ARGS;
    for (int i = 0; i < child->argc; i++) {
        if (argv && argv[i]) {
            size_t len = strlen(argv[i]) + 1;
            child->argv[i] = (char *)kmalloc(len, GFP_KERNEL);
            if (child->argv[i]) {
                memcpy(child->argv[i], argv[i], len);
            }
        } else {
            child->argv[i] = NULL;
        }
    }

    /* Copy environment */
    if (envp) {
        int env_idx = 0;
        while (envp[env_idx] && env_idx < (PROC_MAX_ENVP - 1)) {
            size_t len = strlen(envp[env_idx]) + 1;
            child->envp[env_idx] = (char *)kmalloc(len, GFP_KERNEL);
            if (child->envp[env_idx]) {
                memcpy(child->envp[env_idx], envp[env_idx], len);
            }
            env_idx++;
        }
        child->envp[env_idx] = NULL;
    }

    /* Create thread for process via decoupled KAL */
    child->task = kthread_run((kthread_fn_t)proc_task_wrapper, child, child->name);

    if (!child->task) {
        /* Cleanup allocated argv/envp */
        for (int i = 0; i < child->argc; i++) {
            if (child->argv[i]) kfree(child->argv[i]);
        }
        for (int i = 0; i < PROC_MAX_ENVP; i++) {
            if (child->envp[i]) kfree(child->envp[i]);
        }
        child->state = PROC_STATE_UNUSED;
        mutex_unlock(&g_proc_lock);
        return -EIO;
    }

    pid_t new_pid = child->pid;
    mutex_unlock(&g_proc_lock);
    return new_pid;
}

pid_t proc_spawn(const char *name, proc_entry_fn entry, int argc, char **argv, char **envp,
                 int stdin_vfs_fd, int stdout_vfs_fd, int stderr_vfs_fd)
{
    return proc_spawn_ex(name, entry, argc, argv, envp, stdin_vfs_fd, stdout_vfs_fd, stderr_vfs_fd, NULL);
}

static void proc_exit_internal(struct process *p, int exit_code)
{
    if (!p) return;

    mutex_lock(&g_proc_lock);
    p->exit_code = exit_code;
    p->state = PROC_STATE_ZOMBIE;
    p->cpu_time_ms = ktime_get_ms() - p->start_time_ms;

    /* Close private file descriptors (above stdio) */
    for (int i = 3; i < PROC_MAX_FDS; i++) {
        if (p->fd_table[i] >= 0) {
            close(p->fd_table[i]);
            p->fd_table[i] = -1;
        }
    }

    /* Wake up waiting parent */
    up(&p->wait_sem);
    mutex_unlock(&g_proc_lock);
}

void proc_exit(int exit_code)
{
    struct process *p = proc_current();
    proc_exit_internal(p, exit_code);
}

pid_t proc_waitpid(pid_t pid, int *status, int options)
{
    struct process *parent = proc_current();
    if (!parent) return -ESRCH;

    for (;;) {
        struct process *target = NULL;

        mutex_lock(&g_proc_lock);
        for (int i = 0; i < PROC_MAX_COUNT; i++) {
            if (g_process_table[i].state != PROC_STATE_UNUSED &&
                g_process_table[i].ppid == parent->pid) {
                if (pid == -1 || g_process_table[i].pid == pid) {
                    target = &g_process_table[i];
                    break;
                }
            }
        }

        if (!target) {
            mutex_unlock(&g_proc_lock);
            return -ECHILD;
        }

        /* If child has exited */
        if (target->state == PROC_STATE_ZOMBIE || target->state == PROC_STATE_DEAD) {
            pid_t child_pid = target->pid;
            if (status) {
                *status = target->exit_code;
            }

            /* Free argv and envp buffers */
            for (int a = 0; a < target->argc; a++) {
                if (target->argv[a]) kfree(target->argv[a]);
            }
            for (int e = 0; e < PROC_MAX_ENVP; e++) {
                if (target->envp[e]) kfree(target->envp[e]);
            }

            if (target->task) {
                kfree(target->task);
                target->task = NULL;
            }

            if (target->priv) {
                kfree(target->priv);
                target->priv = NULL;
            }

            sema_destroy(&target->wait_sem);
            memset(target, 0, sizeof(struct process));
            target->state = PROC_STATE_UNUSED;

            mutex_unlock(&g_proc_lock);
            return child_pid;
        }

        mutex_unlock(&g_proc_lock);

        /* Child still running */
        if (options & WNOHANG) {
            return 0;
        }

        /* Wait for child termination signal */
        down(&target->wait_sem);
    }
}

int proc_kill(pid_t pid, int sig)
{
    struct process *p = proc_get_by_pid(pid);
    if (!p) return -ESRCH;

    if (sig == 9) { /* SIGKILL */
        mutex_lock(&g_proc_lock);
        if (p->state == PROC_STATE_RUNNING || p->state == PROC_STATE_READY || p->state == PROC_STATE_SLEEPING) {
            if (p->task) {
                kthread_delete(p->task);
            }
            p->exit_code = 128 + 9;
            p->state = PROC_STATE_ZOMBIE;
            up(&p->wait_sem);
        }
        mutex_unlock(&g_proc_lock);
        return 0;
    }

    return 0;
}

pid_t proc_getpid(void)
{
    struct process *p = proc_current();
    return p ? p->pid : 1;
}

pid_t proc_getppid(void)
{
    struct process *p = proc_current();
    return p ? p->ppid : 0;
}

int proc_get_cwd(char *buf, size_t size)
{
    if (!buf || size == 0) return -EINVAL;
    struct process *p = proc_current();
    if (!p) return -ESRCH;

    strncpy(buf, p->cwd, size - 1);
    buf[size - 1] = '\0';
    return 0;
}

int proc_set_cwd(const char *path)
{
    if (!path) return -EINVAL;
    struct process *p = proc_current();
    if (!p) return -ESRCH;

    /* Verify path exists and is a directory */
    struct stat st;
    int ret = stat(path, &st);
    if (ret != 0) return ret;
    if (!S_ISDIR(st.st_mode)) return -ENOTDIR;

    strncpy(p->cwd, path, VFS_PATH_MAX - 1);
    p->cwd[VFS_PATH_MAX - 1] = '\0';
    return 0;
}

int proc_fd_install(struct process *proc, int vfs_fd)
{
    if (!proc || vfs_fd < 0) return -EINVAL;

    for (int i = 3; i < PROC_MAX_FDS; i++) {
        if (proc->fd_table[i] < 0) {
            proc->fd_table[i] = vfs_fd;
            return i;
        }
    }
    return -EMFILE;
}

int proc_fd_close(struct process *proc, int pfd)
{
    if (!proc || pfd < 0 || pfd >= PROC_MAX_FDS) return -EBADF;
    if (proc->fd_table[pfd] < 0) return -EBADF;

    int vfs_fd = proc->fd_table[pfd];
    proc->fd_table[pfd] = -1;
    return close(vfs_fd);
}

int proc_fd_get(struct process *proc, int pfd)
{
    if (!proc || pfd < 0 || pfd >= PROC_MAX_FDS) return -EBADF;
    return proc->fd_table[pfd];
}

int proc_for_each(int (*cb)(struct process *p, void *arg), void *arg)
{
    if (!cb) return -EINVAL;

    mutex_lock(&g_proc_lock);
    for (int i = 0; i < PROC_MAX_COUNT; i++) {
        if (g_process_table[i].state != PROC_STATE_UNUSED) {
            int ret = cb(&g_process_table[i], arg);
            if (ret != 0) {
                mutex_unlock(&g_proc_lock);
                return ret;
            }
        }
    }
    mutex_unlock(&g_proc_lock);
    return 0;
}

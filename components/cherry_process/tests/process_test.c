/*
 * CherryProcess Acceptance Test Suite
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "process_test.h"
#include "cherry_process.h"
#include "cherry_syscall.h"
#include "cherry_busybox.h"
#include "cherry_printk.h"
#include <string.h>

static int dummy_worker_fn(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return 42;
}

static int fd_worker_fn(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int fd = sys_open("/cherry_proc_test/proc_fd_test.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 1;

    const char *msg = "hello from child process fd";
    ssize_t w = sys_write(fd, msg, strlen(msg));
    sys_close(fd);

    return (w == (ssize_t)strlen(msg)) ? 0 : 2;
}

static int smp_worker_fn(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kal_thread_sleep_ms(50);
    return 0;
}

int cherry_process_run_tests(void)
{
    pr_info("cherry_process: ==> starting cherryprocess acceptance test suite...\n");

    bool pass_spawn    = false;
    bool pass_wait     = false;
    bool pass_fd       = false;
    bool pass_pipe     = false;
    bool pass_syscall  = false;
    bool pass_applets  = false;
    bool pass_sh       = false;
    bool pass_smp_ps   = false;

    sys_mkdir("/cherry_proc_test", 0755);

    /* 1. Test Process Spawn */
    char *args1[] = { (char *)"dummy", NULL };
    pid_t pid1 = proc_spawn("dummy_worker", dummy_worker_fn, 1, args1, NULL, -1, -1, -1);
    if (pid1 > 1) {
        pass_spawn = true;
    }

    /* 2. Test Process Exit & Waitpid */
    if (pass_spawn) {
        int status = 0;
        pid_t waited = proc_waitpid(pid1, &status, 0);
        if (waited == pid1 && status == 42) {
            pass_wait = true;
        }
    }

    /* 3. Test Per-process File Descriptors */
    char *args_fd[] = { (char *)"fd_worker", NULL };
    pid_t pid_fd = proc_spawn("fd_worker", fd_worker_fn, 1, args_fd, NULL, -1, -1, -1);
    if (pid_fd > 0) {
        int st_fd = 0;
        proc_waitpid(pid_fd, &st_fd, 0);
        if (st_fd == 0) {
            int verify_fd = sys_open("/cherry_proc_test/proc_fd_test.tmp", O_RDONLY, 0);
            if (verify_fd >= 0) {
                char buf[64];
                ssize_t r = sys_read(verify_fd, buf, sizeof(buf) - 1);
                sys_close(verify_fd);
                if (r > 0) {
                    buf[r] = '\0';
                    if (strstr(buf, "hello from child process fd")) {
                        pass_fd = true;
                    }
                }
            }
        }
    }

    /* 4. Test In-Memory Pipe & Redirection */
    int pipefd[2];
    if (sys_pipe2(pipefd, 0) == 0) {
        const char *pipe_msg = "cherry_pipe_message_stream_test";
        ssize_t w = sys_write(pipefd[1], pipe_msg, strlen(pipe_msg));
        char read_buf[64];
        memset(read_buf, 0, sizeof(read_buf));
        ssize_t r = sys_read(pipefd[0], read_buf, sizeof(read_buf) - 1);

        sys_close(pipefd[0]);
        sys_close(pipefd[1]);

        if (w == (ssize_t)strlen(pipe_msg) && r == w && strcmp(read_buf, pipe_msg) == 0) {
            pass_pipe = true;
        }
    }

    /* 5. Test POSIX System Call Dispatcher (cherry_syscall) */
    long cur_pid = cherry_syscall(__NR_getpid, 0, 0, 0, 0, 0, 0);
    struct utsname un;
    long un_ret = cherry_syscall(__NR_uname, (long)&un, 0, 0, 0, 0, 0);
    char cwd_buf[128];
    long cwd_ret = cherry_syscall(__NR_getcwd, (long)cwd_buf, (long)sizeof(cwd_buf), 0, 0, 0, 0);

    if (cur_pid > 0 && un_ret == 0 && strcmp(un.sysname, "CherryOS") == 0 && cwd_ret == 0 && strlen(cwd_buf) > 0) {
        pass_syscall = true;
    }

    /* 6. Test BusyBox Applets via Process Execution */
    int ret_echo = busybox_run_cmd("echo 'busybox test line' > /cherry_proc_test/bb_test.txt");
    if (ret_echo == 0) {
        int vfd = sys_open("/cherry_proc_test/bb_test.txt", O_RDONLY, 0);
        if (vfd >= 0) {
            char tbuf[64];
            ssize_t tr = sys_read(vfd, tbuf, sizeof(tbuf) - 1);
            sys_close(vfd);
            if (tr > 0) {
                tbuf[tr] = '\0';
                if (strstr(tbuf, "busybox test line")) {
                    pass_applets = true;
                }
            }
        }
    }

    /* 7. Test BusyBox Shell Pipelines */
    int ret_sh = busybox_run_cmd("echo pipeline_ok | cat");
    if (ret_sh == 0) {
        pass_sh = true;
    }

    /* 8. Test SMP Concurrent Processes & ps Report */
    char *smp_args[] = { (char *)"smp_worker", NULL };
    pid_t smp1 = proc_spawn("smp_worker1", smp_worker_fn, 1, smp_args, NULL, -1, -1, -1);
    pid_t smp2 = proc_spawn("smp_worker2", smp_worker_fn, 1, smp_args, NULL, -1, -1, -1);

    if (smp1 > 0 && smp2 > 0) {
        char *ps_argv[] = { (char *)"ps", NULL };
        int ps_ret = applet_ps(1, ps_argv);

        int st1 = 0, st2 = 0;
        proc_waitpid(smp1, &st1, 0);
        proc_waitpid(smp2, &st2, 0);

        if (ps_ret == 0) {
            pass_smp_ps = true;
        }
    }

    bool all_pass = pass_spawn && pass_wait && pass_fd && pass_pipe &&
                    pass_syscall && pass_applets && pass_sh && pass_smp_ps;

    pr_info("cherry_process: [process-verify] spawn=%s wait=%s fd=%s pipe=%s syscall=%s applets=%s sh=%s smp_ps=%s all=%s\n",
            pass_spawn ? "pass" : "fail",
            pass_wait ? "pass" : "fail",
            pass_fd ? "pass" : "fail",
            pass_pipe ? "pass" : "fail",
            pass_syscall ? "pass" : "fail",
            pass_applets ? "pass" : "fail",
            pass_sh ? "pass" : "fail",
            pass_smp_ps ? "pass" : "fail",
            all_pass ? "pass" : "fail");

    return all_pass ? 0 : -1;
}

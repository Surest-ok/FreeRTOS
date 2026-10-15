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
#include "cherry_elf.h"
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
    bool pass_elf      = false;

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

    /* 9. Test Complete ELF64 Loader, glibc Syscalls & Trampoline Execution */
    uint8_t dummy_elf[sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) + 64];
    memset(dummy_elf, 0, sizeof(dummy_elf));

    Elf64_Ehdr *eh = (Elf64_Ehdr *)dummy_elf;
    eh->e_ident[EI_MAG0] = ELFMAG0;
    eh->e_ident[EI_MAG1] = ELFMAG1;
    eh->e_ident[EI_MAG2] = ELFMAG2;
    eh->e_ident[EI_MAG3] = ELFMAG3;
    eh->e_ident[EI_CLASS] = ELFCLASS64;
    eh->e_ident[EI_DATA] = ELFDATA2LSB;
    eh->e_ident[EI_VERSION] = EV_CURRENT;
    eh->e_type = ET_DYN; /* PIE */
    eh->e_machine = EM_AARCH64;
    eh->e_version = EV_CURRENT;
    eh->e_entry = 0x1000; /* Virtual entry address matching ph->p_vaddr */
    eh->e_phoff = sizeof(Elf64_Ehdr);
    eh->e_ehsize = sizeof(Elf64_Ehdr);
    eh->e_phentsize = sizeof(Elf64_Phdr);
    eh->e_phnum = 1;

    Elf64_Phdr *ph = (Elf64_Phdr *)(dummy_elf + sizeof(Elf64_Ehdr));
    ph->p_type = PT_LOAD;
    ph->p_flags = PF_R | PF_X;
    ph->p_offset = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    ph->p_vaddr = 0x1000;
    ph->p_paddr = 0x1000;
    ph->p_filesz = 64;
    ph->p_memsz = 128;
    ph->p_align = 4096;

    /* Real AArch64 executable instructions:
     *   mov x0, #42   (0xd2800540)
     *   ret           (0xd65f03c0)
     */
    uint32_t *elf_code = (uint32_t *)(dummy_elf + sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr));
    elf_code[0] = 0xd2800540; /* mov x0, #42 */
    elf_code[1] = 0xd65f03c0; /* ret */

    int elf_fd = sys_open("/cherry_proc_test/sample_app.elf", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (elf_fd >= 0) {
        sys_write(elf_fd, dummy_elf, sizeof(dummy_elf));
        sys_close(elf_fd);

        char *test_argv[] = { (char *)"sample_app", (char *)"--version", NULL };
        char *test_envp[] = { (char *)"PATH=/bin", NULL };

        /* (a) Test Synchronous ELF Execution via proc_exec_elf */
        int exec_ret = proc_exec_elf("/cherry_proc_test/sample_app.elf", 2, test_argv, test_envp);

        /* (b) Test Asynchronous Process Spawn via proc_spawn_elf */
        pid_t elf_pid = proc_spawn_elf("/cherry_proc_test/sample_app.elf", 2, test_argv, test_envp, -1, -1, -1);
        int elf_status = 0;
        if (elf_pid > 0) {
            proc_waitpid(elf_pid, &elf_status, 0);
        }

        /* (c) Test glibc bootstrap syscalls */
        long tid = cherry_syscall(__NR_set_tid_address, 0, 0, 0, 0, 0, 0);
        char rand_buf[16];
        long rnd = cherry_syscall(__NR_getrandom, (long)rand_buf, sizeof(rand_buf), 0, 0, 0, 0);

        struct iovec iov;
        const char *hello_v = "hello writev";
        iov.iov_base = (void *)hello_v;
        iov.iov_len = strlen(hello_v);
        int tmp_wfd = sys_open("/cherry_proc_test/writev.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        long wv = (tmp_wfd >= 0) ? cherry_syscall(__NR_writev, tmp_wfd, (long)&iov, 1, 0, 0, 0) : -1;
        if (tmp_wfd >= 0) sys_close(tmp_wfd);

        if (exec_ret == 42 && elf_status == 42 && tid > 0 && rnd == 16 && wv == (long)strlen(hello_v)) {
            pass_elf = true;
        }
    }

    bool all_pass = pass_spawn && pass_wait && pass_fd && pass_pipe &&
                    pass_syscall && pass_applets && pass_sh && pass_smp_ps && pass_elf;

    pr_info("cherry_process: [process-verify] spawn=%s wait=%s fd=%s pipe=%s syscall=%s applets=%s sh=%s smp_ps=%s elf_glibc=%s all=%s\n",
            pass_spawn ? "pass" : "fail",
            pass_wait ? "pass" : "fail",
            pass_fd ? "pass" : "fail",
            pass_pipe ? "pass" : "fail",
            pass_syscall ? "pass" : "fail",
            pass_applets ? "pass" : "fail",
            pass_sh ? "pass" : "fail",
            pass_smp_ps ? "pass" : "fail",
            pass_elf ? "pass" : "fail",
            all_pass ? "pass" : "fail");

    return all_pass ? 0 : -1;
}

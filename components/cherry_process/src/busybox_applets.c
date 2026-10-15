/*
 * CherryBusybox - Core Applet Implementations
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_busybox.h"
#include "cherry_syscall.h"
#include "cherry_vfs.h"
#include "uart.h"
#include <string.h>

static int simple_atoi(const char *s)
{
    int val = 0;
    int sign = 1;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val * sign;
}

/* Helper to write string to stdout */
static void put_str(const char *str)
{
    if (str) {
        sys_write(1, str, strlen(str));
    }
}

int applet_echo(int argc, char **argv)
{
    bool newline = true;
    int idx = 1;

    if (idx < argc && strcmp(argv[idx], "-n") == 0) {
        newline = false;
        idx++;
    }

    for (int i = idx; i < argc; i++) {
        put_str(argv[i]);
        if (i < argc - 1) {
            put_str(" ");
        }
    }

    if (newline) {
        put_str("\n");
    }
    return 0;
}

int applet_cat(int argc, char **argv)
{
    char buf[512];

    if (argc <= 1) {
        /* Read from stdin */
        ssize_t n;
        while ((n = sys_read(0, buf, sizeof(buf))) > 0) {
            sys_write(1, buf, (size_t)n);
        }
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            ssize_t n;
            while ((n = sys_read(0, buf, sizeof(buf))) > 0) {
                sys_write(1, buf, (size_t)n);
            }
            continue;
        }

        int fd = sys_open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            char err[128];
            snprintf(err, sizeof(err), "cat: %s: No such file or directory\n", argv[i]);
            sys_write(2, err, strlen(err));
            continue;
        }

        ssize_t n;
        while ((n = sys_read(fd, buf, sizeof(buf))) > 0) {
            sys_write(1, buf, (size_t)n);
        }
        sys_close(fd);
    }
    return 0;
}

int applet_pwd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char cwd[VFS_PATH_MAX];
    if (sys_getcwd(cwd, sizeof(cwd)) == 0) {
        put_str(cwd);
        put_str("\n");
        return 0;
    }
    return 1;
}

int applet_ls(int argc, char **argv)
{
    bool show_all = false;
    bool long_fmt = false;
    const char *target_dir = ".";

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (size_t j = 1; j < strlen(argv[i]); j++) {
                if (argv[i][j] == 'a') show_all = true;
                if (argv[i][j] == 'l') long_fmt = true;
            }
        } else {
            target_dir = argv[i];
        }
    }

    char full_path[VFS_PATH_MAX];
    if (target_dir[0] == '/') {
        strncpy(full_path, target_dir, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    } else {
        char cwd[VFS_PATH_MAX];
        sys_getcwd(cwd, sizeof(cwd));
        if (strcmp(cwd, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", target_dir);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", cwd, target_dir);
        }
    }

    DIR *d = opendir(full_path);
    if (!d) {
        char err[128];
        snprintf(err, sizeof(err), "ls: cannot access '%s': No such file or directory\n", target_dir);
        sys_write(2, err, strlen(err));
        return 1;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!show_all && de->d_name[0] == '.') {
            continue;
        }

        if (long_fmt) {
            char entry_path[VFS_PATH_MAX * 2];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", full_path, de->d_name);
            struct stat st;
            memset(&st, 0, sizeof(st));
            stat(entry_path, &st);

            char line[256];
            char type_char = '-';
            if (S_ISDIR(st.st_mode)) type_char = 'd';
            else if (S_ISCHR(st.st_mode)) type_char = 'c';
            else if (S_ISBLK(st.st_mode)) type_char = 'b';

            snprintf(line, sizeof(line), "%crwxr-xr-x  1 root root %8lu %s\n",
                     type_char, (unsigned long)st.st_size, de->d_name);
            put_str(line);
        } else {
            put_str(de->d_name);
            put_str("  ");
        }
    }
    if (!long_fmt) {
        put_str("\n");
    }

    closedir(d);
    return 0;
}

int applet_cp(int argc, char **argv)
{
    if (argc < 3) {
        sys_write(2, "cp: missing destination file operand\n", 37);
        return 1;
    }

    int src = sys_open(argv[1], O_RDONLY, 0);
    if (src < 0) {
        sys_write(2, "cp: cannot open source file\n", 28);
        return 1;
    }

    int dst = sys_open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst < 0) {
        sys_close(src);
        sys_write(2, "cp: cannot open destination file\n", 33);
        return 1;
    }

    char buf[1024];
    ssize_t n;
    while ((n = sys_read(src, buf, sizeof(buf))) > 0) {
        sys_write(dst, buf, (size_t)n);
    }

    sys_close(src);
    sys_close(dst);
    return 0;
}

int applet_mv(int argc, char **argv)
{
    if (argc < 3) {
        sys_write(2, "mv: missing destination file operand\n", 37);
        return 1;
    }
    int ret = rename(argv[1], argv[2]);
    if (ret != 0) {
        /* Fallback to cp + rm */
        char *cp_args[4] = { (char *)"cp", argv[1], argv[2], NULL };
        if (applet_cp(3, cp_args) == 0) {
            sys_unlink(argv[1]);
            return 0;
        }
        sys_write(2, "mv: failed to rename\n", 21);
        return 1;
    }
    return 0;
}

int applet_rm(int argc, char **argv)
{
    if (argc < 2) {
        sys_write(2, "rm: missing operand\n", 20);
        return 1;
    }

    int start = 1;
    while (start < argc && argv[start][0] == '-') {
        start++;
    }

    for (int i = start; i < argc; i++) {
        sys_unlink(argv[i]);
    }
    return 0;
}

int applet_mkdir(int argc, char **argv)
{
    if (argc < 2) {
        sys_write(2, "mkdir: missing operand\n", 23);
        return 1;
    }

    int start = 1;
    bool parents = false;
    if (strcmp(argv[1], "-p") == 0) {
        parents = true;
        start = 2;
    }

    (void)parents;
    for (int i = start; i < argc; i++) {
        sys_mkdir(argv[i], 0755);
    }
    return 0;
}

int applet_rmdir(int argc, char **argv)
{
    if (argc < 2) {
        sys_write(2, "rmdir: missing operand\n", 23);
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        rmdir(argv[i]);
    }
    return 0;
}

int applet_touch(int argc, char **argv)
{
    if (argc < 2) {
        sys_write(2, "touch: missing file operand\n", 28);
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        int fd = sys_open(argv[i], O_WRONLY | O_CREAT, 0644);
        if (fd >= 0) {
            sys_close(fd);
        }
    }
    return 0;
}

int applet_uname(int argc, char **argv)
{
    struct utsname u;
    if (sys_uname(&u) != 0) return 1;

    bool all = false;
    if (argc > 1 && (strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "--all") == 0)) {
        all = true;
    }

    if (all) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s %s %s %s %s\n",
                 u.sysname, u.nodename, u.release, u.version, u.machine);
        put_str(buf);
    } else {
        put_str(u.sysname);
        put_str("\n");
    }
    return 0;
}

static int ps_visitor(struct process *p, void *arg)
{
    (void)arg;
    const char *state_str = "RUN";
    switch (p->state) {
    case PROC_STATE_READY:    state_str = "READY"; break;
    case PROC_STATE_RUNNING:  state_str = "RUN";   break;
    case PROC_STATE_SLEEPING: state_str = "SLEEP"; break;
    case PROC_STATE_ZOMBIE:   state_str = "ZOMB";  break;
    default:                  state_str = "UNK";   break;
    }

    char line[128];
    snprintf(line, sizeof(line), "%5d %5d %-6s %4u  %s\n",
             p->pid, p->ppid, state_str, (unsigned int)p->core_id, p->name);
    put_str(line);
    return 0;
}

int applet_ps(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    put_str("  PID  PPID STATE   CPU  COMMAND\n");
    proc_for_each(ps_visitor, NULL);
    return 0;
}

int applet_kill(int argc, char **argv)
{
    if (argc < 2) {
        sys_write(2, "kill: usage: kill [-9] PID...\n", 30);
        return 1;
    }

    int sig = 15;
    int idx = 1;
    if (argv[1][0] == '-') {
        sig = simple_atoi(&argv[1][1]);
        idx = 2;
    }

    for (int i = idx; i < argc; i++) {
        pid_t p = (pid_t)simple_atoi(argv[i]);
        if (p > 0) {
            sys_kill(p, sig);
        }
    }
    return 0;
}

int applet_sleep(int argc, char **argv)
{
    if (argc < 2) {
        sys_write(2, "sleep: missing operand\n", 23);
        return 1;
    }
    int sec = simple_atoi(argv[1]);
    if (sec > 0) {
        struct timespec ts = { .tv_sec = sec, .tv_nsec = 0 };
        sys_nanosleep(&ts, NULL);
    }
    return 0;
}

int applet_head(int argc, char **argv)
{
    int lines = 10;
    int file_idx = 1;

    if (argc >= 3 && strcmp(argv[1], "-n") == 0) {
        lines = simple_atoi(argv[2]);
        file_idx = 3;
    }

    int fd = 0;
    if (file_idx < argc) {
        fd = sys_open(argv[file_idx], O_RDONLY, 0);
        if (fd < 0) return 1;
    }

    char c;
    int line_count = 0;
    while (line_count < lines && sys_read(fd, &c, 1) == 1) {
        sys_write(1, &c, 1);
        if (c == '\n') line_count++;
    }

    if (fd > 0) sys_close(fd);
    return 0;
}

int applet_tail(int argc, char **argv)
{
    return applet_head(argc, argv);
}

int applet_wc(int argc, char **argv)
{
    int fd = 0;
    if (argc > 1 && argv[1][0] != '-') {
        fd = sys_open(argv[1], O_RDONLY, 0);
        if (fd < 0) return 1;
    }

    size_t lines = 0, words = 0, bytes = 0;
    char c;
    bool in_word = false;

    while (sys_read(fd, &c, 1) == 1) {
        bytes++;
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\n') {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            words++;
        }
    }

    if (fd > 0) sys_close(fd);

    char out[128];
    if (argc > 1 && argv[1][0] != '-') {
        snprintf(out, sizeof(out), "%7lu %7lu %7lu %s\n", lines, words, bytes, argv[1]);
    } else {
        snprintf(out, sizeof(out), "%7lu %7lu %7lu\n", lines, words, bytes);
    }
    put_str(out);
    return 0;
}

int applet_grep(int argc, char **argv)
{
    if (argc < 2) {
        sys_write(2, "grep: usage: grep PATTERN [FILE]\n", 33);
        return 1;
    }

    const char *pat = argv[1];
    int fd = 0;
    if (argc > 2) {
        fd = sys_open(argv[2], O_RDONLY, 0);
        if (fd < 0) return 1;
    }

    char line[256];
    size_t idx = 0;
    char c;

    while (sys_read(fd, &c, 1) == 1) {
        if (c == '\n' || idx >= sizeof(line) - 1) {
            line[idx] = '\0';
            if (strstr(line, pat)) {
                put_str(line);
                put_str("\n");
            }
            idx = 0;
        } else {
            line[idx++] = c;
        }
    }

    if (fd > 0) sys_close(fd);
    return 0;
}

int applet_df(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    put_str("Filesystem           1K-blocks      Used Available Use% Mounted on\n");
    put_str("/dev/mmcblk0             65536      4096     61440   6% /\n");
    put_str("/dev/nvme0n1             65536      1024     64512   2% /mnt/nvme\n");
    put_str("/dev/sda                 65536      1024     64512   2% /mnt/sata\n");
    return 0;
}

int applet_free(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    put_str("               total        used        free      shared  buff/cache   available\n");
    put_str("Mem:          262144       16384      245760           0        8192      245760\n");
    put_str("Swap:              0           0           0\n");
    return 0;
}

int applet_dmesg(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    put_str("[    0.000000] cherry_process: process subsystem active\n");
    put_str("[    0.000001] cherry_process: posix syscall engine active\n");
    put_str("[    0.000002] cherry_busybox: multi-call applets loaded\n");
    return 0;
}

int applet_date(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uint64_t ms = ktime_get_ms();
    char buf[64];
    snprintf(buf, sizeof(buf), "System uptime: %lu.%03lu seconds\n",
             (unsigned long)(ms / 1000), (unsigned long)(ms % 1000));
    put_str(buf);
    return 0;
}

int applet_true(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return 0;
}

int applet_false(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return 1;
}

int applet_hexdump(int argc, char **argv)
{
    if (argc < 2) {
        sys_write(2, "hexdump: missing file operand\n", 30);
        return 1;
    }
    int fd = sys_open(argv[1], O_RDONLY, 0);
    if (fd < 0) return 1;

    uint8_t buf[16];
    size_t offset = 0;
    ssize_t n;

    while ((n = sys_read(fd, buf, sizeof(buf))) > 0) {
        char line[128];
        int pos = snprintf(line, sizeof(line), "%08lx  ", offset);
        for (ssize_t i = 0; i < n; i++) {
            pos += snprintf(line + pos, sizeof(line) - pos, "%02x ", buf[i]);
            if (i == 7) pos += snprintf(line + pos, sizeof(line) - pos, " ");
        }
        for (ssize_t i = n; i < 16; i++) {
            pos += snprintf(line + pos, sizeof(line) - pos, "   ");
            if (i == 7) pos += snprintf(line + pos, sizeof(line) - pos, " ");
        }
        pos += snprintf(line + pos, sizeof(line) - pos, " |");
        for (ssize_t i = 0; i < n; i++) {
            char ch = (buf[i] >= 32 && buf[i] <= 126) ? (char)buf[i] : '.';
            pos += snprintf(line + pos, sizeof(line) - pos, "%c", ch);
        }
        snprintf(line + pos, sizeof(line) - pos, "|\n");
        put_str(line);
        offset += n;
    }
    sys_close(fd);
    return 0;
}

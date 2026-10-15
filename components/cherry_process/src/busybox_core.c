/*
 * CherryBusybox - Core Dispatcher and Multi-call Binary Architecture
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_busybox.h"
#include "cherry_syscall.h"
#include "cherry_printk.h"
#include "uart.h"
#include <string.h>

static const struct busybox_applet g_applets[] = {
    { "busybox", busybox_main,      "busybox [function [arguments]...]" },
    { "sh",      applet_sh,         "sh [-c 'command']" },
    { "echo",    applet_echo,       "echo [-n] [-e] [string ...]" },
    { "cat",     applet_cat,        "cat [FILE]..." },
    { "ls",      applet_ls,         "ls [-a] [-l] [FILE]..." },
    { "cp",      applet_cp,         "cp SOURCE DEST" },
    { "mv",      applet_mv,         "mv SOURCE DEST" },
    { "rm",      applet_rm,         "rm [-r] [-f] FILE..." },
    { "mkdir",   applet_mkdir,      "mkdir [-p] DIRECTORY..." },
    { "rmdir",   applet_rmdir,      "rmdir DIRECTORY..." },
    { "pwd",     applet_pwd,        "pwd" },
    { "touch",   applet_touch,      "touch FILE..." },
    { "uname",   applet_uname,      "uname [-a] [-s] [-n] [-r] [-m]" },
    { "ps",      applet_ps,         "ps" },
    { "kill",    applet_kill,       "kill [-9] PID..." },
    { "sleep",   applet_sleep,      "sleep SECONDS" },
    { "head",    applet_head,       "head [-n N] [FILE]..." },
    { "tail",    applet_tail,       "tail [-n N] [FILE]..." },
    { "wc",      applet_wc,         "wc [-l] [-w] [-c] [FILE]..." },
    { "grep",    applet_grep,       "grep PATTERN [FILE]..." },
    { "df",      applet_df,         "df" },
    { "free",    applet_free,       "free" },
    { "dmesg",   applet_dmesg,      "dmesg" },
    { "date",    applet_date,       "date" },
    { "true",    applet_true,       "true" },
    { "false",   applet_false,      "false" },
    { "hexdump", applet_hexdump,    "hexdump [-C] FILE" },
    { NULL,      NULL,              NULL }
};

const struct busybox_applet *busybox_find_applet(const char *name)
{
    if (!name) return NULL;

    /* Strip leading path (e.g. /bin/ls -> ls) */
    const char *p = strrchr(name, '/');
    if (p) name = p + 1;

    for (int i = 0; g_applets[i].name != NULL; i++) {
        if (strcmp(g_applets[i].name, name) == 0) {
            return &g_applets[i];
        }
    }
    return NULL;
}

int busybox_main(int argc, char **argv)
{
    if (argc <= 1 || (argc == 2 && strcmp(argv[1], "--help") == 0)) {
        sys_write(1, "BusyBox v1.36.0 (CherryOS AArch64 SMP) multi-call binary.\n\n", 58);
        sys_write(1, "Currently defined functions:\n", 29);
        sys_write(1, "\tcat, cp, date, df, dmesg, echo, false, free, grep, head,\n", 59);
        sys_write(1, "\thexdump, kill, ls, mkdir, mv, ps, pwd, rm, rmdir, sh,\n", 56);
        sys_write(1, "\tsleep, tail, touch, true, uname, wc\n\n", 37);
        return 0;
    }

    const struct busybox_applet *applet = busybox_find_applet(argv[1]);
    if (!applet) {
        char err[64];
        snprintf(err, sizeof(err), "busybox: %s: applet not found\n", argv[1]);
        sys_write(2, err, strlen(err));
        return 127;
    }

    return applet->fn(argc - 1, &argv[1]);
}

static int parse_cmdline(const char *cmdline, char *buf, size_t buf_len, int *argc, char *argv[PROC_MAX_ARGS])
{
    if (!cmdline || !buf || !argc || !argv) return -EINVAL;

    strncpy(buf, cmdline, buf_len - 1);
    buf[buf_len - 1] = '\0';

    int count = 0;
    char *p = buf;

    while (*p && count < (PROC_MAX_ARGS - 1)) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            argv[count++] = p;
            while (*p && *p != quote) p++;
            if (*p) *p++ = '\0';
        } else {
            argv[count++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    argv[count] = NULL;
    *argc = count;
    return count > 0 ? 0 : -EINVAL;
}

pid_t busybox_spawn(const char *cmdline, int in_fd, int out_fd, int err_fd)
{
    if (!cmdline) return -EINVAL;

    char arg_buf[256];
    char *argv[PROC_MAX_ARGS];
    int argc = 0;

    int ret = parse_cmdline(cmdline, arg_buf, sizeof(arg_buf), &argc, argv);
    if (ret != 0 || argc == 0) return -EINVAL;

    const struct busybox_applet *applet = busybox_find_applet(argv[0]);
    if (!applet) {
        return -ENOENT;
    }

    return proc_spawn(argv[0], applet->fn, argc, argv, NULL, in_fd, out_fd, err_fd);
}

int busybox_run_cmd(const char *cmdline)
{
    if (!cmdline) return -EINVAL;

    /* Check for shell special characters (pipes or redirection) */
    if (strchr(cmdline, '|') || strchr(cmdline, '>') || strchr(cmdline, '<')) {
        char *sh_argv[4];
        sh_argv[0] = (char *)"sh";
        sh_argv[1] = (char *)"-c";
        sh_argv[2] = (char *)cmdline;
        sh_argv[3] = NULL;

        pid_t pid = proc_spawn("sh", applet_sh, 3, sh_argv, NULL, -1, -1, -1);
        if (pid < 0) return pid;

        int status = 0;
        proc_waitpid(pid, &status, 0);
        return status;
    }

    pid_t pid = busybox_spawn(cmdline, -1, -1, -1);
    if (pid < 0) return pid;

    int status = 0;
    proc_waitpid(pid, &status, 0);
    return status;
}

/*
 * CherryBusybox - Lightweight POSIX Shell (sh / ash) Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_busybox.h"
#include "cherry_syscall.h"
#include "uart.h"
#include <string.h>

#define SH_MAX_LINE     256
#define SH_MAX_PIPES    8

/* Trim leading/trailing whitespace */
static char *trim_whitespace(char *str)
{
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') str++;
    if (*str == '\0') return str;

    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
    return str;
}

/* Parse redirections (> and <) out of a single command */
static int handle_redirections(char *cmd, int *in_vfs_fd, int *out_vfs_fd)
{
    *in_vfs_fd = -1;
    *out_vfs_fd = -1;

    /* Check for >> */
    char *append_pos = strstr(cmd, ">>");
    if (append_pos) {
        *append_pos = '\0';
        char *target = trim_whitespace(append_pos + 2);
        int fd = open(target, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) return fd;
        *out_vfs_fd = fd;
    } else {
        /* Check for > */
        char *out_pos = strchr(cmd, '>');
        if (out_pos) {
            *out_pos = '\0';
            char *target = trim_whitespace(out_pos + 1);
            int fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) return fd;
            *out_vfs_fd = fd;
        }
    }

    /* Check for < */
    char *in_pos = strchr(cmd, '<');
    if (in_pos) {
        *in_pos = '\0';
        char *target = trim_whitespace(in_pos + 1);
        int fd = open(target, O_RDONLY, 0);
        if (fd < 0) {
            if (*out_vfs_fd >= 0) close(*out_vfs_fd);
            return fd;
        }
        *in_vfs_fd = fd;
    }

    return 0;
}

/* Execute a single pipeline (1 or more commands connected by |) */
static int execute_pipeline(char *line)
{
    char *stages[SH_MAX_PIPES];
    int stage_count = 0;

    char *token = line;
    while (token && stage_count < SH_MAX_PIPES) {
        char *pipe_pos = strchr(token, '|');
        if (pipe_pos) {
            *pipe_pos = '\0';
            stages[stage_count++] = trim_whitespace(token);
            token = pipe_pos + 1;
        } else {
            stages[stage_count++] = trim_whitespace(token);
            break;
        }
    }

    if (stage_count == 0 || strlen(stages[0]) == 0) {
        return 0;
    }

    /* Single command optimization and built-in check */
    if (stage_count == 1) {
        char copy[SH_MAX_LINE];
        strncpy(copy, stages[0], sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        char *first_word = copy;
        while (*first_word == ' ' || *first_word == '\t') first_word++;
        char *space = strchr(first_word, ' ');
        if (space) *space = '\0';

        if (strcmp(first_word, "cd") == 0) {
            char *target = copy + strlen("cd");
            while (*target == ' ' || *target == '\t') target++;
            if (*target == '\0') target = (char *)"/";
            return sys_chdir(target);
        } else if (strcmp(first_word, "exit") == 0) {
            sys_exit(0);
            return 0;
        }

        int in_fd = -1, out_fd = -1;
        int ret = handle_redirections(stages[0], &in_fd, &out_fd);
        if (ret != 0) return ret;

        pid_t pid = busybox_spawn(trim_whitespace(stages[0]), in_fd, out_fd, -1);
        if (pid < 0) {
            char err[128];
            snprintf(err, sizeof(err), "sh: %s: command not found\n", first_word);
            sys_write(2, err, strlen(err));
            return 127;
        }

        int status = 0;
        proc_waitpid(pid, &status, 0);
        return status;
    }

    /* Multi-stage pipeline */
    int pipe_fds[SH_MAX_PIPES - 1][2];
    for (int i = 0; i < stage_count - 1; i++) {
        if (cherry_pipe_create(&pipe_fds[i][0], &pipe_fds[i][1]) != 0) {
            return -EIO;
        }
    }

    pid_t pids[SH_MAX_PIPES];
    for (int i = 0; i < stage_count; i++) {
        int in_fd = (i == 0) ? -1 : pipe_fds[i - 1][0];
        int out_fd = (i == stage_count - 1) ? -1 : pipe_fds[i][1];

        pids[i] = busybox_spawn(stages[i], in_fd, out_fd, -1);
    }

    /* Close all pipe descriptors in parent */
    for (int i = 0; i < stage_count - 1; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    /* Wait for all pipeline children */
    int last_status = 0;
    for (int i = 0; i < stage_count; i++) {
        if (pids[i] > 0) {
            int st = 0;
            proc_waitpid(pids[i], &st, 0);
            if (i == stage_count - 1) {
                last_status = st;
            }
        }
    }
    return last_status;
}

int applet_sh(int argc, char **argv)
{
    /* If called with -c "command" */
    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        char cmd[SH_MAX_LINE];
        strncpy(cmd, argv[2], sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
        return execute_pipeline(cmd);
    }

    /* Interactive shell loop */
    sys_write(1, "\nCherryOS UNIX Shell (ash/sh) - FreeRTOS SMP\n", 45);

    char line[SH_MAX_LINE];
    for (;;) {
        char cwd[VFS_PATH_MAX];
        sys_getcwd(cwd, sizeof(cwd));
        char prompt[VFS_PATH_MAX + 16];
        snprintf(prompt, sizeof(prompt), "cherry:%s$ ", cwd);
        sys_write(1, prompt, strlen(prompt));

        size_t idx = 0;
        char c;
        while (sys_read(0, &c, 1) == 1) {
            if (c == '\r' || c == '\n') {
                sys_write(1, "\n", 1);
                break;
            }
            if (c == 0x08 || c == 0x7F) {
                if (idx > 0) {
                    idx--;
                    sys_write(1, "\b \b", 3);
                }
                continue;
            }
            if (idx < sizeof(line) - 1) {
                line[idx++] = c;
                sys_write(1, &c, 1);
            }
        }

        line[idx] = '\0';
        char *trimmed = trim_whitespace(line);
        if (strlen(trimmed) == 0) continue;

        if (strcmp(trimmed, "exit") == 0) {
            break;
        }

        execute_pipeline(trimmed);
    }
    return 0;
}

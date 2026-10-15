/*
 * CherryBusybox - Lightweight Multi-call UNIX Utilities & Shell Engine
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Busybox applet architecture packaged into lightweight processes.
 */

#ifndef _CHERRY_BUSYBOX_H_
#define _CHERRY_BUSYBOX_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cherry_process.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*busybox_applet_fn)(int argc, char **argv);

struct busybox_applet {
    const char          *name;
    busybox_applet_fn    fn;
    const char          *usage;
};

/**
 * Busybox master dispatcher entry.
 * Usage: busybox <applet> [args...]
 */
int busybox_main(int argc, char **argv);

/**
 * Find applet by name.
 */
const struct busybox_applet *busybox_find_applet(const char *name);

/**
 * Spawn a busybox applet in a new child process with custom standard I/O descriptors.
 *
 * @param cmdline Command line string (e.g. "ls -la /")
 * @param in_fd   VFS fd for stdin (-1 for default/inherit)
 * @param out_fd  VFS fd for stdout (-1 for default/inherit)
 * @param err_fd  VFS fd for stderr (-1 for default/inherit)
 * @return PID of the spawned process, or negative error code
 */
pid_t busybox_spawn(const char *cmdline, int in_fd, int out_fd, int err_fd);

/**
 * Synchronously run a command line string inside a process and wait for completion.
 *
 * @param cmdline Command line string
 * @return Exit code of the command process, or negative error code
 */
int busybox_run_cmd(const char *cmdline);

/* Core Applet Implementations */
int applet_echo(int argc, char **argv);
int applet_cat(int argc, char **argv);
int applet_ls(int argc, char **argv);
int applet_cp(int argc, char **argv);
int applet_mv(int argc, char **argv);
int applet_rm(int argc, char **argv);
int applet_mkdir(int argc, char **argv);
int applet_rmdir(int argc, char **argv);
int applet_pwd(int argc, char **argv);
int applet_touch(int argc, char **argv);
int applet_uname(int argc, char **argv);
int applet_ps(int argc, char **argv);
int applet_kill(int argc, char **argv);
int applet_sleep(int argc, char **argv);
int applet_head(int argc, char **argv);
int applet_tail(int argc, char **argv);
int applet_wc(int argc, char **argv);
int applet_grep(int argc, char **argv);
int applet_df(int argc, char **argv);
int applet_free(int argc, char **argv);
int applet_dmesg(int argc, char **argv);
int applet_date(int argc, char **argv);
int applet_true(int argc, char **argv);
int applet_false(int argc, char **argv);
int applet_hexdump(int argc, char **argv);
int applet_sh(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_BUSYBOX_H_ */

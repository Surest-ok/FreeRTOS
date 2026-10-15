/*
 * CherryFSTools - Filesystem and Storage Management Utility Suite
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef CHERRY_FSTOOLS_H
#define CHERRY_FSTOOLS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize CherryFSTools subsystem and register all shell commands and C symbols */
int cherry_fstools_init(void);

/* Acceptance test suite */
void cherry_fstools_run_tests(void);

/* C Functions (VxWorks-style callable) - prefixed with cherry_ */
int cherry_fdisk(const char *dev);
int cherry_part_create(const char *dev, int part_no, uint32_t start_lba, uint32_t sectors, uint8_t type);
int cherry_mount(const char *src, const char *target, const char *fstype);
int cherry_umount(const char *target);
int cherry_ls(const char *path);
int cherry_cat(const char *path);
int cherry_cp(const char *src, const char *dst);
int cherry_mv(const char *src, const char *dst);
int cherry_rm(const char *path);
int cherry_mkdir(const char *path);
int cherry_echo(const char *str, const char *redir_file, bool append);

/* Backward compatibility aliases */
#define fstools_fdisk        cherry_fdisk
#define fstools_part_create  cherry_part_create
#define fstools_mount        cherry_mount
#define fstools_umount       cherry_umount
#define fstools_ls           cherry_ls
#define fstools_cat          cherry_cat
#define fstools_cp           cherry_cp
#define fstools_mv           cherry_mv
#define fstools_rm           cherry_rm
#define fstools_mkdir        cherry_mkdir
#define fstools_echo         cherry_echo

/* Shell Command Callbacks */
int cmd_fs(int argc, char **argv);
int cmd_fstool(int argc, char **argv);
int cmd_fdisk(int argc, char **argv);
int cmd_mount(int argc, char **argv);
int cmd_umount(int argc, char **argv);
int cmd_ls(int argc, char **argv);
int cmd_cat(int argc, char **argv);
int cmd_cp(int argc, char **argv);
int cmd_mv(int argc, char **argv);
int cmd_rm(int argc, char **argv);
int cmd_mkdir(int argc, char **argv);
int cmd_echo(int argc, char **argv);
int cmd_mkfs(int argc, char **argv);
int cmd_fsck(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* CHERRY_FSTOOLS_H */

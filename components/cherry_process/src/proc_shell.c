/*
 * CherryProcess - CherryShell Command Bindings (busybox, ps, kill, sh)
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_process.h"
#include "cherry_busybox.h"
#include "cherry_shell.h"
#include "cherry_printk.h"

int cmd_busybox(int argc, char **argv)
{
    return busybox_main(argc, argv);
}

int cmd_ps(int argc, char **argv)
{
    return applet_ps(argc, argv);
}

int cmd_kill(int argc, char **argv)
{
    return applet_kill(argc, argv);
}

int cmd_sh(int argc, char **argv)
{
    return applet_sh(argc, argv);
}

#include "cherry_elf.h"

int cmd_elf_run(int argc, char **argv)
{
    if (argc < 2) {
        printk("Usage: elf_run <path/to/elf> [args...]\n");
        return -1;
    }
    pid_t pid = proc_spawn_elf(argv[1], argc - 1, &argv[1], NULL, -1, -1, -1);
    if (pid <= 0) {
        printk("elf_run: failed to spawn ELF '%s' (err=%d)\n", argv[1], (int)pid);
        return -1;
    }
    int status = 0;
    proc_waitpid(pid, &status, 0);
    return status;
}

void proc_register_shell_commands(void)
{
    cherry_shell_register_symbol("busybox", (uintptr_t)cmd_busybox, CHERRY_SHELL_SYM_CMD, "busybox multi-call utility suite");
    cherry_shell_register_symbol("ps",      (uintptr_t)cmd_ps,      CHERRY_SHELL_SYM_CMD, "report process snapshot");
    cherry_shell_register_symbol("kill",    (uintptr_t)cmd_kill,    CHERRY_SHELL_SYM_CMD, "send signal to process: kill [-9] PID");
    cherry_shell_register_symbol("sh",      (uintptr_t)cmd_sh,      CHERRY_SHELL_SYM_CMD, "interactive or scripted UNIX shell");
    cherry_shell_register_symbol("elf_run", (uintptr_t)cmd_elf_run, CHERRY_SHELL_SYM_CMD, "execute Linux AArch64 ELF binary: elf_run <path> [args...]");
    cherry_shell_register_symbol("proc_spawn_elf", (uintptr_t)proc_spawn_elf, CHERRY_SHELL_SYM_FUNC, "proc_spawn_elf(path, argc, argv, envp, in, out, err)");
}

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

void proc_register_shell_commands(void)
{
    cherry_shell_register_symbol("busybox", (uintptr_t)cmd_busybox, CHERRY_SHELL_SYM_CMD, "busybox multi-call utility suite");
    cherry_shell_register_symbol("ps",      (uintptr_t)cmd_ps,      CHERRY_SHELL_SYM_CMD, "report process snapshot");
    cherry_shell_register_symbol("kill",    (uintptr_t)cmd_kill,    CHERRY_SHELL_SYM_CMD, "send signal to process: kill [-9] PID");
    cherry_shell_register_symbol("sh",      (uintptr_t)cmd_sh,      CHERRY_SHELL_SYM_CMD, "interactive or scripted UNIX shell");
}

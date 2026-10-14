/*
 * CherryShell - Acceptance & Verification Test Suite
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "cherry_shell.h"
#include "cherry_kal.h"
#include "uart.h"
#include <string.h>

extern uintptr_t test_add(uintptr_t a, uintptr_t b);
extern uintptr_t test_sum8(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d,
                           uintptr_t e, uintptr_t f, uintptr_t g, uintptr_t h);
extern uintptr_t test_str_len(const char *str);

static uintptr_t s_custom_calc(uintptr_t x, uintptr_t y)
{
    return (x * 2) + y;
}

static uintptr_t s_test_variable = 0x55AA1234;

int cherry_shell_run_tests(void)
{
    kal_printf("<6>cherry_shell: starting verification test suite...\n");

    int pass_syms = 0;
    int pass_addr2sym = 0;
    int pass_c_func = 0;
    int pass_sum8 = 0;
    int pass_str = 0;
    int pass_cmds = 0;

    /* Initialize CherryShell */
    cherry_shell_init();

    /* ---------------------------------------------------------------------
     * 1. Symbol Registration & Lookup Test
     * --------------------------------------------------------------------- */
    int reg_ret = cherry_shell_register_symbol("s_custom_calc", (uintptr_t)s_custom_calc,
                                               CHERRY_SHELL_SYM_FUNC, "Custom calculation");
    cherry_shell_register_symbol("s_test_variable", (uintptr_t)&s_test_variable,
                                 CHERRY_SHELL_SYM_VAR, "Test variable");
    uintptr_t addr = cherry_shell_sym_to_addr("s_custom_calc");
    uintptr_t var_addr = cherry_shell_sym_to_addr("s_test_variable");
    uintptr_t add_addr = cherry_shell_sym_to_addr("test_add");

    if (reg_ret == 0 && addr == (uintptr_t)s_custom_calc &&
        var_addr == (uintptr_t)&s_test_variable && add_addr == (uintptr_t)test_add) {
        pass_syms = 1;
    } else {
        kal_printf("<3>cherry_shell: syms lookup failed (reg=%d addr=0x%lx vs 0x%lx, add_addr=0x%lx)\n",
                   reg_ret, (unsigned long)addr, (unsigned long)s_custom_calc, (unsigned long)add_addr);
    }

    /* ---------------------------------------------------------------------
     * 2. Address to Symbol (addr2sym) Resolution Test
     * --------------------------------------------------------------------- */
    char sym_buf[64];
    uintptr_t offset = 0;

    /* 2a. Exact match test */
    int a2s_ret1 = cherry_shell_addr_to_sym((uintptr_t)test_add, sym_buf, sizeof(sym_buf), &offset);
    bool exact_ok = (a2s_ret1 == 0 && offset == 0 && strcmp(sym_buf, "test_add") == 0);

    /* 2b. Offset match test (+0x18) */
    int a2s_ret2 = cherry_shell_addr_to_sym((uintptr_t)test_add + 0x18, sym_buf, sizeof(sym_buf), &offset);
    bool offset_ok = (a2s_ret2 == 0 && offset == 0x18 && strcmp(sym_buf, "test_add+0x18") == 0);

    if (exact_ok && offset_ok) {
        pass_addr2sym = 1;
    } else {
        kal_printf("<3>cherry_shell: addr2sym failed (exact=%d sym='%s' off=%lu, offset=%d sym='%s' off=%lu)\n",
                   exact_ok, sym_buf, (unsigned long)offset, offset_ok, sym_buf, (unsigned long)offset);
    }

    /* ---------------------------------------------------------------------
     * 3. VxWorks-style C-Function Invocation Test
     * --------------------------------------------------------------------- */
    char out_buf[128];
    memset(out_buf, 0, sizeof(out_buf));

    /* 3a. Call test_add(15, 27) -> expected 42 = 0x2a */
    int exec_ret1 = cherry_shell_exec("test_add(15, 27)", out_buf, sizeof(out_buf));
    if (exec_ret1 == 0 && strstr(out_buf, "42") != NULL) {
        pass_c_func = 1;
    } else {
        kal_printf("<3>cherry_shell: test_add failed (ret=%d out='%s')\n", exec_ret1, out_buf);
    }

    /* 3b. Call test_sum8(1, 2, 3, 4, 5, 6, 7, 8) -> expected 36 = 0x24 */
    memset(out_buf, 0, sizeof(out_buf));
    int exec_ret2 = cherry_shell_exec("test_sum8(1, 2, 3, 4, 5, 6, 7, 8)", out_buf, sizeof(out_buf));
    if (exec_ret2 == 0 && strstr(out_buf, "36") != NULL) {
        pass_sum8 = 1;
    } else {
        kal_printf("<3>cherry_shell: test_sum8 failed (ret=%d out='%s')\n", exec_ret2, out_buf);
    }

    /* 3c. Call string length: test_str_len("CherryVxWorks") -> expected 13 */
    memset(out_buf, 0, sizeof(out_buf));
    int exec_ret3 = cherry_shell_exec("test_str_len(\"CherryVxWorks\")", out_buf, sizeof(out_buf));
    if (exec_ret3 == 0 && strstr(out_buf, "13") != NULL) {
        pass_str = 1;
    } else {
        kal_printf("<3>cherry_shell: test_str_len failed (ret=%d out='%s')\n", exec_ret3, out_buf);
    }

    /* ---------------------------------------------------------------------
     * 4. Shell Commands Execution Test (help, addr2sym, ls, mkdir, touch)
     * --------------------------------------------------------------------- */
    char cmd_a2s[64];
    snprintf(cmd_a2s, sizeof(cmd_a2s), "addr2sym 0x%lx", (unsigned long)test_add);

    int c1 = cherry_shell_exec("help", NULL, 0);
    int c2 = cherry_shell_exec(cmd_a2s, NULL, 0);
    int c3 = cherry_shell_exec("ls /", NULL, 0);
    int c4 = cherry_shell_exec("mkdir /cherry_test_dir", NULL, 0);
    int c5 = cherry_shell_exec("touch /cherry_test_dir/file.log", NULL, 0);
    int c6 = cherry_shell_exec("ls /cherry_test_dir", NULL, 0);

    if (c1 == 0 && c2 == 0 && c3 == 0 && c4 == 0 && c5 == 0 && c6 == 0) {
        pass_cmds = 1;
    } else {
        kal_printf("<3>cherry_shell: cmds failed (c1=%d c2=%d c3=%d c4=%d c5=%d c6=%d)\n",
                   c1, c2, c3, c4, c5, c6);
    }

    /* Summary report */
    bool all_pass = (pass_syms && pass_addr2sym && pass_c_func && pass_sum8 && pass_str && pass_cmds);
    kal_printf("<6>cherry_shell: [shell-verify] syms=%s addr2sym=%s c_func=%s sum8=%s str=%s cmds=%s all=%s\n",
               pass_syms ? "pass" : "fail",
               pass_addr2sym ? "pass" : "fail",
               pass_c_func ? "pass" : "fail",
               pass_sum8 ? "pass" : "fail",
               pass_str ? "pass" : "fail",
               pass_cmds ? "pass" : "fail",
               all_pass ? "pass" : "fail");

    return all_pass ? 0 : -1;
}

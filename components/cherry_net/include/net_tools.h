/*
 * CherryNet - Network Tools & Diagnostic Commands
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_TOOLS_H_
#define _CHERRY_NET_TOOLS_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ping_run(const char *target, int count, int timeout_ms);
int ifconfig_print(void);
int ifconfig_configure(const char *name, const char *ip_str, const char *mask_str);

int net_cmd_ifconfig(int argc, char **argv);
int net_cmd_ping(int argc, char **argv);
int net_cmd_iperf(int argc, char **argv);
int net_cmd_netstat(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_TOOLS_H_ */

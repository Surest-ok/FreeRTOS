/*
 * CherryNet - Network Interface Management
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_if.h"
#include "net_ip6.h"
#include "cherry_kal.h"
#include <string.h>

static struct netif *s_netif_list = NULL;
static struct netif *s_default_netif = NULL;
static spinlock_t    s_if_lock = SPIN_LOCK_UNLOCKED;

int netif_init(void)
{
    spin_lock_init(&s_if_lock);
    s_netif_list = NULL;
    s_default_netif = NULL;
    return 0;
}

int netif_register(struct netif *nif, const char *name, uint16_t mtu)
{
    if (!nif || !name) return -1;
    strncpy(nif->name, name, sizeof(nif->name) - 1);
    nif->mtu = mtu ? mtu : ETH_DATA_LEN;

    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);

    nif->next = s_netif_list;
    s_netif_list = nif;

    if (!s_default_netif && !(nif->flags & NETIF_FLAG_LOOPBACK)) {
        s_default_netif = nif;
    }

    spin_unlock_irqrestore(&s_if_lock, flags);
    return 0;
}

struct netif *netif_find(const char *name)
{
    if (!name) return NULL;
    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);

    struct netif *curr = s_netif_list;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            spin_unlock_irqrestore(&s_if_lock, flags);
            return curr;
        }
        curr = curr->next;
    }

    spin_unlock_irqrestore(&s_if_lock, flags);
    return NULL;
}

struct netif *netif_get_default(void)
{
    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);
    struct netif *nif = s_default_netif ? s_default_netif : s_netif_list;
    spin_unlock_irqrestore(&s_if_lock, flags);
    return nif;
}

void netif_set_default(struct netif *nif)
{
    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);
    s_default_netif = nif;
    spin_unlock_irqrestore(&s_if_lock, flags);
}

int netif_set_up(struct netif *nif)
{
    if (!nif) return -1;
    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);
    nif->flags |= (NETIF_FLAG_UP | NETIF_FLAG_LINK_UP);
    spin_unlock_irqrestore(&s_if_lock, flags);
    return 0;
}

int netif_set_down(struct netif *nif)
{
    if (!nif) return -1;
    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);
    nif->flags &= ~(NETIF_FLAG_UP | NETIF_FLAG_LINK_UP);
    spin_unlock_irqrestore(&s_if_lock, flags);
    return 0;
}

int netif_set_ip4(struct netif *nif, uint32_t ip, uint32_t netmask, uint32_t gw)
{
    if (!nif) return -1;
    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);
    nif->ip4_addr.addr = ip;
    nif->ip4_netmask.addr = netmask;
    nif->ip4_gw.addr = gw;
    spin_unlock_irqrestore(&s_if_lock, flags);
    return 0;
}

int netif_set_ip6(struct netif *nif, const struct ip6_addr *ip6, uint8_t prefix_len)
{
    if (!nif || !ip6) return -1;
    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);
    memcpy(&nif->ip6_global, ip6, sizeof(struct ip6_addr));
    nif->ip6_prefix_len = prefix_len;
    spin_unlock_irqrestore(&s_if_lock, flags);
    return 0;
}

struct netif *netif_route4(uint32_t dest_ip)
{
    /* Loopback 127.0.0.0/8 */
    if ((ntohl(dest_ip) & 0xFF000000U) == 0x7F000000U) {
        return netif_find("lo0");
    }

    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);

    struct netif *curr = s_netif_list;
    while (curr) {
        if ((curr->flags & NETIF_FLAG_UP) && curr->ip4_addr.addr) {
            uint32_t net = curr->ip4_addr.addr & curr->ip4_netmask.addr;
            uint32_t dst_net = dest_ip & curr->ip4_netmask.addr;
            if (net == dst_net) {
                spin_unlock_irqrestore(&s_if_lock, flags);
                return curr;
            }
        }
        curr = curr->next;
    }

    struct netif *def = s_default_netif;
    spin_unlock_irqrestore(&s_if_lock, flags);
    return def;
}

struct netif *netif_route6(const struct ip6_addr *dest_ip6)
{
    if (!dest_ip6) return NULL;

    /* IPv6 Loopback ::1 */
    static const uint8_t s_loopback6[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
    if (memcmp(dest_ip6->u8, s_loopback6, 16) == 0) {
        return netif_find("lo0");
    }

    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);

    struct netif *curr = s_netif_list;
    while (curr) {
        if ((curr->flags & NETIF_FLAG_UP)) {
            /* Check link-local */
            if (dest_ip6->u8[0] == 0xFE && (dest_ip6->u8[1] & 0xC0) == 0x80) {
                if (!(curr->flags & NETIF_FLAG_LOOPBACK)) {
                    spin_unlock_irqrestore(&s_if_lock, flags);
                    return curr;
                }
            }
            /* Check matching prefix */
            if (curr->ip6_prefix_len >= 64 &&
                memcmp(curr->ip6_global.u8, dest_ip6->u8, curr->ip6_prefix_len / 8) == 0) {
                spin_unlock_irqrestore(&s_if_lock, flags);
                return curr;
            }
        }
        curr = curr->next;
    }

    struct netif *def = s_default_netif;
    spin_unlock_irqrestore(&s_if_lock, flags);
    return def;
}

int netif_for_each(void *ctx, int (*cb)(struct netif *nif, void *ctx))
{
    if (!cb) return -1;
    unsigned long flags;
    spin_lock_irqsave(&s_if_lock, flags);

    struct netif *curr = s_netif_list;
    while (curr) {
        int ret = cb(curr, ctx);
        if (ret != 0) {
            spin_unlock_irqrestore(&s_if_lock, flags);
            return ret;
        }
        curr = curr->next;
    }

    spin_unlock_irqrestore(&s_if_lock, flags);
    return 0;
}

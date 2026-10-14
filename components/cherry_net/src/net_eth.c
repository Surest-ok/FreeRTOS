/*
 * CherryNet - Ethernet II Framing and ARP Cache
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_eth.h"
#include "net_ip4.h"
#include "net_ip6.h"
#include "cherry_kal.h"
#include <string.h>

#define ARP_TABLE_SIZE 32

struct arp_entry {
    uint32_t ip;
    uint8_t  mac[ETH_ALEN];
    bool     valid;
};

static struct arp_entry s_arp_table[ARP_TABLE_SIZE];
static spinlock_t       s_arp_lock = SPIN_LOCK_UNLOCKED;

int arp_init(void)
{
    spin_lock_init(&s_arp_lock);
    memset(s_arp_table, 0, sizeof(s_arp_table));
    return 0;
}

int arp_lookup(uint32_t ip, uint8_t *mac_out)
{
    if (!mac_out) return -1;
    unsigned long flags;
    spin_lock_irqsave(&s_arp_lock, flags);

    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (s_arp_table[i].valid && s_arp_table[i].ip == ip) {
            memcpy(mac_out, s_arp_table[i].mac, ETH_ALEN);
            spin_unlock_irqrestore(&s_arp_lock, flags);
            return 0;
        }
    }

    spin_unlock_irqrestore(&s_arp_lock, flags);
    return -1;
}

void arp_update(uint32_t ip, const uint8_t *mac)
{
    if (!mac || ip == 0) return;
    unsigned long flags;
    spin_lock_irqsave(&s_arp_lock, flags);

    /* Look for existing */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (s_arp_table[i].valid && s_arp_table[i].ip == ip) {
            memcpy(s_arp_table[i].mac, mac, ETH_ALEN);
            spin_unlock_irqrestore(&s_arp_lock, flags);
            return;
        }
    }

    /* Find empty or LRU slot */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!s_arp_table[i].valid) {
            s_arp_table[i].valid = true;
            s_arp_table[i].ip = ip;
            memcpy(s_arp_table[i].mac, mac, ETH_ALEN);
            spin_unlock_irqrestore(&s_arp_lock, flags);
            return;
        }
    }

    /* Fallback overwrite slot 0 */
    s_arp_table[0].valid = true;
    s_arp_table[0].ip = ip;
    memcpy(s_arp_table[0].mac, mac, ETH_ALEN);

    spin_unlock_irqrestore(&s_arp_lock, flags);
}

int arp_request(struct netif *nif, uint32_t target_ip)
{
    if (!nif) return -1;
    struct net_buf *buf = net_buf_alloc();
    if (!buf) return -1;

    struct arp_hdr *arp = (struct arp_hdr *)buf->data;
    buf->len = sizeof(struct arp_hdr);
    buf->tot_len = buf->len;

    arp->htype = htons(1);         /* Ethernet */
    arp->ptype = htons(ETH_P_IP);  /* IPv4 */
    arp->hlen = ETH_ALEN;
    arp->plen = 4;
    arp->opcode = htons(ARP_OP_REQUEST);
    memcpy(arp->sender_mac, nif->hwaddr, ETH_ALEN);
    arp->sender_ip = nif->ip4_addr.addr;
    memset(arp->target_mac, 0, ETH_ALEN);
    arp->target_ip = target_ip;

    static const uint8_t bcast_mac[ETH_ALEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    return eth_output(nif, buf, bcast_mac, ETH_P_ARP);
}

int arp_input(struct netif *nif, struct net_buf *buf)
{
    if (!nif || !buf || buf->len < sizeof(struct arp_hdr)) {
        net_buf_free(buf);
        return -1;
    }

    struct arp_hdr *arp = (struct arp_hdr *)buf->data;
    uint16_t op = ntohs(arp->opcode);

    /* Update cache from sender */
    arp_update(arp->sender_ip, arp->sender_mac);

    /* If it's a request for our IP, reply! */
    if (op == ARP_OP_REQUEST && arp->target_ip == nif->ip4_addr.addr) {
        struct net_buf *reply = net_buf_alloc();
        if (reply) {
            struct arp_hdr *r = (struct arp_hdr *)reply->data;
            reply->len = sizeof(struct arp_hdr);
            reply->tot_len = reply->len;

            r->htype = htons(1);
            r->ptype = htons(ETH_P_IP);
            r->hlen = ETH_ALEN;
            r->plen = 4;
            r->opcode = htons(ARP_OP_REPLY);
            memcpy(r->sender_mac, nif->hwaddr, ETH_ALEN);
            r->sender_ip = nif->ip4_addr.addr;
            memcpy(r->target_mac, arp->sender_mac, ETH_ALEN);
            r->target_ip = arp->sender_ip;

            eth_output(nif, reply, arp->sender_mac, ETH_P_ARP);
        }
    }

    net_buf_free(buf);
    return 0;
}

int eth_input(struct netif *nif, struct net_buf *buf)
{
    if (!nif || !buf) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    if (buf->len < sizeof(struct eth_hdr)) {
        nif->rx_errors++;
        net_buf_free(buf);
        return -1;
    }

    struct eth_hdr *eth = (struct eth_hdr *)buf->data;
    uint16_t type = ntohs(eth->type);

    net_buf_pull(buf, sizeof(struct eth_hdr));

    if (type == ETH_P_IP) {
        return ip4_input(nif, buf);
    } else if (type == ETH_P_ARP) {
        return arp_input(nif, buf);
    } else if (type == ETH_P_IPV6) {
        return ip6_input(nif, buf);
    }

    /* Unsupported protocol */
    net_buf_free(buf);
    return 0;
}

int eth_output(struct netif *nif, struct net_buf *buf, const uint8_t *dst_mac, uint16_t ethertype)
{
    if (!nif || !buf || !dst_mac) {
        if (buf) net_buf_free(buf);
        return -1;
    }

    struct eth_hdr *eth = (struct eth_hdr *)net_buf_push(buf, sizeof(struct eth_hdr));
    if (!eth) {
        nif->tx_dropped++;
        net_buf_free(buf);
        return -1;
    }

    memcpy(eth->dst, dst_mac, ETH_ALEN);
    memcpy(eth->src, nif->hwaddr, ETH_ALEN);
    eth->type = htons(ethertype);

    buf->netif = nif;
    if (nif->send) {
        return nif->send(nif, buf);
    }

    net_buf_free(buf);
    return 0;
}

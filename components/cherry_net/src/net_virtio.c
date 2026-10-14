/*
 * CherryNet - VirtIO-Net PCI Network Interface Driver
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "net_virtio.h"
#include "net_queue.h"
#include "net_ip6.h"
#include "cherry_kal.h"
#include <string.h>

#define VIRTIO_NET_QUEUE_SIZE 64

struct virtio_vring {
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_used  *used;
    uint16_t            size;
    uint16_t            last_used_idx;
    uint16_t            free_head;
    void               *raw_mem;
    struct net_buf     *bufs[VIRTIO_NET_QUEUE_SIZE];
};

struct virtio_net_dev {
    struct pci_dev     *pdev;
    uintptr_t           io_base;
    uint8_t             mac[ETH_ALEN];
    struct virtio_vring rx_vq;
    struct virtio_vring tx_vq;
    spinlock_t          tx_lock;
};

static struct virtio_net_dev s_virtio_dev;
static struct netif          s_virtio_netif;

/* I/O Helpers for PCI BAR0 (MMIO on ARM64) */
static inline uint8_t v_in8(uintptr_t base, int off) {
    return *(volatile uint8_t *)(base + off);
}
static inline uint16_t v_in16(uintptr_t base, int off) {
    return *(volatile uint16_t *)(base + off);
}
static inline uint32_t v_in32(uintptr_t base, int off) {
    return *(volatile uint32_t *)(base + off);
}
static inline void v_out8(uintptr_t base, int off, uint8_t val) {
    *(volatile uint8_t *)(base + off) = val;
}
static inline void v_out16(uintptr_t base, int off, uint16_t val) {
    *(volatile uint16_t *)(base + off) = val;
}
static inline void v_out32(uintptr_t base, int off, uint32_t val) {
    *(volatile uint32_t *)(base + off) = val;
}

static int vring_setup(struct virtio_vring *vq, uint16_t num)
{
    /* 4KB aligned allocation: Desc (num*16) + Avail (4+2*num) aligned to 4KB, Used (4+8*num) */
    size_t desc_avail_sz = (num * sizeof(struct vring_desc)) + (sizeof(uint16_t) * (3 + num));
    size_t desc_avail_padded = (desc_avail_sz + 4095) & ~4095;
    size_t used_sz = sizeof(uint16_t) * 3 + sizeof(struct vring_used_elem) * num;
    size_t total_sz = desc_avail_padded + ((used_sz + 4095) & ~4095) + 4096;

    uint8_t *raw = (uint8_t *)kmalloc(total_sz, GFP_KERNEL);
    if (!raw) return -1;
    memset(raw, 0, total_sz);

    uintptr_t aligned = ((uintptr_t)raw + 4095) & ~4095ULL;
    vq->raw_mem = raw;
    vq->size = num;
    vq->last_used_idx = 0;
    vq->desc = (struct vring_desc *)aligned;
    vq->avail = (struct vring_avail *)(aligned + (num * sizeof(struct vring_desc)));
    vq->used = (struct vring_used *)(aligned + desc_avail_padded);

    for (uint16_t i = 0; i < num; i++) {
        vq->desc[i].next = (i + 1 < num) ? (i + 1) : 0;
    }
    vq->free_head = 0;
    return 0;
}

static int virtio_net_send(struct netif *nif, struct net_buf *buf)
{
    struct virtio_net_dev *vdev = (struct virtio_net_dev *)nif->priv;
    if (!vdev || !buf) return -1;

    unsigned long flags;
    spin_lock_irqsave(&vdev->tx_lock, flags);

    struct virtio_vring *tx = &vdev->tx_vq;
    uint16_t desc_idx = tx->avail->idx % tx->size;

    /* Prepend 10-byte virtio header zeroed */
    struct virtio_net_hdr *hdr = (struct virtio_net_hdr *)net_buf_push(buf, sizeof(struct virtio_net_hdr));
    if (!hdr) {
        spin_unlock_irqrestore(&vdev->tx_lock, flags);
        net_buf_free(buf);
        return -1;
    }
    memset(hdr, 0, sizeof(*hdr));

    tx->desc[desc_idx].addr = (uint64_t)(uintptr_t)buf->data;
    tx->desc[desc_idx].len = buf->len;
    tx->desc[desc_idx].flags = 0;
    tx->desc[desc_idx].next = 0;

    tx->avail->ring[tx->avail->idx % tx->size] = desc_idx;
    smp_wmb();
    tx->avail->idx++;
    smp_wmb();

    /* Notify host of TX queue 1 */
    v_out16(vdev->io_base, VIRTIO_PCI_QUEUE_NOTIFY, 1);

    nif->tx_packets++;
    nif->tx_bytes += (buf->len - sizeof(struct virtio_net_hdr));

    spin_unlock_irqrestore(&vdev->tx_lock, flags);

    /* Clean completed TX buffers */
    smp_rmb();
    while (tx->last_used_idx != tx->used->idx) {
        tx->last_used_idx++;
    }

    net_buf_free(buf);
    return 0;
}

void virtio_net_poll(void)
{
    struct virtio_net_dev *vdev = &s_virtio_dev;
    if (!vdev->io_base) return;

    /* Read and clear interrupt status */
    uint8_t isr = v_in8(vdev->io_base, VIRTIO_PCI_ISR);
    (void)isr;

    struct virtio_vring *rx = &vdev->rx_vq;
    smp_rmb();

    while (rx->last_used_idx != rx->used->idx) {
        uint16_t uidx = rx->last_used_idx % rx->size;
        struct vring_used_elem *elem = &rx->used->ring[uidx];
        uint32_t id = elem->id;
        uint32_t len = elem->len;

        struct net_buf *buf = rx->bufs[id];
        if (buf && len > sizeof(struct virtio_net_hdr)) {
            buf->data += sizeof(struct virtio_net_hdr);
            buf->len = len - sizeof(struct virtio_net_hdr);
            buf->tot_len = buf->len;
            buf->netif = &s_virtio_netif;

            s_virtio_netif.rx_packets++;
            s_virtio_netif.rx_bytes += buf->len;

            net_queue_dispatch(buf);

            /* Replenish with fresh buffer */
            struct net_buf *fresh = net_buf_alloc();
            if (fresh) {
                rx->bufs[id] = fresh;
                rx->desc[id].addr = (uint64_t)(uintptr_t)fresh->data;
                rx->desc[id].len = NET_BUF_DATA_SIZE - NET_BUF_HEADROOM;
                rx->desc[id].flags = VRING_DESC_F_WRITE;
            } else {
                rx->bufs[id] = NULL;
            }
        }

        rx->avail->ring[rx->avail->idx % rx->size] = (uint16_t)id;
        smp_wmb();
        rx->avail->idx++;
        rx->last_used_idx++;
    }
}

static int virtio_net_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    (void)id;
    pr_info("cherry_net: probing virtio-net pci device at [%02x:%02x.%d]...\n",
            pdev->bus, pdev->dev, pdev->fn);

    pci_enable_device(pdev);
    pci_set_master(pdev);

    uintptr_t io = (uintptr_t)pdev->resource[0];
    if (!io) {
        pr_err("cherry_net: missing bar0 io address\n");
        return -1;
    }

    struct virtio_net_dev *vdev = &s_virtio_dev;
    memset(vdev, 0, sizeof(*vdev));
    vdev->pdev = pdev;
    vdev->io_base = io;
    spin_lock_init(&vdev->tx_lock);

    /* 1. Reset device */
    v_out8(io, VIRTIO_PCI_STATUS, 0);

    /* 2. Set ACKNOWLEDGE and DRIVER */
    v_out8(io, VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    /* 3. Read & negotiate features */
    uint32_t features = v_in32(io, VIRTIO_PCI_HOST_FEATURES);
    uint32_t guest_features = features & (VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS);
    v_out32(io, VIRTIO_PCI_GUEST_FEATURES, guest_features);

    /* 4. Read MAC address from device config */
    for (int i = 0; i < ETH_ALEN; i++) {
        vdev->mac[i] = v_in8(io, VIRTIO_PCI_CONFIG_OFF + i);
    }
    pr_info("cherry_net: virtio-net hwaddr %02x:%02x:%02x:%02x:%02x:%02x\n",
            vdev->mac[0], vdev->mac[1], vdev->mac[2],
            vdev->mac[3], vdev->mac[4], vdev->mac[5]);

    /* 5. Setup RX Virtqueue (queue 0) */
    v_out16(io, VIRTIO_PCI_QUEUE_SEL, 0);
    uint16_t rx_size = v_in16(io, VIRTIO_PCI_QUEUE_NUM);
    if (rx_size > VIRTIO_NET_QUEUE_SIZE) rx_size = VIRTIO_NET_QUEUE_SIZE;
    if (vring_setup(&vdev->rx_vq, rx_size) != 0) {
        pr_err("cherry_net: rx vring setup failed\n");
        return -1;
    }
    /* Pre-fill RX descriptors with packet buffers */
    for (uint16_t i = 0; i < rx_size; i++) {
        struct net_buf *b = net_buf_alloc();
        vdev->rx_vq.bufs[i] = b;
        vdev->rx_vq.desc[i].addr = (uint64_t)(uintptr_t)b->data;
        vdev->rx_vq.desc[i].len = NET_BUF_DATA_SIZE - NET_BUF_HEADROOM;
        vdev->rx_vq.desc[i].flags = VRING_DESC_F_WRITE;
        vdev->rx_vq.avail->ring[i] = i;
    }
    vdev->rx_vq.avail->idx = rx_size;
    uintptr_t rx_phys = (uintptr_t)vdev->rx_vq.desc;
    v_out32(io, VIRTIO_PCI_QUEUE_PFN, (uint32_t)(rx_phys >> 12));

    /* 6. Setup TX Virtqueue (queue 1) */
    v_out16(io, VIRTIO_PCI_QUEUE_SEL, 1);
    uint16_t tx_size = v_in16(io, VIRTIO_PCI_QUEUE_NUM);
    if (tx_size > VIRTIO_NET_QUEUE_SIZE) tx_size = VIRTIO_NET_QUEUE_SIZE;
    if (vring_setup(&vdev->tx_vq, tx_size) != 0) {
        pr_err("cherry_net: tx vring setup failed\n");
        return -1;
    }
    uintptr_t tx_phys = (uintptr_t)vdev->tx_vq.desc;
    v_out32(io, VIRTIO_PCI_QUEUE_PFN, (uint32_t)(tx_phys >> 12));

    /* 7. Assign IRQ & set DRIVER_OK */
    pci_assign_irq(pdev);
    v_out8(io, VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);

    /* 8. Register Network Interface eth0 */
    memset(&s_virtio_netif, 0, sizeof(s_virtio_netif));
    memcpy(s_virtio_netif.hwaddr, vdev->mac, ETH_ALEN);
    s_virtio_netif.send = virtio_net_send;
    s_virtio_netif.priv = vdev;
    s_virtio_netif.flags = NETIF_FLAG_UP | NETIF_FLAG_BROADCAST | NETIF_FLAG_ETH;

    /* IPv4: 10.0.2.15/24, GW: 10.0.2.2 (Standard QEMU user networking) */
    s_virtio_netif.ip4_addr.addr = IP4_ADDR(10, 0, 2, 15);
    s_virtio_netif.ip4_netmask.addr = IP4_ADDR(255, 255, 255, 0);
    s_virtio_netif.ip4_gw.addr = IP4_ADDR(10, 0, 2, 2);

    /* IPv6 Link-Local */
    ip6_addr_create_linklocal(&s_virtio_netif.ip6_ll, vdev->mac);

    netif_register(&s_virtio_netif, "eth0", 1500);
    netif_set_up(&s_virtio_netif);
    netif_set_default(&s_virtio_netif);

    pci_set_drvdata(pdev, vdev);
    pr_info("cherry_net: virtio-net eth0 registered successfully (irq=%d)\n", pdev->irq);
    return 0;
}

static void virtio_net_remove(struct pci_dev *pdev)
{
    (void)pdev;
}

static const struct pci_device_id s_virtio_net_ids[] = {
    { VIRTIO_PCI_VENDOR_ID, VIRTIO_PCI_DEVICE_NET_LEGACY, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { VIRTIO_PCI_VENDOR_ID, VIRTIO_PCI_DEVICE_NET_MODERN, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 }
};

static struct pci_driver s_virtio_net_driver = {
    .name = "virtio-net",
    .id_table = s_virtio_net_ids,
    .probe = virtio_net_probe,
    .remove = virtio_net_remove,
};

int virtio_net_init(void)
{
    return pci_register_driver(&s_virtio_net_driver);
}

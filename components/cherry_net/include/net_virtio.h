/*
 * CherryNet - VirtIO-Net PCI Network Interface Driver
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#ifndef _CHERRY_NET_VIRTIO_H_
#define _CHERRY_NET_VIRTIO_H_

#include "net_types.h"
#include "net_buf.h"
#include "net_if.h"
#include "cherry_pci.h"

#ifdef __cplusplus
extern "C" {
#endif

/* VirtIO PCI Vendor & Device IDs */
#define VIRTIO_PCI_VENDOR_ID          0x1AF4
#define VIRTIO_PCI_DEVICE_NET_LEGACY  0x1000
#define VIRTIO_PCI_DEVICE_NET_MODERN  0x1041

/* VirtIO Legacy PCI Registers (BAR0) */
#define VIRTIO_PCI_HOST_FEATURES      0x00U /* 32-bit R */
#define VIRTIO_PCI_GUEST_FEATURES     0x04U /* 32-bit W */
#define VIRTIO_PCI_QUEUE_PFN          0x08U /* 32-bit RW (pfn = phys >> 12) */
#define VIRTIO_PCI_QUEUE_NUM          0x0CU /* 16-bit R */
#define VIRTIO_PCI_QUEUE_SEL          0x0EU /* 16-bit W */
#define VIRTIO_PCI_QUEUE_NOTIFY       0x10U /* 16-bit W */
#define VIRTIO_PCI_STATUS             0x12U /* 8-bit RW */
#define VIRTIO_PCI_ISR                0x13U /* 8-bit R */
#define VIRTIO_PCI_CONFIG_OFF         0x14U /* Start of Device Specific Config */

/* Device Status Flags */
#define VIRTIO_STATUS_ACKNOWLEDGE     1U
#define VIRTIO_STATUS_DRIVER          2U
#define VIRTIO_STATUS_DRIVER_OK       4U
#define VIRTIO_STATUS_FAILED          128U

/* VirtIO Net Feature Bits */
#define VIRTIO_NET_F_CSUM             (1U << 0)
#define VIRTIO_NET_F_MAC              (1U << 5)
#define VIRTIO_NET_F_STATUS           (1U << 16)
#define VIRTIO_NET_F_MQ               (1U << 22)

/* VirtIO Ring Flags */
#define VRING_DESC_F_NEXT             1U
#define VRING_DESC_F_WRITE            2U

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

/* Standard VirtIO Network Packet Header (10 bytes) */
struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed));

/* Device Config in PCI space */
struct virtio_net_config {
    uint8_t  mac[ETH_ALEN];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
} __attribute__((packed));

int  virtio_net_init(void);
void virtio_net_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_NET_VIRTIO_H_ */

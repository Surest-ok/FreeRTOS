/*
 * CherryPCI - ARM GICv3 ITS (Interrupt Translation Service) Engine
 *
 * Provides MSI / MSI-X translation, Command Queue handling,
 * and LPI (Locality-specific Peripheral Interrupt) management.
 */

#ifndef CHERRY_ITS_H
#define CHERRY_ITS_H

#include <stdint.h>
#include <stdbool.h>
#include "app_irq.h"

/* GICv3 ITS Register Offsets */
#define GITS_CTLR               0x0000
#define GITS_IIDR               0x0004
#define GITS_TYPER              0x0008
#define GITS_CBASER             0x0080
#define GITS_CWRITER            0x0088
#define GITS_CREADR             0x0090
#define GITS_BASER(n)           (0x0100 + (n) * 8)
#ifndef GITS_TRANSLATER
#define GITS_TRANSLATER         0x08090040ULL
#endif
#define GITS_TRANSLATER_OFFSET  0x10040

/* GITS_CTLR Bits */
#define GITS_CTLR_ENABLED       (1U << 0)
#define GITS_CTLR_QUIESCENT     (1U << 31)

/* GITS_TYPER Bits */
#define GITS_TYPER_PHYSICAL     (1ULL << 0)
#define GITS_TYPER_PTA          (1ULL << 19)

/* GITS_CBASER Bits */
#define GITS_CBASER_VALID       (1ULL << 63)
#define GITS_CBASER_INNER_SHARE (1ULL << 10)
#define GITS_CBASER_WA_WB       (5ULL << 59)

/* GITS_BASER Bits */
#define GITS_BASER_VALID        (1ULL << 63)
#define GITS_BASER_INDIRECT     (1ULL << 62)
#define GITS_BASER_TYPE_DEVICE  (1ULL << 56)
#define GITS_BASER_TYPE_COLL    (4ULL << 56)
#define GITS_BASER_INNER_SHARE  (1ULL << 10)
#define GITS_BASER_PAGE_SIZE_64K (2ULL << 8)
#define GITS_BASER_PAGE_SIZE_4K  (0ULL << 8)

/* GIC Redistributor LPI registers */
#define GICR_CTLR_ENABLE_LPIS   (1U << 0)
#define GICR_PROPBASER_REG      0x0070
#define GICR_PENDBASER_REG      0x0078
#define GICR_PENDBASER_PTZ      (1ULL << 62)

/* ITS Commands (4 x 64-bit words = 32 bytes) */
#define GITS_CMD_MAPD           0x08
#define GITS_CMD_MAPC           0x09
#define GITS_CMD_MAPTI          0x0A
#define GITS_CMD_MAPI           0x0B
#define GITS_CMD_SYNC           0x05
#define GITS_CMD_INV            0x0C
#define GITS_CMD_INVALL         0x0D

/* LPI Configuration constants */
#define GIC_LPI_BASE            8192U
#define GIC_LPI_COUNT           256U

/* Forward declarations */
struct pci_dev;

/* Public API */
int  its_init(void);
bool its_is_available(void);
int  its_alloc_msi(struct pci_dev *dev, isr_fn_t handler, void *arg, uint32_t *out_lpi, uint32_t *out_event_id);
void its_dump_info(void);

#endif /* CHERRY_ITS_H */

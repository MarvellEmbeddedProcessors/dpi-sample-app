/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _DPI_VF_H_
#define _DPI_VF_H_

/**
 * A structure describing the location of a PCI device.
 */
struct plt_pci_device {
	uint32_t domain;	/* Device domain */
	uint8_t bus;		/* Device bus */
	uint8_t devid;		/* Device ID */
	uint8_t function;	/* Device function. */
};

struct dpi_vf {
	struct plt_pci_device pci_dev;
	const char *pci_addr;
	uint8_t *rbase;
	uint16_t vfid;
};

#ifndef BIT_ULL
#define BIT_ULL(nr) (1ULL << (nr))
#endif

/* DPI VF register offsets from VF_BAR0 */
#define DPI_VDMA_EN	   (0x0)
#define DPI_VDMA_REQQ_CTL  (0x8)
#define DPI_VDMA_DBELL	   (0x10)
#define DPI_VDMA_SADDR	   (0x18)
#define DPI_VDMA_COUNTS	   (0x20)
#define DPI_VDMA_NADDR	   (0x28)
#define DPI_VDMA_IWBUSY	   (0x30)
#define DPI_VDMA_CNT	   (0x38)
#define DPI_VF_INT	   (0x100)
#define DPI_VF_INT_W1S	   (0x108)
#define DPI_VF_INT_ENA_W1C (0x110)
#define DPI_VF_INT_ENA_W1S (0x118)
#define DPI_MBOX_VF_PF_DATA0 (0x2000)
#define DPI_MBOX_VF_PF_DATA1 (0x2008)

#define DPI_QUEUE_OPEN	0x1
#define DPI_QUEUE_CLOSE 0x2
#define DPI_REG_DUMP	0x3
#define DPI_GET_REG_CFG 0x4
#define DPI_QUEUE_OPEN_V2 0x5

enum dpi_mbox_word_type {
	DPI_MBOX_TYPE_CMD,
	DPI_MBOX_TYPE_RSP_ACK,
	DPI_MBOX_TYPE_RSP_NACK,
};

enum dpi_mbox_cmd_status {
	DPI_MBOX_CMD_STATUS_NOT_SETUP = 1,
	DPI_MBOX_CMD_STATUS_TIMEDOUT = 2,
	DPI_MBOX_CMD_STATUS_NACK = 3,
	DPI_MBOX_CMD_STATUS_BUSY = 4
};

typedef union dpi_mbox_msg_t {
	uint64_t u[2];
	struct dpi_mbox_message_s {
		/* VF ID to configure */
		uint64_t vfid : 8;
		/* Command code */
		uint64_t cmd : 4;
		/* Command buffer size in 8-byte words */
		uint64_t csize : 16;
		/* aura of the command buffer */
		uint64_t aura : 20;
		/* SSO PF function */
		uint64_t sso_pf_func : 16;
		/* NPA PF function */
		uint64_t npa_pf_func : 16;
		/* WQE queue DMA completion status enable */
		uint64_t wqecs : 1;
		/* WQE queue DMA completion status offset */
		uint64_t wqecsoff : 7;
		/* Reserved for future use */
		uint64_t rsvd : 40;
	} s;
} dpi_mbox_msg_t;

int dpi_vf_configure(struct dpi_vf *dpi, uint32_t chunk_sz, uint64_t aura);
int dpi_vf_configure_v2(struct dpi_vf *dpi_vf, uint32_t chunk_sz, uint64_t aura);
void dpi_vf_dev_init(struct dpi_vf *dpi_vf);
int dpi_vf_dev_fini(struct dpi_vf *dpi_vf);
void dpi_vf_enable(struct dpi_vf *dpi);
void dpi_vf_disable(struct dpi_vf *dpi);

static inline void plt_write64(uint64_t val, volatile void *addr)
{
	*((volatile uint64_t *)addr) = val;
}

static inline uint64_t plt_read64(volatile void *addr)
{
	return *((volatile uint64_t *)addr);
}
#endif

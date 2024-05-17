/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <sys/types.h>
#include <linux/limits.h>
#include <linux/vfio.h>
#include <sys/stat.h>

#include "log.h"
#include "dpi_vf.h"

#define DPI_VF_MBOX_TIMEOUT_MS 1000

static inline int
send_mbox_msg(struct dpi_vf *dpi_vf, dpi_mbox_msg_t *cmd, dpi_mbox_msg_t *rsp)
{
	volatile uint64_t reg_val;
	int count;

	plt_write64(cmd->u[0], dpi_vf->rbase + DPI_MBOX_VF_PF_DATA1);
	plt_write64(cmd->u[1], dpi_vf->rbase + DPI_MBOX_VF_PF_DATA0);

	/* No response or no ack for the mailbox message */
	if (!rsp)
		return 0;

	for (count = 0; count < DPI_VF_MBOX_TIMEOUT_MS; count++) {
		usleep(1000);
		reg_val = plt_read64(dpi_vf->rbase + DPI_MBOX_VF_PF_DATA0);
		if (reg_val != cmd->u[1]) {
			rsp->u[0] = reg_val;
			break;
		}
	}
	if (count == DPI_VF_MBOX_TIMEOUT_MS) {
		error("DPI mbox cmd timedout\n");
		return DPI_MBOX_CMD_STATUS_TIMEDOUT;
	}
	if (rsp->u[0] != DPI_MBOX_TYPE_RSP_ACK) {
		error("DPI mbox received NACK from PF\n");
		return DPI_MBOX_CMD_STATUS_NACK;
	}

	return 0;
}

void dpi_vf_enable(struct dpi_vf *dpi)
{
	plt_write64(0x1, dpi->rbase + DPI_VDMA_EN);
}

void dpi_vf_disable(struct dpi_vf *dpi)
{
	plt_write64(0x0, dpi->rbase + DPI_VDMA_EN);
}

int dpi_vf_configure(struct dpi_vf *dpi_vf, uint32_t chunk_sz, uint64_t aura)
{
	dpi_mbox_msg_t mbox_msg, rsp;
	uint64_t reg;
	int rc;

	dpi_vf_disable(dpi_vf);
	reg = plt_read64(dpi_vf->rbase + DPI_VDMA_SADDR);
	while (!(reg & BIT_ULL(63)))
		reg = plt_read64(dpi_vf->rbase + DPI_VDMA_SADDR);

	plt_write64(0x0, dpi_vf->rbase + DPI_VDMA_REQQ_CTL);
	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	mbox_msg.s.vfid = dpi_vf->vfid;
	mbox_msg.s.cmd = DPI_QUEUE_OPEN;
	mbox_msg.s.csize = chunk_sz;
	mbox_msg.s.aura = aura;
	mbox_msg.s.sso_pf_func = 0; /* get sso pffunc from sso driver */
	mbox_msg.s.npa_pf_func = 0; /* get npa pffunc from npa driver */
	mbox_msg.s.wqecsoff = 0; /* get wqe offset from sso driver */
	mbox_msg.s.wqecs = 0;

	rc = send_mbox_msg(dpi_vf, &mbox_msg, &rsp);
	if (rc < 0)
		error("Failed to send mbox message %d to DPI PF, err %d", mbox_msg.s.cmd, rc);

	return rc;
}

int dpi_vf_configure_v2(struct dpi_vf *dpi_vf, uint32_t chunk_sz, uint64_t aura)
{
	dpi_mbox_msg_t mbox_msg, rsp;
	uint64_t reg;
	int rc;

	dpi_vf_disable(dpi_vf);
	reg = plt_read64(dpi_vf->rbase + DPI_VDMA_SADDR);
	while (!(reg & BIT_ULL(63)))
		reg = plt_read64(dpi_vf->rbase + DPI_VDMA_SADDR);

	plt_write64(0x0, dpi_vf->rbase + DPI_VDMA_REQQ_CTL);
	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	mbox_msg.s.vfid = dpi_vf->vfid;
	mbox_msg.s.cmd = DPI_QUEUE_OPEN_V2;
	mbox_msg.s.csize = chunk_sz / 8;
	mbox_msg.s.aura = aura;
	mbox_msg.s.sso_pf_func = 0; /* get sso pffunc from sso driver */
	mbox_msg.s.npa_pf_func = 0; /* get npa pffunc from npa driver */
	mbox_msg.s.wqecsoff = 0; /* get wqe offset from sso driver */
	mbox_msg.s.wqecs = 0;

	rc = send_mbox_msg(dpi_vf, &mbox_msg, &rsp);
	if (rc)
		error("Failed to send mbox message %d to DPI PF, err %d", mbox_msg.s.cmd, rc);

	return rc;
}

void dpi_vf_dev_init(struct dpi_vf *dpi_vf)
{
	struct plt_pci_device *pci_dev = &dpi_vf->pci_dev;
	uint16_t vfid;

	pci_dev->devid = atoi(&dpi_vf->pci_addr[9]);
	pci_dev->function = atoi(&dpi_vf->pci_addr[11]);
	vfid = ((pci_dev->devid & 0x1F) << 3) | (pci_dev->function & 0x7);
	vfid -= 1;
	dpi_vf->vfid = vfid;
}

int dpi_vf_dev_fini(struct dpi_vf *dpi_vf)
{
	dpi_mbox_msg_t mbox_msg, rsp;
	uint64_t reg;
	int rc;

	/* Wait for SADDR to become idle */
	reg = plt_read64(dpi_vf->rbase + DPI_VDMA_SADDR);
	while (!(reg & BIT_ULL(63)))
		reg = plt_read64(dpi_vf->rbase + DPI_VDMA_SADDR);

	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	mbox_msg.s.vfid = dpi_vf->vfid;
	mbox_msg.s.cmd = DPI_QUEUE_CLOSE;

	rc = send_mbox_msg(dpi_vf, &mbox_msg, &rsp);
	if (rc)
		error("Failed to send mbox message %d to DPI PF, err %d", mbox_msg.s.cmd, rc);

	return rc;
}

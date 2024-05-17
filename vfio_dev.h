/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef VFIO_DEV_H
#define VFIO_DEV_H

#include <stdint.h>

/* Device VFIO memory resource */
struct vfio_mem_resource {
	uint8_t *addr;	/* Mapped virtual address */
	uint64_t len;	/* Length of resource */
};

/* initializes the IOMMU for the device.
 * Returns the devices file descriptor or -1 on error.
 */
int vfio_dev_init(const char *pci_addr);

/* Close the device file descriptors & unmap memory resource */
void vfio_dev_fini(int vfio_fd, struct vfio_mem_resource *res);

/* Returns a pointer to mapped VFIO memory resource or MAP_FAILED if failed.
 * region_index is to be taken from linux/vfio.h
 */
struct vfio_mem_resource *vfio_dev_map_region(int vfio_fd, int region_index);

#endif

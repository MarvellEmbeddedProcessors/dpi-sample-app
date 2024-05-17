/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <errno.h>
#include <libgen.h>

#include <linux/limits.h>
#include <linux/vfio.h>
#include <sys/mman.h>

#include "log.h"
#include "vfio_dev.h"

static int vfio_cfd = -1;
static int vfio_gfd = -1;

/* returns the devices file descriptor or -ve return code on error */
int vfio_dev_init(const char *pci_addr)
{
	char path[PATH_MAX], iommu_group_path[PATH_MAX];
	int ret, len, groupid, vfio_fd;
	char *group_name;
	struct stat st;

	/* find iommu group for the device
	 * `readlink /sys/bus/pci/device/<segn:busn:devn.funcn>/iommu_group`
	 */
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/", pci_addr);
	ret = stat(path, &st);
	if (ret < 0) {
		error("No such device at %s", path);
		return -1;
	}

	strncat(path, "iommu_group", sizeof(path) - strlen(path) - 1);

	len = readlink(path, iommu_group_path, sizeof(iommu_group_path));
	if (len < 0) {
		error("Error in iommu_group_path");
		return len;
	}

	/* append 0x00 to the string to end it */
	iommu_group_path[len] = '\0';

	group_name = basename(iommu_group_path);

	/* convert group id to int */
	sscanf(group_name, "%d", &groupid);

	/* open vfio file to create new vfio container */
	vfio_cfd = open("/dev/vfio/vfio", O_RDWR);
	if (vfio_cfd < 0) {
		error("Failed to open /dev/vfio/vfio");
		return vfio_cfd;
	}

	/* open VFIO group containing the device */
	snprintf(path, sizeof(path), "/dev/vfio/%d", groupid);
	vfio_gfd = open(path, O_RDWR);
	if (vfio_gfd < 0) {
		error("Failed to open vfio group");
		return vfio_gfd;
	}

	/* Add group to container */
	ret = ioctl(vfio_gfd, VFIO_GROUP_SET_CONTAINER, &vfio_cfd);
	if (ret < 0) {
		error("Failed to set container");
		return -1;
	}

	ret = ioctl(vfio_cfd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if (ret < 0) {
		error("Failed to set IOMMU type");
		return ret;
	}

	/* get device file descriptor */
	vfio_fd = ioctl(vfio_gfd, VFIO_GROUP_GET_DEVICE_FD, pci_addr);
	if (vfio_fd < 0)
		error("Failed to get device fd");

	return vfio_fd;
}

void vfio_dev_fini(int vfio_fd, struct vfio_mem_resource *res)
{
	munmap(res->addr, res->len);
	free(res);

	if (vfio_fd > 0)
		close(vfio_fd);

	if (vfio_gfd > 0)
		close(vfio_gfd);

	if (vfio_cfd > 0)
		close(vfio_cfd);
}

/* returns a pointer to the MMAPED BAR region or MAP_FAILED if failed */
struct vfio_mem_resource *vfio_dev_map_region(int vfio_fd, int region_index)
{
	struct vfio_region_info region_info = {.argsz = sizeof(region_info)};
	struct vfio_mem_resource *res;
	int ret;

	region_info.index = region_index;

	ret = ioctl(vfio_fd, VFIO_DEVICE_GET_REGION_INFO, &region_info);
	if (ret == -1) {
		error("Failed to get region info");
		return MAP_FAILED;
	}

	res = malloc(sizeof(struct vfio_mem_resource));
	if (res == NULL)
		return MAP_FAILED;

	res->addr = mmap(NULL, region_info.size, PROT_READ | PROT_WRITE, MAP_SHARED, vfio_fd,
		   region_info.offset);
	if (res->addr == MAP_FAILED) {
		free(res);
		error("Failed to map region");
		return MAP_FAILED;
	}

	res->len = region_info.size;

	return res;
}

